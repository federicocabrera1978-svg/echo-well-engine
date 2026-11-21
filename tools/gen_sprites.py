#!/usr/bin/env python3
"""Genera el atlas de sprites pixel-art PNG 128×128 (8×8 celdas de 16×16 px).

Uso:
  python3 tools/gen_sprites.py -o assets/atlas/sprites.png
  python3 tools/gen_sprites.py --check assets/atlas/sprites.png
  python3 tools/gen_sprites.py --ppm sprites.ppm

Layout (16×16 por celda, transparente en A=0, A=1..255 = relieve/altura, >240 = auto-emisión):
  Fila 0: Idle       (cols 0..3) — Explorador del Farol en reposo mirando a +X
  Fila 1: Walk       (cols 0..3) — Caminata de 4 cuadros
  Fila 2: Jump       (cols 0..3) — 0..1 impulso ascendente, 2..3 caída con capa al viento
  Fila 3: DoubleJump (cols 0..3) — Despliegue de alas dracónicas cian en el aire
  Fila 4: Climb      (cols 0..3) — Trepando cuerda (brazos y piernas alternados)
  Fila 5: Swim       (cols 0..3) — Nado horizontal con brazada y reflejos de agua
  Fila 6: Items      (cols 0..3 Huevo de Dragón con escamas, cols 4..7 Reliquia Alada)

Sin dependencias externas: PNG vía zlib (stdlib), filtro 0, IHDR/IDAT/IEND.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

CELL = 16
GRID = 8
ATLAS = CELL * GRID  # 128

# Paleta protagonista: Explorador del Farol (fantasía oscura D&D + Animal Well)
HERO_PAL: dict[str, tuple[int, int, int, int]] = {
    ".": (0, 0, 0, 0),
    "o": (20, 18, 34, 140),     # contorno índigo profundo
    "c": (46, 44, 78, 178),     # sombra de capa
    "C": (70, 68, 114, 210),    # tono medio de capa / capucha
    "h": (104, 102, 158, 232),  # brillo superior de capucha / hombro
    "s": (16, 14, 26, 156),     # sombra bajo la capucha
    "e": (255, 224, 130, 250),  # ojo brillante ámbar (auto-emisivo)
    "b": (136, 56, 52, 198),    # bufanda / cinturón carmesí apagado
    "p": (34, 34, 56, 168),     # túnica / pantalón oscuro
    "k": (84, 54, 40, 192),     # botas de cuero
    "m": (128, 136, 152, 215),  # metal del farol
    "l": (255, 172, 54, 248),   # llama cálida del farol (auto-emisiva)
    "L": (255, 246, 182, 255),  # núcleo blanco-oro del farol (auto-emisiva)
    "w": (46, 186, 232, 244),   # ala / estela dracónica cian (auto-emisiva)
    "W": (170, 246, 255, 255),  # filo brillante del ala cian
    "a": (90, 210, 238, 225),   # espuma / reflejo aguamarina (nado)
}

# Paleta objetos: Huevo de Dragón (cols 0..3) y Reliquia Alada (cols 4..7)
EGG_PAL: dict[str, tuple[int, int, int, int]] = {
    ".": (0, 0, 0, 0),
    "o": (30, 16, 20, 145),     # contorno oscuro
    "d": (118, 34, 32, 184),    # escama sombra rojo oscuro
    "r": (176, 56, 44, 214),    # escama carmesí
    "g": (222, 156, 60, 232),   # ribete bronce / oro
    "f": (255, 192, 74, 250),   # brasa dracónica interna (auto-emisiva)
    "F": (255, 242, 172, 255),  # chispa cálida (auto-emisiva)
}

RELIC_PAL: dict[str, tuple[int, int, int, int]] = {
    ".": (0, 0, 0, 0),
    "o": (18, 32, 50, 145),     # contorno azul oscuro
    "c": (42, 142, 196, 204),   # pluma cian oscuro
    "C": (88, 216, 248, 232),   # pluma cian brillante
    "w": (208, 246, 255, 248),  # filo plateado alado
    "g": (116, 248, 255, 250),  # aura de gema central
    "G": (244, 255, 255, 255),  # núcleo incandescente de reliquia
}

# Cada celda es 16×16. Personaje protagonista ocupa 12×12 (x=2..13, y=2..13), patas hasta y=13.
FRAMES: dict[tuple[int, int], tuple[dict[str, tuple[int, int, int, int]], list[str]]] = {
    # --- Fila 0: Idle (0..3) ---
    (0, 0): (HERO_PAL, [
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo......",
        "....oChCCo..m...",
        "...ocCCCCo.mlo..",
        "...ocCCCbo.mLo..",
        "...oopppbo..m...",
        "....okooko......",
        "....okooko......",
        "....ookook......",
        "................",
        "................",
    ]),
    (1, 0): (HERO_PAL, [
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo......",
        "....oChCCo..m...",
        "...ocCCCCo.mLo..",
        "...ocCCCbo.mlo..",
        "...oopppbo..m...",
        "....okooko......",
        "....okooko......",
        "....ookook......",
        "................",
        "................",
    ]),
    (2, 0): (HERO_PAL, [
        "................",
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo..m...",
        "...oChCCCo.mLo..",
        "...ocCCCbo.mLo..",
        "...oopppbo..m...",
        "....okooko......",
        "....okooko......",
        "....ookook......",
        "................",
        "................",
    ]),
    (3, 0): (HERO_PAL, [
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo......",
        "....oChCCo..m...",
        "...ocCCCCo.mLo..",
        "...ocCCCbo.mLo..",
        "...oopppbo..m...",
        "....okooko......",
        "....okooko......",
        "....ookook......",
        "................",
        "................",
    ]),

    # --- Fila 1: Walk (0..3) ---
    (0, 1): (HERO_PAL, [
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo......",
        "...oChCCCo..m...",
        "..ocCCCCCo.mLo..",
        "..oocCCCbo.mlo..",
        "....opppbo..m...",
        "...okooooko.....",
        "..ooko..oko.....",
        "........ook.....",
        "................",
        "................",
    ]),
    (1, 1): (HERO_PAL, [
        "................",
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo..m...",
        "...oChCCCo.mLo..",
        "...ocCCCbo.mLo..",
        "...oopppbo..m...",
        "....okooko......",
        "....ookok.......",
        "......oko.......",
        "................",
        "................",
    ]),
    (2, 1): (HERO_PAL, [
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo......",
        "...oChCCCo...m..",
        "..ocCCCCCo..mLo.",
        "..oocCCCbo..mlo.",
        "....opppbo...m..",
        "....okooko......",
        "....okoooko.....",
        "....ook...o.....",
        "................",
        "................",
    ]),
    (3, 1): (HERO_PAL, [
        "................",
        "................",
        "................",
        ".....oooo.......",
        "....ohCCCo......",
        "...ocCCssso.....",
        "...ocCssseo.....",
        "....ocbbbo..m...",
        "...oChCCCo.mlo..",
        "...ocCCCbo.mLo..",
        "...oopppbo..m...",
        "....okooko......",
        "....okooko......",
        "....ookook......",
        "................",
        "................",
    ]),

    # --- Fila 2: Jump (0..1 ascenso, 2..3 descenso) ---
    (0, 2): (HERO_PAL, [
        "................",
        ".....oooo.......",
        "....ohCCCo..m...",
        "...ocCCsssomLo..",
        "...ocCssseomlo..",
        "....ocbbbo..m...",
        "...oChCCCo......",
        "..ocCCCCCo......",
        "..ocCCCCbo......",
        "..oocpppko......",
        "...okooko.......",
        "...ok.ok........",
        "................",
        "................",
        "................",
        "................",
    ]),
    (1, 2): (HERO_PAL, [
        "................",
        ".....oooo.......",
        "....ohCCCo..m...",
        "...ocCCsssomLo..",
        "...ocCssseomLo..",
        "....ocbbbo..m...",
        "...oChCCCo......",
        "..ocCCCCCo......",
        ".oocCCCCbo......",
        ".o.ocpppko......",
        "....okooko......",
        "....okook.......",
        "................",
        "................",
        "................",
        "................",
    ]),
    (2, 2): (HERO_PAL, [
        "................",
        "................",
        "....oooo........",
        "...ohCCCo.......",
        "..ocCCssso...m..",
        ".oocCssseo..mLo.",
        ".ocCCbbbo...mlo.",
        "oChCCCCo.....m..",
        "oCCCCbo.........",
        "oopppbo.........",
        "...okooko.......",
        "...ok..oko......",
        "...ok...ok......",
        "................",
        "................",
        "................",
    ]),
    (3, 2): (HERO_PAL, [
        "................",
        "................",
        "....oooo........",
        "...ohCCCo.......",
        "..ocCCssso..m...",
        "..ocCssseo.mLo..",
        ".oocCbbbo..mLo..",
        "oChCCCCo....m...",
        "ocCCCbo.........",
        ".oopppbo........",
        "....okooko......",
        "...ook..oko.....",
        "...ok....ok.....",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 3: DoubleJump (alas dracónicas cian) ---
    (0, 3): (HERO_PAL, [
        "................",
        ".WW..oooo.......",
        ".wWWohCCCo..m...",
        "..wwCCsssomLo...",
        "..wocCssseomlo..",
        "...wocbbbo..m...",
        "...oChCCCo......",
        "..wocCCCCo......",
        "...oopppko......",
        "....okooko......",
        "....ok.oko......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (1, 3): (HERO_PAL, [
        ".W..............",
        "WWw..oooo.......",
        "wwwWohCCCo..m...",
        ".wWwCCsssomLo...",
        ".wWocCssseomLo..",
        "..wwocbbbo..m...",
        "...oChCCCo......",
        "..wocCCCCo......",
        "...oopppko......",
        "....okooko......",
        "....okook.......",
        ".....w.W........",
        "................",
        "................",
        "................",
        "................",
    ]),
    (2, 3): (HERO_PAL, [
        "................",
        ".wW..oooo.......",
        "..WwohCCCo..m...",
        "..wwCCsssomLo...",
        "...ocCssseomlo..",
        "....ocbbbo..m...",
        "...oChCCCo......",
        "..wocCCCCo......",
        "...oopppko......",
        "....okooko......",
        "...w.ok.W.......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (3, 3): (HERO_PAL, [
        "................",
        ".....oooo.......",
        "..w.ohCCCo..m...",
        "...wCCsssomLo...",
        "...ocCssseomlo..",
        "....ocbbbo..m...",
        "...oChCCCo......",
        "...ocCCCCo......",
        "...oopppko......",
        "....okooko......",
        "....w...W.......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 4: Climb (0..3) ---
    (0, 4): (HERO_PAL, [
        ".......om.......",
        "......oooo......",
        ".....ohCCCo.....",
        "....ocCssseo....",
        "....ocCbbbo.....",
        "...moChCCo......",
        "..mLoCCCCCoo....",
        "..mloCCCCCoko...",
        "...m.opppbo.....",
        "....okooko......",
        "....ok..oko.....",
        "....ok...ok.....",
        "................",
        "................",
        "................",
        "................",
    ]),
    (1, 4): (HERO_PAL, [
        "......m..m......",
        "......oooo......",
        ".....ohCCCo.....",
        "....ocCssseo....",
        "....ocCbbbo.....",
        "....oChCCCo.....",
        "...ooCCCCCCo....",
        "...oCCCCCCoo....",
        "....opppbmo.....",
        "....okoomLo.....",
        "....ok..okm.....",
        "....ok..ok......",
        "................",
        "................",
        "................",
        "................",
    ]),
    (2, 4): (HERO_PAL, [
        ".......mo.......",
        "......oooo......",
        ".....ohCCCo.....",
        "....ocCssseo....",
        "....ocCbbbo.....",
        ".....oChCCo.m...",
        "....ooCCCCComLo.",
        "....okoCCCComlo.",
        ".....opppbo..m..",
        ".....okooko.....",
        "....oko..ok.....",
        "....ok...ok.....",
        "................",
        "................",
        "................",
        "................",
    ]),
    (3, 4): (HERO_PAL, [
        "......m..m......",
        "......oooo......",
        ".....ohCCCo.....",
        "....ocCssseo....",
        "....ocCbbbo.....",
        "....oChCCCo.....",
        "...ooCCCCCCo....",
        "...oCCCCCCoo....",
        "....opppbmo.....",
        "....okoomlo.....",
        "....ok..okm.....",
        "....ok..ok......",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 5: Swim (0..3) ---
    (0, 5): (HERO_PAL, [
        "................",
        "................",
        "................",
        "......oooo......",
        ".....ohCCCo..m..",
        "....ocCCsssomLo.",
        "....ocCssseomlo.",
        "..ooocCbbbo..m..",
        ".oChCCCCCoo.....",
        "aaoCCppppo.aa...",
        ".aaookoooka.....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (1, 5): (HERO_PAL, [
        "................",
        "................",
        "................",
        "......oooo......",
        ".....ohCCCo..m..",
        "....ocCCsssomLo.",
        "....ocCssseomLo.",
        ".oooocCbbbo..m..",
        "oChCCCCCCoo.....",
        ".aaoCppppo.a....",
        "..aokooko.aa....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (2, 5): (HERO_PAL, [
        "................",
        "................",
        "................",
        "......oooo......",
        ".....ohCCCo..m..",
        "....ocCCsssomLo.",
        "....ocCssseomlo.",
        "..ooocCbbbo..m..",
        ".oChCCCCCoo.....",
        ".aooCppppoaaa...",
        "aa..okook.......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (3, 5): (HERO_PAL, [
        "................",
        "................",
        "................",
        "......oooo......",
        ".....ohCCCo..m..",
        "....ocCCsssomLo.",
        "....ocCssseomLo.",
        ".oooocCbbbo..m..",
        "oChCCCCCCoo.....",
        "aaaoCppppo..a...",
        "...okooko..a....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 6, cols 0..3: Huevo de Dragón con escamas ---
    (0, 6): (EGG_PAL, [
        "................",
        "................",
        "......oooo......",
        ".....orrrgo.....",
        "....orrgddgo....",
        "...orgrrfdgro...",
        "...ordrfFfdro...",
        "...ogdrdfddgo...",
        "...ordrggrrdo...",
        "...odddrrdddo...",
        "....odgddgdo....",
        "....oodggdoo....",
        ".....oooooo.....",
        "................",
        "................",
        "................",
    ]),
    (1, 6): (EGG_PAL, [
        "................",
        "................",
        "......oooo......",
        ".....orrrgo.....",
        "....orrgfdgo....",
        "...orgrfFFgro...",
        "...ordrfFfbro...".replace("b", "d"),
        "...ogdrdfddgo...",
        "...ordrggrrdo...",
        "...odddrrdddo...",
        "....odgddgdo....",
        "....oodggdoo....",
        ".....oooooo.....",
        "................",
        "................",
        "................",
    ]),
    (2, 6): (EGG_PAL, [
        "................",
        "................",
        "......oooo......",
        ".....orrrgo.....",
        "....orrgFFgo....",
        "...orgrFFfgro...",
        "...ordrfFfdro...",
        "...ogdrdfddgo...",
        "...ordrggrrdo...",
        "...odddrrdddo...",
        "....odgddgdo....",
        "....oodggdoo....",
        ".....oooooo.....",
        "................",
        "................",
        "................",
    ]),
    (3, 6): (EGG_PAL, [
        "................",
        "................",
        "......oooo......",
        ".....orrrgo.....",
        "....orrgddgo....",
        "...orgrrfFgro...",
        "...ordrfFFdro...",
        "...ogdrdfddgo...",
        "...ordrggrrdo...",
        "...odddrrdddo...",
        "....odgddgdo....",
        "....oodggdoo....",
        ".....oooooo.....",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 6, cols 4..7: Reliquia Alada de Doble Salto ---
    (4, 6): (RELIC_PAL, [
        "................",
        "................",
        ".ooo........ooo.",
        ".owwo......owwo.",
        ".owCCo.oo.oCCwo.",
        "..ocCCoggCoCCo..",
        "...ocCogGgoCo...",
        "....oCogGgoCo...",
        ".....oCggCo.....",
        "......occo......",
        "......oCCo......",
        ".......oo.......",
        "................",
        "................",
        "................",
        "................",
    ]),
    (5, 6): (RELIC_PAL, [
        "................",
        ".ooo........ooo.",
        ".owwo......owwo.",
        ".owCCo....oCCwo.",
        "..ocCCo..oCCCo..",
        "...ocCoggoCCo...",
        "....oCogGgoCo...",
        ".....ogGGgo.....",
        ".....oCggCo.....",
        "......occo......",
        "......oCCo......",
        ".......oo.......",
        "................",
        "................",
        "................",
        "................",
    ]),
    (6, 6): (RELIC_PAL, [
        "................",
        "................",
        ".oo..........oo.",
        ".owwo......owwo.",
        ".owCCo.oo.oCCwo.",
        "..ocCCoGGoCCCo..",
        "...ocCogGgoCo...",
        "....oCogggoCo...",
        ".....oCggCo.....",
        "......occo......",
        "......oCCo......",
        ".......oo.......",
        "................",
        "................",
        "................",
        "................",
    ]),
    (7, 6): (RELIC_PAL, [
        "................",
        "................",
        "................",
        ".ooo........ooo.",
        ".owCCo.oo.oCCwo.",
        ".owCCoggCoCCwo..",
        "..ocCCoGGoCCCo..",
        "...ocCogggoCo...",
        "....ooCggCoo....",
        "......occo......",
        "......oCCo......",
        ".......oo.......",
        "................",
        "................",
        "................",
        "................",
    ]),
}


def build() -> bytearray:
    px = bytearray(ATLAS * ATLAS * 4)
    for (col, row), (pal, grid) in FRAMES.items():
        if not (0 <= col < GRID and 0 <= row < GRID):
            raise ValueError(f"celda fuera de rango: ({col}, {row})")
        if len(grid) != CELL:
            raise ValueError(f"celda ({col}, {row}) tiene {len(grid)} filas (se esperaban {CELL})")
        ox, oy = col * CELL, row * CELL
        for y, line in enumerate(grid):
            if len(line) != CELL:
                raise ValueError(f"celda ({col}, {row}) fila {y} mide {len(line)} (se esperaba {CELL})")
            for x, ch in enumerate(line):
                rgba = pal[ch]
                i = ((oy + y) * ATLAS + (ox + x)) * 4
                px[i : i + 4] = bytes(rgba)
    return px


def write_png(path: Path, rgba: bytes, w: int = ATLAS, h: int = ATLAS) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b"".join(b"\x00" + rgba[y * w * 4 : (y + 1) * w * 4] for y in range(h))
    compressed = zlib.compress(raw, 9)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", compressed)
    png += chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def read_png_rgba(path: Path) -> tuple[int, int, bytes]:
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
        if ft == 1:
            for i in range(len(row)):
                row[i] = (row[i] + (row[i - bpp] if i >= bpp else 0)) & 0xFF
        elif ft == 2:
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
        if rgba[i + 3] == 0:
            body += bytes((12, 14, 24))
        else:
            body += rgba[i : i + 3]
    path.parent.mkdir(parents=True, exist_ok=True)
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
        print(f"gen_sprites: {args.output} ({ATLAS}x{ATLAS}, {len(FRAMES)} sprites)")
    if args.ppm:
        write_ppm(args.ppm, rgba)
        print(f"gen_sprites: {args.ppm}")
    if args.check:
        w, h, got = read_png_rgba(args.check)
        if (w, h) != (ATLAS, ATLAS):
            print(f"FAIL tamaño {w}x{h}, se esperaba {ATLAS}x{ATLAS}", file=sys.stderr)
            return 1
        if got != rgba:
            diffs = sum(1 for a, b in zip(got, rgba) if a != b)
            print(f"FAIL {args.check} difiere del generador ({diffs} bytes)", file=sys.stderr)
            return 1
        print(f"gen_sprites: {args.check} OK (pixels = generador)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
