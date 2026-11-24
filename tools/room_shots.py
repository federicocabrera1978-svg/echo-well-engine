#!/usr/bin/env python3
"""Capturas headless de cada sala de campaña (slice, level2, level3, world) + chequeo de los fondos.

Uso: tools/room_shots.py BUILD_DIR OUT_DIR [--png]
Corre echo_engine con --start en cada sala y --dump a OUT_DIR/<nombre>.ppm (320×180).
Para comparar, compila una copia de cada mapa con todos los fondos en `vacio` (el fondo
liso de antes) y saca la misma captura (OUT_DIR/<nombre>_vacio.ppm).
Necesita un display (en CI: xvfb-run, LIBGL_ALWAYS_SOFTWARE=1). Falla si:
  - algún frame no es un PPM 320×180 válido,
  - el fondo de una sala no suma detalle respecto de `vacio` (bordes en la mitad de arriba),
  - dos salas de un mismo mapa quedaron con el mismo fondo (diferencia media por píxel casi nula).
--png además guarda cada captura en .png (×3, NEAREST) si hay Pillow.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# (nombre_mapa, [(label, x, y), ...]): puntos seguros de cada sala de campaña.
LEVELS = [
    ("slice", [
        ("sala0", 32, 152),
        ("sala1", 536, 56),
        ("sala2", 720, 56),
        ("sala3", 1152, 56),
        ("sala4", 1440, 56),
        ("sala5", 1620, 56),
        ("sala6", 1936, 152),
        ("sala7", 2264, 152),
    ]),
    ("level2", [
        ("level2_c00", 32, 152),
        ("level2_c10", 352, 152),
        ("level2_c20", 712, 152),
        ("level2_c30", 984, 152),
        ("level2_c01", 80, 336),
        ("level2_c11", 368, 336),
        ("level2_c21", 664, 272),
        ("level2_c31", 1048, 336),
    ]),
    ("level3", [
        ("level3_c00", 32, 152),
        ("level3_c10", 344, 152),
        ("level3_c20", 664, 152),
        ("level3_c01", 184, 336),
        ("level3_c11", 520, 336),
        ("level3_c21", 800, 336),
    ]),
    ("world", [
        ("world_c00", 32, 152),
        ("world_c10", 536, 56),
        ("world_c20", 720, 56),
        ("world_c30", 1152, 56),
        ("world_c40", 1440, 56),
        ("world_c50", 1620, 56),
        ("world_c60", 1964, 152),
        ("world_c70", 2264, 152),
        ("world_c01", 300, 336),
        ("world_c11", 560, 336),
        ("world_c21", 928, 336),
        ("world_c31", 1104, 336),
        ("world_c41", 1560, 336),
        ("world_c51", 1848, 336),
        ("world_c61", 2192, 336),
        ("world_c71", 2416, 336),
        ("world_c02", 264, 520),
        ("world_c12", 464, 520),
        ("world_c22", 760, 520),
        ("world_c32", 1120, 520),
        ("world_c42", 1472, 520),
        ("world_c52", 1888, 456),
        ("world_c62", 2176, 520),
        ("world_c72", 2480, 520),
        ("world_c03", 48, 704),
        ("world_c13", 520, 704),
        ("world_c23", 740, 704),
        ("world_c33", 1216, 704),
        ("world_c43", 1344, 704),
        ("world_c53", 1728, 704),
        ("world_c63", 2080, 704),
        ("world_c73", 2400, 704),
        ("world_c04", 264, 888),
        ("world_c14", 592, 888),
        ("world_c24", 800, 888),
        ("world_c34", 1120, 888),
        ("world_c44", 1440, 888),
        ("world_c54", 1760, 888),
        ("world_c64", 2080, 888),
        ("world_c74", 2400, 888),
    ]),
]
HDR = b"P6\n320 180\n255\n"
W = 320


def top_half(ppm: Path) -> bytes:
    b = ppm.read_bytes()
    assert b.startswith(HDR) and len(b) == len(HDR) + W * 180 * 3, f"{ppm}: PPM inválido"
    return b[len(HDR) + 16 * W * 3: len(HDR) + 90 * W * 3]  # filas 16..89, sin el HUD


def edge_ratio(px: bytes) -> float:
    """Fracción de píxeles con un salto de color marcado respecto del vecino derecho."""
    e = n = 0
    for y in range(len(px) // (W * 3)):
        row = px[y * W * 3:(y + 1) * W * 3]
        for x in range(W - 1):
            i = x * 3
            if abs(row[i] - row[i + 3]) + abs(row[i + 1] - row[i + 4]) + abs(row[i + 2] - row[i + 5]) > 6:
                e += 1
            n += 1
    return e / n


def mad(a: bytes, b: bytes) -> float:
    return sum(abs(x - y) for x, y in zip(a, b)) / len(a)


def shoot(exe: Path, x: int, y: int, ppm: Path, extra=()):
    subprocess.run([str(exe), "--frames", "20", "--start", str(x), str(y), "--dump", str(ppm),
                    "--nosound", "--nogamepad", "--nosave", *extra], check=True, stdout=subprocess.DEVNULL)


def replace_backdrop_themes(src: str) -> str:
    """Sustituye theme: <tema> por theme: vacio solo dentro de la sección backdrops:."""
    lines = []
    in_backdrops = False
    for line in src.splitlines(keepends=True):
        if line and not line[0].isspace() and not line.startswith("#") and ":" in line:
            in_backdrops = line.startswith("backdrops:")
        if in_backdrops:
            line = re.sub(r"theme:\s*\w+", "theme: vacio", line)
        lines.append(line)
    return "".join(lines)


def resolve_real_map(build: Path, out: Path, ewmapc: Path, name: str, src_yaml: Path) -> Path:
    for cand in (build / "maps" / f"{name}.ewmap", build.parent / "maps" / f"{name}.ewmap"):
        if cand.is_file():
            return cand
    real_map = out / f"{name}.ewmap"
    subprocess.run([str(ewmapc), "--crc", str(src_yaml), "-o", str(real_map)],
                   check=True, stdout=subprocess.DEVNULL)
    return real_map


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    build, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    win = sys.platform == "win32"
    exe = build / ("echo_engine.exe" if win else "echo_engine")
    if not exe.is_file():
        exe = build / "Release" / ("echo_engine.exe" if win else "echo_engine")
    ewmapc = build / ("ewmapc.exe" if win else "ewmapc")
    if not ewmapc.is_file():
        ewmapc = build / "Release" / ("ewmapc.exe" if win else "ewmapc")

    shots = []
    for map_name, rooms in LEVELS:
        src_yaml = ROOT / "assets/maps/src" / f"{map_name}.yaml"
        src = src_yaml.read_text(encoding="utf-8")
        real_map = resolve_real_map(build, out, ewmapc, map_name, src_yaml)
        flat_yaml = out / f"{map_name}_vacio.yaml"
        flat_yaml.write_text(replace_backdrop_themes(src), encoding="utf-8")
        flat_map = out / f"{map_name}_vacio.ewmap"
        subprocess.run([str(ewmapc), "--crc", str(flat_yaml), "-o", str(flat_map)],
                       check=True, stdout=subprocess.DEVNULL)

        tops = []
        for label, x, y in rooms:
            ppm, flat = out / f"{label}.ppm", out / f"{label}_vacio.ppm"
            shoot(exe, x, y, ppm, ("--map", str(real_map)))
            shoot(exe, x, y, flat, ("--map", str(flat_map)))
            t, f = top_half(ppm), top_half(flat)
            er, ef, d = edge_ratio(t), edge_ratio(f), mad(t, f)
            print(f"{label}: bordes {er:.3f} (vacio {ef:.3f}), dif media vs vacio {d:.2f}")
            assert er >= ef + 0.03, f"{label}: el fondo no suma detalle ({er:.3f} vs {ef:.3f})"
            assert d > 2.0, f"{label}: igual que el fondo liso (dif {d:.2f})"
            tops.append((label, t))
            shots.append(label)
        for i in range(len(tops)):
            for j in range(i + 1, len(tops)):
                d = mad(tops[i][1], tops[j][1])
                assert d > 2.0, f"{tops[i][0]} y {tops[j][0]} tienen el mismo fondo (dif {d:.2f})"

    if "--png" in sys.argv:
        try:
            from PIL import Image
            for label in shots:
                for suf in ("", "_vacio"):
                    im = Image.open(out / f"{label}{suf}.ppm")
                    im.resize((960, 540), Image.NEAREST).save(out / f"{label}{suf}.png")
        except ImportError:
            print("sin Pillow: solo PPM")
    print("room_shots OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
