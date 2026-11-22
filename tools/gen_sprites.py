#!/usr/bin/env python3
"""Genera el atlas de sprites pixel-art PNG 256×128 (16×8 celdas de 16×16 px).

Uso:
  python3 tools/gen_sprites.py -o assets/atlas/sprites.png
  python3 tools/gen_sprites.py --check assets/atlas/sprites.png
  python3 tools/gen_sprites.py --ppm sprites.ppm

Layout (16×16 por celda, transparente en A=0, A=1..255 = relieve/altura, >240 = auto-emisión):
  Cols 0..3,  filas 0..5: Sr. del Farol (Idle, Walk, Jump, DoubleJump, Climb, Swim)
  Cols 4..7,  filas 0..5: La Zarigüeya  (Idle, Walk, Jump, DoubleJump, Climb, Swim)
  Cols 8..11, filas 0..5: El Castor     (Idle, Walk, Jump, DoubleJump, Climb, Swim)
  Fila 6: Items    (cols 0..3 Huevo de Dragón con escamas, cols 4..7 Reliquia Alada)
  Fila 7: Npc/Lore (cols 0..3 Vigía/Cronista del Farol, cols 4..7 Estela dracónica con runas)

Sin dependencias externas: PNG vía zlib (stdlib), filtro 0, IHDR/IDAT/IEND.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

CELL = 16
GRID_W = 16
GRID_H = 8
ATLAS_W = CELL * GRID_W  # 256
ATLAS_H = CELL * GRID_H  # 128

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

# Paleta protagonista 2: La Zarigüeya (cols 4..7, filas 0..5)
OPOSSUM_PAL: dict[str, tuple[int, int, int, int]] = {
    ".": (0, 0, 0, 0),
    "o": (22, 20, 32, 140),     # contorno oscuro índigo-pizarra
    "d": (74, 78, 96, 178),     # lomo gris oscuro
    "g": (138, 144, 162, 210),  # pelaje gris plata medio
    "h": (186, 194, 212, 228),  # pelaje gris plata claro
    "w": (238, 232, 218, 232),  # hocico blanco / vientre crema
    "m": (42, 40, 56, 165),     # máscara oscura facial
    "p": (236, 134, 156, 208),  # rosa (orejas, nariz, cola prensil)
    "P": (196, 92, 118, 185),   # rosa sombra (cola prensil)
    "e": (255, 220, 92, 250),   # ojo dorado brillante (auto-emisivo)
    "t": (124, 134, 150, 210),  # asa del pequeño farol
    "f": (100, 220, 248, 246),  # farol celeste brillante (auto-emisivo)
    "F": (255, 236, 152, 254),  # núcleo dorado del farol (auto-emisivo)
    "c": (64, 224, 248, 246),   # membrana / ala espectral cian (auto-emisiva)
    "C": (188, 252, 255, 255),  # filo brillante del ala espectral cian
    "a": (90, 210, 238, 225),   # espuma / estela aguamarina (nado)
}

# Paleta protagonista 3: El Castor (cols 8..11, filas 0..5)
BEAVER_PAL: dict[str, tuple[int, int, int, int]] = {
    ".": (0, 0, 0, 0),
    "o": (26, 16, 14, 140),     # contorno marrón profundo
    "d": (88, 42, 26, 182),     # pelaje castaño-rojizo oscuro
    "b": (138, 68, 40, 212),    # pelaje castaño-rojizo medio
    "h": (178, 98, 60, 228),    # brillo de pelaje
    "w": (226, 198, 162, 220),  # hocico / vientre crema
    "z": (248, 246, 240, 238),  # dos dientes incisivos blancos
    "r": (188, 42, 46, 215),    # pañuelo rojo de aventurero
    "R": (226, 76, 66, 232),    # brillo del pañuelo rojo
    "e": (255, 196, 72, 250),   # ojo ámbar luminoso (auto-emisivo)
    "k": (58, 34, 24, 196),     # cola ancha plana / patas marrón oscura
    "K": (92, 56, 38, 212),     # cuadriculado de cola plana
    "m": (166, 134, 72, 218),   # latón del farol
    "l": (255, 168, 52, 248),   # llama cálida del farol (auto-emisiva)
    "L": (255, 242, 174, 255),  # núcleo brillante del farol (auto-emisivo)
    "g": (255, 218, 94, 248),   # alas doradas (auto-emisiva)
    "c": (92, 236, 255, 252),   # filo cian brillante de alas
    "C": (196, 252, 255, 255),  # destello blanco-cian de alas
    "a": (90, 210, 238, 225),   # espuma / estela aguamarina (nado)
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

NPC_PAL: dict[str, tuple[int, int, int, int]] = {
    ".": (0, 0, 0, 0),
    "o": (22, 16, 24, 145),     # contorno oscuro
    "r": (92, 46, 42, 186),     # túnica sombra carmesí/bronce
    "R": (134, 68, 54, 214),    # túnica tono medio
    "g": (214, 162, 74, 232),   # ribete dorado
    "w": (214, 208, 194, 216),  # barba blanca
    "s": (18, 14, 24, 156),     # sombra capucha
    "e": (255, 224, 130, 250),  # ojo sabio luminoso (auto-emisivo)
    "t": (106, 76, 48, 202),    # madera del báculo
    "f": (255, 184, 64, 248),   # gema/llama del báculo (auto-emisiva)
    "F": (255, 244, 182, 255),  # núcleo brillante del báculo (auto-emisivo)
}

LORE_PAL: dict[str, tuple[int, int, int, int]] = {
    ".": (0, 0, 0, 0),
    "o": (16, 18, 28, 145),     # contorno oscuro
    "d": (48, 52, 70, 186),     # piedra sombra
    "m": (76, 82, 108, 214),    # piedra media
    "h": (112, 118, 148, 232),  # arista piedra iluminada
    "f": (255, 168, 58, 248),   # runa dracónica brasa (auto-emisiva)
    "F": (255, 234, 148, 255),  # runa núcleo dorado (auto-emisivo)
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

    # --- Fila 7, cols 0..3: NPC Vigía / Cronista con báculo luminoso ---
    (0, 7): (NPC_PAL, [
        "................",
        "................",
        "..ooo..oooo.....",
        ".oFfo.oRRgRo....",
        "..oto.oRssso....",
        "..oto.oeswwo....",
        "..oto.orwggRo...",
        "..otooorRRRRRo..",
        "..otooRRRrrRRo..",
        "..oto.orrrrrRo..",
        "..oto.orrrgRro..",
        "..oto.oorrrrro..",
        "..oto..oororo...",
        "..ooo..oooooo...",
        "................",
        "................",
    ]),
    (1, 7): (NPC_PAL, [
        "................",
        "................",
        "..ooo..oooo.....",
        ".ofFo.oRRgRo....",
        "..oto.oRssso....",
        "..oto.oeswwo....",
        "..oto.orwggRo...",
        "..otooorRRRRRo..",
        "..otooRRRRrRRo..",
        "..oto.orrrrrRo..",
        "..oto.orrrgRro..",
        "..oto.oorrrrro..",
        "..oto..oororo...",
        "..ooo..oooooo...",
        "................",
        "................",
    ]),
    (2, 7): (NPC_PAL, [
        "................",
        "................",
        "..ooo...........",
        ".oFFo..oooo.....",
        "..oto.oRRgRo....",
        "..oto.oRssso....",
        "..oto.oeswwo....",
        "..otooorwggRo...",
        "..otooRRRRRRRo..",
        "..oto.orrrrRRo..",
        "..oto.orrrgRro..",
        "..oto.oorrrrro..",
        "..oto..oororo...",
        "..ooo..oooooo...",
        "................",
        "................",
    ]),
    (3, 7): (NPC_PAL, [
        "................",
        "................",
        "..ooo..oooo.....",
        ".oFFo.oRRgRo....",
        "..oto.oRssso....",
        "..oto.oeswwo....",
        "..oto.orwggRo...",
        "..otooorRRRRRo..",
        "..otooRRRrrRRo..",
        "..oto.orrrrrRo..",
        "..oto.oorrgRro..",
        "..oto.oorrrrro..",
        "..oto..oororo...",
        "..ooo..oooooo...",
        "................",
        "................",
    ]),

    # --- Fila 7, cols 4..7: Estela dracónica con runas incandescentes ---
    (4, 7): (LORE_PAL, [
        "................",
        "......oooo......",
        ".....ohhhho.....",
        "....ohmmmmho....",
        "....ohmffmmo....",
        "....ommfFmmo....",
        "....omdmmmdo....",
        "....omdffmdo....",
        "....omdmmddo....",
        "....omdfdddo....",
        "....ohmmdddo....",
        "...oohmmdddoo...",
        "...ohdddddddo...",
        "...oooooooooo...",
        "................",
        "................",
    ]),
    (5, 7): (LORE_PAL, [
        "................",
        "......oooo......",
        ".....ohhhho.....",
        "....ohmmmmho....",
        "....ohmFfmmo....",
        "....ommFFmmo....",
        "....omdmmmdo....",
        "....omdFfmdo....",
        "....omdmmddo....",
        "....omdFdddo....",
        "....ohmmdddo....",
        "...oohmmdddoo...",
        "...ohdddddddo...",
        "...oooooooooo...",
        "................",
        "................",
    ]),
    (6, 7): (LORE_PAL, [
        "................",
        "......oooo......",
        ".....ohhhho.....",
        "....ohmmmmho....",
        "....ohmFFmmo....",
        "....ommffmmo....",
        "....omdmmmdo....",
        "....omdfFmdo....",
        "....omdmmddo....",
        "....omdFdddo....",
        "....ohmmdddo....",
        "...oohmmdddoo...",
        "...ohdddddddo...",
        "...oooooooooo...",
        "................",
        "................",
    ]),
    (7, 7): (LORE_PAL, [
        "................",
        "......oooo......",
        ".....ohhhho.....",
        "....ohmmmmho....",
        "....ohmffmmo....",
        "....ommfFmmo....",
        "....omdmmmdo....",
        "....omdffmdo....",
        "....omdmmddo....",
        "....omdfFddo....",
        "....ohmmdddo....",
        "...oohmmdddoo...",
        "...ohdddddddo...",
        "...oooooooooo...",
        "................",
        "................",
    ]),

    # =========================================================================
    # La Zarigüeya (cols 4..7, filas 0..5)
    # =========================================================================
    # --- Fila 0: Idle (4..7) ---
    (4, 0): (OPOSSUM_PAL, [
        "................",
        "................",
        "....oo..oo......",
        "....opoopo......",
        "...odghhdmo.....",
        "...odgmmewwwo...",
        "..ooogwwwwwpo...",
        ".opPodhhwwwt....",
        ".oPo.odgwwwtfFo.",
        ".opPoodgwwwtFfo.",
        "..ooooddwwto....",
        ".....odoodo.....",
        ".....opoopo.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),
    (5, 0): (OPOSSUM_PAL, [
        "................",
        "................",
        "....oo..oo......",
        "....opoopo......",
        "...odghhdmo.....",
        "...odgmmewwwo...",
        ".ooo.ogwwwwpo...",
        ".opPoodhwwwt....",
        "..oPoodgwwwtFfo.",
        ".opPoodgwwwtfFo.",
        "..ooooddwwto....",
        ".....odoodo.....",
        ".....opoopo.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),
    (6, 0): (OPOSSUM_PAL, [
        "................",
        "................",
        "................",
        "....oo..oo......",
        "....opoopo......",
        "...odghhdmo.....",
        "...odgmmewwwo...",
        "..ooogwwwwwpo...",
        ".opPodhhwwwtFfo.",
        ".oPo.odgwwwtfFo.",
        ".opPooddwwto....",
        "..oooodoodo.....",
        ".....opoopo.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),
    (7, 0): (OPOSSUM_PAL, [
        "................",
        "................",
        "....oo..oo......",
        "....opoopo......",
        "...odghhdmo.....",
        "...odgmmewwwo...",
        "..ooogwwwwwpo...",
        ".opPodhhwwwt....",
        ".oPo.odgwwwtFFo.",
        ".opPoodgwwwtffo.",
        "..ooooddwwto....",
        ".....odoodo.....",
        ".....opoopo.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),

    # --- Fila 1: Walk (4..7) — trote ágil con cola balanceándose ---
    (4, 1): (OPOSSUM_PAL, [
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwwo..",
        ".....ogwwwwwpo..",
        ".ooo.odhhwwwt...",
        "opPoodggwwwtFfo.",
        ".oPoooddwwtofo..",
        "..ooo.odoodo....",
        ".....opoo.opo...",
        "....opoo...oo...",
        "....ooo.........",
        "................",
        "................",
    ]),
    (5, 1): (OPOSSUM_PAL, [
        "................",
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwwo..",
        "..oo.ogwwwwwpo..",
        ".opPoodhhwwwt...",
        ".oPo.odggwwwtfFo",
        "..ooooddwwto.o..",
        "......odoodo....",
        "......opoopo....",
        "......oooooo....",
        "................",
        "................",
    ]),
    (6, 1): (OPOSSUM_PAL, [
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwwo..",
        ".oo..ogwwwwwpo..",
        "opPo.odhhwwwt...",
        ".oPooodggwwwtfFo",
        "..ooooddwwtoFfo.",
        ".....odoodo..o..",
        "....opo..opo....",
        "....oo....opo...",
        "..........ooo...",
        "................",
        "................",
    ]),
    (7, 1): (OPOSSUM_PAL, [
        "................",
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwwo..",
        ".ooo.ogwwwwwpo..",
        "opPoodhhwwwt....",
        ".oooodggwwwtFfo.",
        ".....oddwwto.o..",
        "......odoodo....",
        ".....opooopo....",
        ".....ooooooo....",
        "................",
        "................",
    ]),

    # --- Fila 2: Jump/Fall (4..5 ascenso, 6..7 descenso) ---
    (4, 2): (OPOSSUM_PAL, [
        "................",
        ".....oo..oo.....",
        ".....opoopo..t..",
        "....odghhdmofFo.",
        "....odgmmewwFfo.",
        ".....ogwwwwwpo..",
        "....odhhwwwto...",
        "....odggwwwo....",
        "...oodddwwo.....",
        "..opPodoodo.....",
        ".opPo.op.op.....",
        ".ooo............",
        "................",
        "................",
        "................",
        "................",
    ]),
    (5, 2): (OPOSSUM_PAL, [
        "................",
        ".....oo..oo.....",
        ".....opoopo..t..",
        "....odghhdmoFfo.",
        "....odgmmewwFFo.",
        ".....ogwwwwwpo..",
        "....odhhwwwto...",
        "...oodggwwwo....",
        "..opPoddwwo.....",
        ".opPo.odoodo....",
        "..oo..opo.opo...",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (6, 2): (OPOSSUM_PAL, [
        "................",
        "................",
        "....oo..oo......",
        "....opoopo......",
        ".oo.odghhdmo..t.",
        "opPoodgmmewwwfFo",
        ".oPo.ogwwwwwpFfo",
        "..oooodhhwwwto..",
        ".....odggwwwo...",
        ".....odddwwo....",
        "....opoodoodo...",
        "....oo....opo...",
        "................",
        "................",
        "................",
        "................",
    ]),
    (7, 2): (OPOSSUM_PAL, [
        "................",
        "................",
        "....oo..oo......",
        ".oo.opoopo......",
        "opPoodghhdmo.t..",
        ".oPo.odgmmewwfFo",
        "..oo.ogwwwwwpFfo",
        "....oodhhwwwto..",
        ".....odggwwwo...",
        ".....odddwwo....",
        "....opoodoodo...",
        "....ooo...opo...",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 3: DoubleJump (4..7) — membrana/alas espectrales cian brillantes ---
    (4, 3): (OPOSSUM_PAL, [
        "................",
        ".CC..oo..oo.....",
        ".cCC.opoopo..t..",
        "..ccodghhdmofFo.",
        "..coodgmmewwFfo.",
        "...codgwwwwwpo..",
        "...codhhwwwto...",
        "..ooodggwwwo....",
        ".opPooddwwo.....",
        ".ooo.odoodo.....",
        ".....op..op.....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (5, 3): (OPOSSUM_PAL, [
        "..C.............",
        ".CCc.oo..oo.....",
        "cccCCopoopo..t..",
        ".cCcodghhdmoFfo.",
        ".cCoodgmmewwFFo.",
        "..ccodgwwwwwpo..",
        "...codhhwwwto...",
        "..ooodggwwwo....",
        ".opPooddwwo.....",
        ".ooo.odoodo.....",
        ".....opo.op.....",
        ".....c..C.......",
        "................",
        "................",
        "................",
        "................",
    ]),
    (6, 3): (OPOSSUM_PAL, [
        "................",
        ".cC..oo..oo.....",
        "..Ccopoopo...t..",
        "..ccodghhdmofFo.",
        "...codgmmewwFfo.",
        "...oodgwwwwwpo..",
        "...codhhwwwto...",
        "..ooodggwwwo....",
        ".opPooddwwo.....",
        ".ooo.odoodo.....",
        "....c.op.C......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (7, 3): (OPOSSUM_PAL, [
        "................",
        ".....oo..oo.....",
        "..c..opoopo..t..",
        "...codghhdmofFo.",
        "...oodgmmewwFfo.",
        "....odgwwwwwpo..",
        "....odhhwwwto...",
        "..ooodggwwwo....",
        ".opPooddwwo.....",
        ".ooo.odoodo.....",
        "....c....C......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 4: Climb (4..7) ---
    (4, 4): (OPOSSUM_PAL, [
        ".......ot.......",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwpo..",
        "...todhhwwwto...",
        "..fFoodggwwwo...",
        "..Ffooodgwwopo..",
        "...o.opPoddwo...",
        ".....oPoodoodo..",
        "......oo.op.op..",
        ".........oo..o..",
        "................",
        "................",
        "................",
        "................",
    ]),
    (5, 4): (OPOSSUM_PAL, [
        "......t..t......",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwpo..",
        "....odhhwwwto...",
        "....odggwwwto...",
        "....oodgwwwopo..",
        ".....opPoddt....",
        ".....oPoodofFo..",
        "......oo.opFfo..",
        ".........oo.o...",
        "................",
        "................",
        "................",
        "................",
    ]),
    (6, 4): (OPOSSUM_PAL, [
        ".......to.......",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwpo..",
        "....odhhwwwtot..",
        "....odggwwwfFo..",
        "...oodgwwwpFfo..",
        "..opPooddwo.o...",
        "..oPo.odoodo....",
        "...oo.op..op....",
        "......oo...o....",
        "................",
        "................",
        "................",
        "................",
    ]),
    (7, 4): (OPOSSUM_PAL, [
        "......t..t......",
        ".....oo..oo.....",
        ".....opoopo.....",
        "....odghhdmo....",
        "....odgmmewwpo..",
        "....odhhwwwto...",
        "....odggwwwto...",
        "....oodgwwwopo..",
        ".....opPoddt....",
        ".....oPoodoFfo..",
        "......oo.opfFo..",
        ".........oo.o...",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 5: Swim (4..7) — nado con cola y estelas ---
    (4, 5): (OPOSSUM_PAL, [
        "................",
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo..t..",
        "....odghhdmofFo.",
        "....odgmmewwFfo.",
        ".oo.odgwwwwwpo..",
        "opPoodhhwwwto...",
        "aaoodggwwwo.aa..",
        ".aa.oopoopoa....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (5, 5): (OPOSSUM_PAL, [
        "................",
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo..t..",
        "....odghhdmoFfo.",
        "....odgmmewwfFo.",
        "ooo.odgwwwwwpo..",
        "pPooddhhwwwto...",
        ".aaodggwwwoa....",
        "..aaopoopo.aa...",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (6, 5): (OPOSSUM_PAL, [
        "................",
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo..t..",
        "....odghhdmofFo.",
        "....odgmmewwFfo.",
        ".oo.odgwwwwwpo..",
        "opPoodhhwwwto...",
        ".aaodggwwwoaaa..",
        "aa..oopoop......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (7, 5): (OPOSSUM_PAL, [
        "................",
        "................",
        "................",
        ".....oo..oo.....",
        ".....opoopo..t..",
        "....odghhdmoFfo.",
        "....odgmmewwFFo.",
        "ooo.odgwwwwwpo..",
        "pPooddhhwwwto...",
        "aaaodggwwwo..a..",
        "...oopoopo..a...",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),

    # =========================================================================
    # El Castor (cols 8..11, filas 0..5)
    # =========================================================================
    # --- Fila 0: Idle (8..11) ---
    (8, 0): (BEAVER_PAL, [
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        "..orRrrrrrro....",
        ".okKkodbwwbom...",
        ".oKkKodwwbomLo..",
        ".okKkodbbbomlo..",
        "..oookodddoom...",
        ".....okooko.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),
    (9, 0): (BEAVER_PAL, [
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        "..orRrrrrrro....",
        ".oKkKodbwwbom...",
        ".okKkodwwbomlo..",
        ".oKkKodbbbomLo..",
        "..oookodddoom...",
        ".....okooko.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),
    (10, 0): (BEAVER_PAL, [
        "................",
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        "..orRrrrrrrom...",
        ".okKkodbwwbomLo.",
        ".oKkKodwwbomLo..",
        "..oookodddoom...",
        ".....okooko.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),
    (11, 0): (BEAVER_PAL, [
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        "..orRrrrrrro....",
        ".okKkodbwwbom...",
        ".oKkKodwwbomLLo.",
        ".okKkodbbbomllo.",
        "..oookodddoom...",
        ".....okooko.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),

    # --- Fila 1: Walk (8..11) — paso firme moviendo cola plana ---
    (8, 1): (BEAVER_PAL, [
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        ".oorRrrrrrro....",
        "okKk.odbwwbom...",
        "oKkKoodwwbomLo..",
        ".ookkodbbbomlo..",
        "....ookdddoom...",
        "....oko..oko....",
        "....ooo..ooo....",
        "................",
        "................",
    ]),
    (9, 1): (BEAVER_PAL, [
        "................",
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        "..orRrrrrrrom...",
        ".okKkodbwwbomLo.",
        ".oKkKodwwbomLo..",
        "..oookodddoom...",
        ".....okooko.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),
    (10, 1): (BEAVER_PAL, [
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        "..orRrrrrrro....",
        ".okKkodbwwbo.m..",
        ".oKkKodwwbo.mLo.",
        ".okKkodbbbo.mlo.",
        "..oookodddo..m..",
        "....okooooko....",
        "....ooo..ooo....",
        "................",
        "................",
    ]),
    (11, 1): (BEAVER_PAL, [
        "................",
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo.....",
        "...odbhbbebbo...",
        "...odbbbwwwwo...",
        "...odbbbozzoo...",
        "..orRrrrrrrom...",
        ".oKkKodbwwbomlo.",
        ".okKkodwwbomLo..",
        "..oookodddoom...",
        ".....okooko.....",
        ".....oooooo.....",
        "................",
        "................",
    ]),

    # --- Fila 2: Jump/Fall (8..9 ascenso, 10..11 descenso) ---
    (8, 2): (BEAVER_PAL, [
        "................",
        ".....oo.oo......",
        "....odhhbdo.m...",
        "...odbhbbebbmLo.",
        "...odbbbwwwwmlo.",
        "...odbbbozzoom..",
        "..orRrrrrrro....",
        "...odbwwbbo.....",
        "..oodwwbbo......",
        ".okKkdddo.......",
        ".oKkKokooko.....",
        "..ooo.oo.oo.....",
        "................",
        "................",
        "................",
        "................",
    ]),
    (9, 2): (BEAVER_PAL, [
        "................",
        ".....oo.oo......",
        "....odhhbdo.m...",
        "...odbhbbebbmLo.",
        "...odbbbwwwwmLo.",
        "...odbbbozzoom..",
        "..orRrrrrrro....",
        "..oodbwwbbo.....",
        ".okKkdwwbbo.....",
        ".oKkKodddo......",
        "..oookooko......",
        ".....oo.oo......",
        "................",
        "................",
        "................",
        "................",
    ]),
    (10, 2): (BEAVER_PAL, [
        "................",
        "................",
        "....oo.oo.......",
        "...odhhbdo......",
        "..odbhbbebbo.m..",
        "..odbbbwwwwomLo.",
        ".orRrbbozzoomlo.",
        ".okKkorrrrrrom..",
        ".oKkKodbwwbo....",
        ".okKkodddbo.....",
        "..oookooko......",
        "....oko.oko.....",
        "................",
        "................",
        "................",
        "................",
    ]),
    (11, 2): (BEAVER_PAL, [
        "................",
        "................",
        "....oo.oo.......",
        "...odhhbdo......",
        "..odbhbbebbo.m..",
        "..odbbbwwwwomLo.",
        ".orRrbbozzoomLo.",
        "okKk.orrrrrrom..",
        "oKkKoodbwwbo....",
        ".ookkodddbo.....",
        ".....okooko.....",
        "....oko..oko....",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 3: DoubleJump (8..11) — alas doradas-cian brillantes ---
    (8, 3): (BEAVER_PAL, [
        "................",
        ".cc..oo.oo......",
        ".gCCodhhbdo.m...",
        "..ggdbhbbebbmLo.",
        "..godbbbwwwwmlo.",
        "...godbbbozzom..",
        "...orRrrrrro....",
        "..oodbwwbbo.....",
        ".okKkdddbbo.....",
        "..oookooko......",
        ".....ok.oko.....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (9, 3): (BEAVER_PAL, [
        ".c..............",
        "ccg..oo.oo......",
        "gggCodhhbdo.m...",
        ".gCgdbhbbebbmLo.",
        ".gCgodbbbwwwwmLo",
        "..ggodbbbozzom..",
        "...orRrrrrro....",
        "..oodbwwbbo.....",
        ".okKkdddbbo.....",
        "..oookooko......",
        ".....okook......",
        ".....g.c........",
        "................",
        "................",
        "................",
        "................",
    ]),
    (10, 3): (BEAVER_PAL, [
        "................",
        ".gc..oo.oo......",
        "..Cgodhhbdo.m...",
        "..ggdbhbbebbmLo.",
        "...godbbbwwwwmlo",
        "....odbbbozzom..",
        "...orRrrrrro....",
        "..oodbwwbbo.....",
        ".okKkdddbbo.....",
        "..oookooko......",
        "...g.ok.c.......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (11, 3): (BEAVER_PAL, [
        "................",
        ".....oo.oo......",
        "..g..odhhbdo.m..",
        "...godbbbebbmLo.",
        "...godbbbwwwwmlo",
        "....odbbbozzom..",
        "...orRrrrrro....",
        "..oodbwwbbo.....",
        ".okKkdddbbo.....",
        "..oookooko......",
        "....g...c.......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 4: Climb (8..11) ---
    (8, 4): (BEAVER_PAL, [
        ".......om.......",
        ".....oo.oo......",
        "....odhhbdo.....",
        "....odbhbebo....",
        "....odbbbzzwo...",
        "...morRrrrrro...",
        "..mLoodbwwbdoo..",
        "..mloodbbbdoko..",
        "..okKkdddbbo....",
        "..oKkKookoko....",
        "...ooo.ok..ok...",
        ".......ok...o...",
        "................",
        "................",
        "................",
        "................",
    ]),
    (9, 4): (BEAVER_PAL, [
        "......m..m......",
        ".....oo.oo......",
        "....odhhbdo.....",
        "....odbhbebo....",
        "....odbbbzzwo...",
        "....orRrrrrro...",
        "...oodbwwbdoo...",
        "...oodbbbdoko...",
        "..okKkdddbmo....",
        "..oKkKookmLo....",
        "...ooo.ok.km....",
        ".......ok.ok....",
        "................",
        "................",
        "................",
        "................",
    ]),
    (10, 4): (BEAVER_PAL, [
        ".......mo.......",
        ".....oo.oo......",
        "....odhhbdo.....",
        "....odbhbebo....",
        "....odbbbzzwo...",
        "....orRrrrrrm...",
        "...oodbwwbdoolo.",
        "...ookdbbdomLoo.",
        "..okKkdddbbo.m..",
        "..oKkKookoko....",
        "...ooo.oko.ok...",
        ".......ok..ok...",
        "................",
        "................",
        "................",
        "................",
    ]),
    (11, 4): (BEAVER_PAL, [
        "......m..m......",
        ".....oo.oo......",
        "....odhhbdo.....",
        "....odbhbebo....",
        "....odbbbzzwo...",
        "....orRrrrrro...",
        "...oodbwwbdoo...",
        "...oodbbbdoko...",
        "..okKkdddbmo....",
        "..oKkKookmlo....",
        "...ooo.ok.km....",
        ".......ok.ok....",
        "................",
        "................",
        "................",
        "................",
    ]),

    # --- Fila 5: Swim (8..11) — golpe de cola en el agua ---
    (8, 5): (BEAVER_PAL, [
        "................",
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo..m..",
        "...odbhbbebbmLo.",
        "...odbbbwwwwmlo.",
        ".okKkodbozzoom..",
        "okKkoorrrrrro...",
        "aaokKodbwwboaa..",
        ".aaookookooa....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (9, 5): (BEAVER_PAL, [
        "................",
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo..m..",
        "...odbhbbebbmLo.",
        ".okKkodbbbwwmLo.",
        "oKkKoodbozzoom..",
        ".ooo.orrrrrro...",
        ".aaookdbwwboa...",
        "..aaokooko.aa...",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (10, 5): (BEAVER_PAL, [
        "................",
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo..m..",
        "...odbhbbebbmLo.",
        "...odbbbwwwwmlo.",
        ".oKkKodbozzoom..",
        "okKkoorrrrrro...",
        ".aaokKdbwwboaaa.",
        "aa..ookook......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
    (11, 5): (BEAVER_PAL, [
        "................",
        "................",
        "................",
        ".....oo.oo......",
        "....odhhbdo..m..",
        "...odbhbbebbmLo.",
        ".oKkKodbbbwwmLo.",
        "okKkoodbozzoom..",
        ".aaa.orrrrrro...",
        "aaaookdbwwbo.a..",
        "...ookooko..a...",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]),
}


def build() -> bytearray:
    px = bytearray(ATLAS_W * ATLAS_H * 4)
    for (col, row), (pal, grid) in FRAMES.items():
        if not (0 <= col < GRID_W and 0 <= row < GRID_H):
            raise ValueError(f"celda fuera de rango: ({col}, {row})")
        if len(grid) != CELL:
            raise ValueError(f"celda ({col}, {row}) tiene {len(grid)} filas (se esperaban {CELL})")
        ox, oy = col * CELL, row * CELL
        for y, line in enumerate(grid):
            if len(line) != CELL:
                raise ValueError(f"celda ({col}, {row}) fila {y} mide {len(line)} (se esperaba {CELL})")
            for x, ch in enumerate(line):
                rgba = pal[ch]
                i = ((oy + y) * ATLAS_W + (ox + x)) * 4
                px[i : i + 4] = bytes(rgba)
    return px


def write_png(path: Path, rgba: bytes, w: int = ATLAS_W, h: int = ATLAS_H) -> None:
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


def write_ppm(path: Path, rgba: bytes, w: int = ATLAS_W, h: int = ATLAS_H) -> None:
    body = bytearray()
    for i in range(0, len(rgba), 4):
        if rgba[i + 3] == 0:
            body += bytes((12, 14, 24))
        else:
            body += rgba[i : i + 3]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(f"P6\n{w} {h}\n255\n".encode("ascii") + body)


def verify_cells(rgba: bytes) -> None:
    def cell_bytes(col: int, row: int) -> bytes:
        ox, oy = col * CELL, row * CELL
        out = bytearray()
        for y in range(CELL):
            off = ((oy + y) * ATLAS_W + ox) * 4
            out += rgba[off : off + CELL * 4]
        return bytes(out)

    def check_group(row: int, c0: int, c1: int) -> None:
        uniq: set[bytes] = set()
        for col in range(c0, c1):
            c = cell_bytes(col, row)
            for cx, cy in ((0, 0), (CELL - 1, 0), (0, CELL - 1), (CELL - 1, CELL - 1)):
                if c[(cy * CELL + cx) * 4 + 3] != 0:
                    raise ValueError(f"celda ({col}, {row}) esquina ({cx},{cy}) no es transparente")
            opaque = sum(1 for i in range(3, len(c), 4) if c[i] > 0)
            if opaque < 20:
                raise ValueError(f"celda ({col}, {row}) tiene solo {opaque} px opacos (< 20)")
            uniq.add(c)
        if len(uniq) != (c1 - c0):
            raise ValueError(f"fila {row} cols {c0}..{c1 - 1} tiene cuadros repetidos")

    for row in range(6):
        check_group(row, 0, 4)   # Sr. del Farol
        check_group(row, 4, 8)   # La Zarigüeya
        check_group(row, 8, 12)  # El Castor
    check_group(6, 0, 4)
    check_group(6, 4, 8)
    check_group(7, 0, 4)
    check_group(7, 4, 8)


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
    verify_cells(rgba)
    if args.output:
        write_png(args.output, rgba)
        print(f"gen_sprites: {args.output} ({ATLAS_W}x{ATLAS_H}, {len(FRAMES)} sprites)")
    if args.ppm:
        write_ppm(args.ppm, rgba)
        print(f"gen_sprites: {args.ppm}")
    if args.check:
        w, h, got = read_png_rgba(args.check)
        if (w, h) != (ATLAS_W, ATLAS_H):
            print(f"FAIL tamaño {w}x{h}, se esperaba {ATLAS_W}x{ATLAS_H}", file=sys.stderr)
            return 1
        if got != rgba:
            diffs = sum(1 for a, b in zip(got, rgba) if a != b)
            print(f"FAIL {args.check} difiere del generador ({diffs} bytes)", file=sys.stderr)
            return 1
        print(f"gen_sprites: {args.check} OK (pixels = generador)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
