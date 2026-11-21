#!/usr/bin/env python3
"""Capturas headless de cada sala de slice (una por chunk) + chequeo de los fondos.

Uso: tools/room_shots.py BUILD_DIR OUT_DIR [--png]
Corre echo_engine con --start en cada sala y --dump a OUT_DIR/salaN.ppm (320×180).
Para comparar, compila una copia de slice.yaml con todos los fondos en `vacio` (el fondo
liso de antes) y saca la misma captura (OUT_DIR/salaN_vacio.ppm).
Necesita un display (en CI: xvfb-run, LIBGL_ALWAYS_SOFTWARE=1). Falla si:
  - algún frame no es un PPM 320×180 válido,
  - el fondo de una sala no suma detalle respecto de `vacio` (bordes en la mitad de arriba),
  - dos salas quedaron con el mismo fondo (diferencia media por píxel casi nula).
--png además guarda salaN.png (×3, NEAREST) si hay Pillow.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# (chunk, x, y): un punto seguro de cada sala de assets/maps/src/slice.yaml
ROOMS = [(0, 32, 152), (1, 536, 56), (2, 720, 56), (3, 1152, 56),
         (4, 1440, 56), (5, 1620, 56), (6, 1936, 152), (7, 2264, 152)]
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


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    build, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    win = sys.platform == "win32"
    exe = build / ("echo_engine.exe" if win else "echo_engine")
    ewmapc = build / ("ewmapc.exe" if win else "ewmapc")
    src = (ROOT / "assets/maps/src/slice.yaml").read_text(encoding="utf-8")
    flat_yaml = out / "slice_vacio.yaml"
    flat_yaml.write_text(re.sub(r"theme:\s*\w+", "theme: vacio", src), encoding="utf-8")
    flat_map = out / "slice_vacio.ewmap"
    subprocess.run([str(ewmapc), "--crc", str(flat_yaml), "-o", str(flat_map)], check=True, stdout=subprocess.DEVNULL)

    tops = []
    for cx, x, y in ROOMS:
        ppm, flat = out / f"sala{cx}.ppm", out / f"sala{cx}_vacio.ppm"
        shoot(exe, x, y, ppm)
        shoot(exe, x, y, flat, ("--map", str(flat_map)))
        t, f = top_half(ppm), top_half(flat)
        er, ef, d = edge_ratio(t), edge_ratio(f), mad(t, f)
        print(f"sala{cx}: bordes {er:.3f} (vacio {ef:.3f}), dif media vs vacio {d:.2f}")
        assert er >= ef + 0.03, f"sala{cx}: el fondo no suma detalle ({er:.3f} vs {ef:.3f})"
        assert d > 2.0, f"sala{cx}: igual que el fondo liso (dif {d:.2f})"
        tops.append(t)
    for i in range(len(tops)):
        for j in range(i + 1, len(tops)):
            d = mad(tops[i], tops[j])
            assert d > 2.0, f"sala{i} y sala{j} tienen el mismo fondo (dif {d:.2f})"
    if "--png" in sys.argv:
        try:
            from PIL import Image
            for cx, _, _ in ROOMS:
                for suf in ("", "_vacio"):
                    im = Image.open(out / f"sala{cx}{suf}.ppm")
                    im.resize((960, 540), Image.NEAREST).save(out / f"sala{cx}{suf}.png")
        except ImportError:
            print("sin Pillow: solo PPM")
    print("room_shots OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
