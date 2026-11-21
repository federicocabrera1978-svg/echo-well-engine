#!/usr/bin/env python3
"""Genera el atlas PNG default 128×128 (16 familias × 16 máscaras de 4 vecinos).

Uso:
  python3 tools/gen_atlas.py -o assets/atlas/tiles.png
  python3 tools/gen_atlas.py --check assets/atlas/tiles.png   # pixels == generador
  python3 tools/gen_atlas.py --ppm atlas.ppm                  # vista RGB

Layout (igual que el procedural y ewmapc):
  fila = familia (0..15), columna = máscara 4 vecinos (N=1, E=2, S=4, W=8).
  cada tile 8×8. RGB = albedo, A = altura (0..255) para el bisel del shader.

Bisel solo en lados *expuestos* (sin vecino): N/W más claros, E/S más oscuros y A baja.
El interior de una familia es idéntico en las 16 máscaras (el cap de pasto solo
toca y=0..1 cuando N está expuesto, no el pixel (3,3) de los tests).

Sin dependencias: PNG vía zlib de la stdlib, filtro 0, solo IHDR/IDAT/IEND.
--check compara *pixels* (no bytes del archivo) para no depender de la
versión de zlib.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

TILE = 8
ATLAS_TILES = 16
ATLAS = TILE * ATLAS_TILES  # 128
N, E, S, W = 1, 2, 4, 8

# Paleta y estilo por familia. (R, G, B) en 0..255.
# Slice usa familia 10 (VIS) y 14 (FG) después del autotile.
FAMILIES: list[tuple[str, tuple[int, int, int], str]] = [
    ("slate",    (68, 68, 86),    "ashlar"),   # 0  pizarra dracónica
    ("stone",    (110, 108, 114), "ashlar"),   # 1  mampostería de calabozo
    ("brick",    (144, 66, 56),   "brick"),    # 2  ladrillo de cripta
    ("dirt",     (112, 78, 52),   "speckle"),  # 3  tierra compacta
    ("grass",    (64, 128, 68),   "grass"),    # 4  musgo y pasto de caverna
    ("wood",     (128, 84, 48),   "plank"),    # 5  tablones con remaches
    ("metal",    (124, 132, 148), "metal"),    # 6  hierro forjado
    ("sand",     (178, 152, 98),  "speckle"),  # 7  arenisca / polvo óseo
    ("ice",      (138, 184, 212), "ice"),      # 8  hielo arcano
    ("cave",     (54, 52, 68),    "basalt"),   # 9  basalto abisal
    ("moss",     (72, 118, 76),   "grass"),    # 10 musgo de mazmorra (slice VIS)
    ("copper",   (156, 92, 62),   "metal"),    # 11 bronce antiguo
    ("crystal",  (124, 78, 168),  "crystal"),  # 12 amatista rúnica
    ("snow",     (198, 204, 214), "ashlar"),   # 13 mármol pálido
    ("darkwood", (76, 52, 42),    "plank"),    # 14 viga oscura (slice FG)
    ("gold",     (196, 154, 62),  "hoard"),    # 15 tesoro dracónico
]


def hash32(x: int) -> int:
    """Mismo mezcla que engine/render/tile_atlas.cpp (determinístico)."""
    x &= 0xFFFFFFFF
    x ^= x >> 16
    x = (x * 0x7FEB352D) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x846CA68B) & 0xFFFFFFFF
    x ^= x >> 16
    return x


def clamp8(v: float) -> int:
    if v < 0:
        return 0
    if v > 255:
        return 255
    return int(v + 0.5)


def fill_pixel(fam: int, style: str, base: tuple[int, int, int], x: int, y: int) -> tuple[int, int, int, int]:
    """Color y altura del interior (sin bisel)."""
    r, g, b = base
    h = 248
    n = (hash32(fam * 64 + y * 8 + x) & 0xFF) / 255.0
    if style == "flat":
        k = 0.96 + 0.08 * n
    elif style == "ashlar":
        # Sillares tallados 4x4 desfasados con juntas suaves (sin hundir h < 215 en (7,4),(4,7))
        bx = (x + (4 if y >= 4 else 0)) % 8
        seam = (y == 3) or (bx == 4 and y < 3) or (x == 0 and y >= 4)
        tone = 0.92 + 0.14 * ((hash32(fam * 31 + (y // 4) * 5 + (bx // 4)) & 0xFF) / 255.0)
        k = 0.78 if seam else (tone * (0.95 + 0.10 * n))
        h = 212 if seam else clamp8(226 + 29 * n)
    elif style == "speckle":
        pebble = (hash32(fam * 97 + x * 7 + y * 19) & 15) == 0
        k = 1.14 if pebble else (0.86 + 0.24 * n)
        h = 246 if pebble else clamp8(208 + 44 * n)
    elif style == "basalt":
        # Grietas diagonales en roca volcánica
        fissure = (x + y * 2) % 7 == 0 and n < 0.55
        k = 0.72 if fissure else (0.88 + 0.22 * n)
        h = 195 if fissure else clamp8(220 + 35 * n)
    elif style == "brick":
        # Juntas: filas 3 y 7; ladrillos desfasados (x=3 arriba, x=7 abajo).
        joint = (y == 3 or y == 7) or (x == (3 if y < 3 else 7) and y not in (3, 7))
        if joint:
            k, r, g, b, h = 0.72, 86, 70, 68, 120
        else:
            bvar = 0.90 + 0.18 * ((hash32(fam * 19 + (y // 4) * 11 + (x // 4)) & 0xFF) / 255.0)
            k = bvar * (0.94 + 0.10 * n)
            h = 238
    elif style == "plank" or style == "grain":
        # Vetas horizontales + costura central + remaches en extremos
        seam = (y == 4)
        rivet = (x in (1, 6)) and (y in (2, 6))
        vein = ((y + fam) % 3 == 0) or ((x + 2 * y + fam) % 7 == 0)
        if rivet:
            r, g, b, k, h = 110, 114, 124, 1.05, 250
        elif seam:
            k, h = 0.70, 185
        else:
            k = 1.10 if vein else (0.90 + 0.14 * n)
            h = 224 if vein else 240
    elif style == "metal":
        # Placa con ranura sutil y remaches en las esquinas interiores
        rivet = (x in (1, 6)) and (y in (1, 6))
        band = (x == 4 or y == 4)
        if rivet:
            k, h = 1.24, 255
        elif band:
            k, h = 0.80, 210
        else:
            k = 0.90 + 0.16 * n
            if (x + y) % 4 == 0:
                k *= 1.10
                h = 244
    elif style == "hoard":
        # Lingotes / tesoro dorado con relieve central y brillo
        center = (2 <= x <= 5) and (2 <= y <= 5)
        k = (1.16 if (x + y) % 3 == 0 else 1.04) if center else (0.86 + 0.14 * n)
        h = 252 if center else 224
    elif style == "ice":
        k = 0.90 + 0.18 * n
        if (x - y + fam) % 5 == 0:
            k, h = 1.20, 244  # destello
    elif style == "crystal":
        facet = ((x + 2 * y) % 3 == 0)
        core = (2 <= x <= 5 and 2 <= y <= 5)
        k = 1.18 if (facet or core) else (0.86 + 0.16 * n)
        h = 250 if core else (238 if facet else 218)
    elif style == "grass":
        # Piedra musgosa / tierra fértil en el cuerpo; el cap verde va en y=0..1 cuando N expuesto.
        rock = (hash32(fam * 53 + x * 11 + y * 5) & 7) == 0
        k = (1.06 if rock else (0.86 + 0.16 * n))
        r = clamp8(r * k + 18 * (y / 7.0))
        g = clamp8(g * k + (6 if not rock else -4))
        b = clamp8(b * k - 6)
        return max(8, r), max(8, g), max(8, b), 252
    else:
        k = 1.0
    cr, cg, cb = clamp8(r * k), clamp8(g * k), clamp8(b * k)
    if (cr | cg | cb) == 0:
        cb = 8
    return cr, cg, cb, h


def grass_cap(fam: int, base: tuple[int, int, int], x: int, y: int) -> tuple[int, int, int, int] | None:
    """Dos filas de pasto en el borde N expuesto. None = usar el fill."""
    if y > 1:
        return None
    n = (hash32(fam * 128 + y * 16 + x) & 0xFF) / 255.0
    blade = (hash32(fam * 17 + x * 13) & 7) > (1 if y == 0 else 3)
    if blade:
        k = 1.05 + 0.15 * n
        return clamp8(base[0] * 0.55 * k), clamp8(base[1] * 1.15 * k), clamp8(base[2] * 0.45 * k), 255
    # tierra entre las briznas
    return clamp8(base[0] * 0.85), clamp8(base[1] * 0.70), clamp8(base[2] * 0.50), 200


def build() -> bytearray:
    px = bytearray(ATLAS * ATLAS * 4)
    for fam, (_name, base, style) in enumerate(FAMILIES):
        for mask in range(16):
            cn, ce, cs, cw = mask & N, mask & E, mask & S, mask & W
            ox, oy = mask * TILE, fam * TILE
            for y in range(TILE):
                for x in range(TILE):
                    if style == "grass" and not cn:
                        cap = grass_cap(fam, base, x, y)
                        if cap is not None:
                            r, g, b, h = cap
                        else:
                            r, g, b, h = fill_pixel(fam, style, base, x, y)
                    else:
                        r, g, b, h = fill_pixel(fam, style, base, x, y)
                    k = 1.0
                    # Bisel: solo lados expuestos.
                    if (not cw and x == 0) or (not cn and y == 0):
                        k *= 1.14
                    if (not ce and x == TILE - 1) or (not cs and y == TILE - 1):
                        k *= 0.76
                        h = clamp8(h * 0.5)
                    r, g, b = clamp8(r * k), clamp8(g * k), clamp8(b * k)
                    i = ((oy + y) * ATLAS + (ox + x)) * 4
                    px[i] = r
                    px[i + 1] = g
                    px[i + 2] = b
                    px[i + 3] = h
    return px


def write_png(path: Path, rgba: bytes, w: int = ATLAS, h: int = ATLAS) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b"".join(b"\x00" + rgba[y * w * 4 : (y + 1) * w * 4] for y in range(h))
    # nivel 9; --check no depende de estos bytes.
    compressed = zlib.compress(raw, 9)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", compressed)
    png += chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def read_png_rgba(path: Path) -> tuple[int, int, bytes]:
    """Decodificador mínimo (8-bit RGBA, filtro 0 u 1, un IDAT concatenado)."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("no es PNG")
    pos, w, h, raw_idat, bit, color, filt = 8, 0, 0, b"", 8, 6, 0
    while pos < len(data):
        ln = struct.unpack(">I", data[pos : pos + 4])[0]
        tag = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + ln]
        pos += 12 + ln
        if tag == b"IHDR":
            w, h, bit, color, _comp, filt, _inter = struct.unpack(">IIBBBBB", chunk)
        elif tag == b"IDAT":
            raw_idat += chunk
        elif tag == b"IEND":
            break
    if bit != 8 or color != 6 or filt != 0:
        raise ValueError(f"PNG no soportado: {w}x{h} bit={bit} color={color} filt={filt}")
    raw = zlib.decompress(raw_idat)
    bpp, rows, prev = 4, [], bytearray(w * 4)
    stride = 1 + w * bpp
    if len(raw) != stride * h:
        raise ValueError("IDAT de tamaño inesperado")
    for y in range(h):
        ft = raw[y * stride]
        row = bytearray(raw[y * stride + 1 : (y + 1) * stride])
        if ft == 1:  # Sub
            for i in range(len(row)):
                row[i] = (row[i] + (row[i - bpp] if i >= bpp else 0)) & 0xFF
        elif ft == 2:  # Up
            for i in range(len(row)):
                row[i] = (row[i] + prev[i]) & 0xFF
        elif ft == 0:
            pass
        else:
            raise ValueError(f"filtro {ft} no soportado")
        rows.append(bytes(row))
        prev = row
    return w, h, b"".join(rows)


def write_ppm(path: Path, rgba: bytes, w: int = ATLAS, h: int = ATLAS) -> None:
    body = bytearray()
    for i in range(0, len(rgba), 4):
        body += rgba[i : i + 3]
    path.write_bytes(f"P6\n{w} {h}\n255\n".encode("ascii") + body)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--output", type=Path, help="escribe el PNG")
    ap.add_argument("--check", type=Path, help="falla si los pixels no coinciden con el generador")
    ap.add_argument("--ppm", type=Path, help="vuelca RGB a un PPM (inspección)")
    args = ap.parse_args()
    if not (args.output or args.check or args.ppm):
        ap.print_help()
        return 2
    rgba = bytes(build())
    if args.output:
        write_png(args.output, rgba)
        print(f"gen_atlas: {args.output} ({ATLAS}x{ATLAS}, {len(FAMILIES)} familias)")
    if args.ppm:
        write_ppm(args.ppm, rgba)
        print(f"gen_atlas: {args.ppm}")
    if args.check:
        w, h, got = read_png_rgba(args.check)
        exp = rgba
        if (w, h) != (ATLAS, ATLAS):
            print(f"FAIL tamaño {w}x{h}, se esperaba {ATLAS}x{ATLAS}", file=sys.stderr)
            return 1
        if got != exp:
            diffs = sum(1 for a, b in zip(got, exp) if a != b)
            print(f"FAIL {args.check} difiere del generador ({diffs} bytes)", file=sys.stderr)
            return 1
        print(f"gen_atlas: {args.check} OK (pixels = generador)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
