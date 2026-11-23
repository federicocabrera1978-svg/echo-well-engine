#!/usr/bin/env python3
"""Genera el mapa unificado Metroidvania 8x5 `assets/maps/src/world.yaml`.

Uso:
  python3 tools/gen_world.py -o assets/maps/src/world.yaml
  python3 tools/gen_world.py --check assets/maps/src/world.yaml

Estructura 8x5 chunks (320x115 tiles = 2560x920 px, 40 pantallas):
  - Fila cy=0 (Zona 1: El Pozo, cx=0..7): recorrido de slice.yaml sin `goal`, con el
    atajo de Zona 3 en la alcoba de (1,0), el eje del ascensor de Zona 2 en (6,0), y
    el pozo de caída tras boss_pozo en (7,0) hacia la pileta de (7,1).
  - Zona 2 (Las Catacumbas, cx=4..7, cy=1..2): espejo horizontal de level2.yaml con
    sw_elev_cat + elev_cat_pozo en (6,1)->(6,0) y owd_wyrm_cat_l en (3,2)<->(4,2).
  - Zona 3 (La Guarida del Wyrm, cx=0..3, cy=1..2): progresión derecha->izquierda
    desde door_boss2 (x=1280) hasta el jefe final `wyrm` y `goal` en (0,1), además del
    atajo directo elev_wyrm_pozo + owd_wyrm_pozo en (1,1)<->(1,0).
  - Zona 4 (Archivo Sumergido, cy=3, cx=0..7): corredor acuático bajo la Guarida,
    con el pozo D1 desde el Santuario (1,2), piletas, checkpoints y lore.
  - Zona 5 (Torre de los Ecos, cy=4, cx=0..7): campanario profundo bajo el Archivo,
    con el pozo D2 desde (7,3), checkpoint y lore.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path


def r(layer: str, x: int, y: int, w: int, h: int, tile: int) -> str:
    return f"  - {{layer: {layer:4s}, x: {x:<3d}, y: {y:<2d}, w: {w:<3d}, h: {h:<2d}, tile: {tile}}}"


def ent(name: str, etype: str, x: int, y: int, extra: str = "") -> str:
    tail = f", {extra}" if extra else ""
    return f"  - {{name: {name}, type: {etype}, x: {x}, y: {y}{tail}}}"


def build_world_yaml() -> str:
    rects: list[str] = []
    entities: list[str] = []
    links: list[str] = []
    ropes: list[str] = []
    water: list[str] = []
    backdrops: list[str] = []
    dialogs: list[str] = []
    guides: list[str] = []

    # --- Estructura global 8x5 (320x115 tiles) ---
    rects += [
        "# Estructura general 8x5 (320x115 tiles = 2560x920 px)",
        r("COL", 0, 20, 320, 6, 1),
        r("VIS", 0, 20, 320, 3, 17),
        r("VIS", 0, 23, 320, 3, 15),
        r("COL", 0, 43, 320, 6, 1),
        r("VIS", 0, 43, 320, 3, 17),
        r("VIS", 0, 46, 320, 3, 15),
        r("COL", 0, 66, 320, 3, 1),
        r("VIS", 0, 66, 320, 3, 17),
        # Zonas nuevas: Archivo Sumergido (cy=3) y Torre de los Ecos (cy=4)
        r("COL", 0, 89, 320, 6, 1),
        r("VIS", 0, 89, 320, 3, 17),
        r("VIS", 0, 92, 320, 3, 15),
        r("COL", 0, 112, 320, 3, 1),
        r("VIS", 0, 112, 320, 3, 17),
        r("COL", 0, 0, 1, 115, 1),
        r("VIS", 0, 0, 1, 115, 17),
        r("COL", 319, 0, 1, 115, 1),
        r("VIS", 319, 0, 1, 115, 17),
    ]

    # --- Zona 1 (cy = 0, cx = 0..7): El Pozo (slice con conectores de mundo) ---
    rects += [
        "",
        "  # --- Zona 1 (cy=0): El Pozo ---",
        # Paredes entre salas 3/4, 4/5 y 5/6
        r("COL", 279, 0, 1, 17, 1),
        r("VIS", 279, 0, 1, 17, 17),
        r("COL", 239, 0, 1, 17, 1),
        r("VIS", 239, 0, 1, 17, 17),
        r("COL", 199, 8, 1, 15, 1),
        r("VIS", 199, 8, 1, 15, 17),
        # Chunk (0,0): pileta inicial y cámara secreta superior (chimenea x=128..152)
        r("META", 29, 17, 8, 3, 2),
        r("COL", 28, 17, 1, 3, 1),
        r("VIS", 28, 17, 1, 3, 17),
        r("COL", 37, 17, 1, 3, 1),
        r("VIS", 37, 17, 1, 3, 17),
        r("COL", 4, 1, 16, 1, 1),
        r("VIS", 4, 1, 16, 1, 15),
        r("COL", 4, 2, 1, 4, 1),
        r("VIS", 4, 2, 1, 4, 15),
        r("COL", 4, 6, 12, 1, 3),
        r("VIS", 4, 6, 12, 1, 5),
        r("COL", 15, 7, 1, 9, 1),
        r("VIS", 15, 7, 1, 9, 15),
        r("COL", 19, 1, 1, 15, 1),
        r("VIS", 19, 1, 1, 15, 15),
        # Chunk (1,0): pared alta con atajo inferior owd_wyrm_pozo (tx=51, ty=17..19) y eje elev_wyrm_pozo (tx=60..62)
        r("COL", 50, 8, 2, 9, 1),
        r("VIS", 50, 8, 2, 9, 17),
        r("COL", 52, 8, 49, 1, 3),
        r("VIS", 52, 8, 49, 1, 5),
        r("COL", 60, 20, 3, 6, 0),
        r("VIS", 60, 20, 3, 6, 0),
        r("VIS", 44, 4, 2, 16, 9),
        r("VIS", 108, 0, 2, 8, 21),
        r("FG", 104, 0, 4, 6, 13),
        # Chunk (2,0): hueco y foso de pinchos
        r("COL", 78, 8, 4, 1, 0),
        r("VIS", 78, 8, 4, 1, 0),
        r("COL", 76, 9, 1, 11, 1),
        r("VIS", 76, 9, 1, 11, 17),
        r("COL", 92, 9, 1, 11, 1),
        r("VIS", 92, 9, 1, 11, 17),
        r("META", 76, 19, 17, 2, 4),
        r("VIS", 77, 20, 15, 1, 11),
        # Chunk (3,0): cisterna inundable
        r("COL", 100, 9, 1, 11, 1),
        r("VIS", 100, 9, 1, 11, 17),
        r("COL", 141, 9, 1, 5, 1),
        r("VIS", 141, 9, 1, 5, 17),
        r("META", 101, 17, 40, 3, 2),
        r("COL", 142, 8, 24, 1, 3),
        r("VIS", 142, 8, 24, 1, 5),
        # Chunk (4,0): sala del huevo del pozo
        r("COL", 170, 8, 2, 12, 1),
        r("VIS", 170, 8, 2, 12, 17),
        r("COL", 172, 8, 27, 1, 3),
        r("VIS", 172, 8, 27, 1, 5),
        r("VIS", 184, 9, 2, 11, 9),
        # Chunk (5,0): plataformas móviles
        r("COL", 200, 8, 6, 1, 3),
        r("VIS", 200, 8, 6, 1, 5),
        r("COL", 223, 8, 3, 12, 1),
        r("VIS", 223, 8, 3, 12, 17),
        r("META", 200, 19, 29, 2, 4),
        r("VIS", 200, 20, 23, 1, 11),
        r("VIS", 226, 20, 3, 1, 11),
        # Chunk (6,0): gravedad invertida + eje de elev_cat_pozo (tx=241..243) antes de flip_up
        r("COL", 240, 0, 16, 3, 1),
        r("VIS", 240, 0, 16, 3, 15),
        r("COL", 268, 0, 11, 3, 1),
        r("VIS", 268, 0, 11, 3, 15),
        r("META", 256, 0, 12, 3, 4),
        r("COL", 241, 20, 3, 6, 0),
        r("VIS", 241, 20, 3, 6, 0),
        r("META", 249, 19, 22, 2, 4),
        r("VIS", 249, 20, 22, 1, 11),
        # Chunk (7,0): arena boss_pozo y caída a (7,1) tras door_boss1 (tx=311..314)
        r("COL", 310, 0, 1, 17, 1),
        r("VIS", 310, 0, 1, 17, 15),
        r("VIS", 286, 4, 2, 16, 9),
        r("VIS", 304, 4, 2, 16, 9),
        r("COL", 311, 20, 4, 6, 0),
        r("VIS", 311, 20, 4, 6, 0),
        r("COL", 315, 0, 4, 43, 1),
        r("VIS", 315, 0, 4, 43, 17),
    ]

    # --- Zona 2 (cx = 4..7, cy = 1..2): Las Catacumbas (espejo de level2) ---
    rects += [
        "",
        "  # --- Zona 2 (cx=4..7, cy=1..2): Las Catacumbas ---",
        # Chunk (7,1): Entrada Catacumbas + pileta bajo boss_pozo + repisa egg_cat_1
        r("COL", 308, 40, 1, 3, 1),
        r("VIS", 308, 40, 1, 3, 17),
        r("META", 309, 40, 6, 3, 2),
        r("COL", 292, 35, 12, 1, 3),
        r("VIS", 292, 35, 12, 1, 5),
        r("VIS", 295, 26, 2, 9, 9),
        r("VIS", 300, 26, 2, 9, 9),
        r("COL", 279, 26, 1, 14, 1),
        r("VIS", 279, 26, 1, 14, 15),
        # Chunk (6,1): Bóveda Central / Hub + pozo a (6,2) + eje elev_cat_pozo (tx=241..243)
        r("COL", 259, 43, 10, 6, 0),
        r("VIS", 259, 43, 10, 6, 0),
        r("COL", 255, 35, 1, 5, 1),
        r("VIS", 255, 35, 1, 5, 15),
        r("COL", 244, 35, 12, 1, 3),
        r("VIS", 244, 35, 12, 1, 5),
        r("COL", 240, 35, 1, 1, 3),
        r("VIS", 240, 35, 1, 1, 5),
        r("COL", 240, 26, 1, 6, 1),
        r("VIS", 240, 26, 1, 6, 15),
        r("COL", 240, 35, 1, 8, 1),
        r("VIS", 240, 35, 1, 8, 15),
        # Chunk (6,2): El Foso + pileta de agua + escalones de retorno con Doble Salto
        r("COL", 258, 63, 1, 3, 1),
        r("VIS", 258, 63, 1, 3, 17),
        r("COL", 269, 63, 1, 3, 1),
        r("VIS", 269, 63, 1, 3, 17),
        r("META", 259, 63, 10, 3, 2),
        r("COL", 240, 58, 18, 1, 3),
        r("VIS", 240, 58, 18, 1, 5),
        r("COL", 259, 51, 6, 1, 3),
        r("VIS", 259, 51, 6, 1, 5),
        # Chunk (7,2): Cisterna del Altar (rel_double_jump + cuerda sobre pinchos)
        r("COL", 279, 49, 1, 14, 1),
        r("VIS", 279, 49, 1, 14, 15),
        r("COL", 295, 66, 3, 1, 0),
        r("VIS", 295, 66, 3, 1, 0),
        r("META", 295, 66, 3, 2, 4),
        r("VIS", 295, 67, 3, 1, 11),
        r("VIS", 310, 51, 2, 15, 9),
        # Chunk (5,2): Galería Subterránea (plat_crypt2 sobre pinchos)
        r("COL", 240, 49, 1, 6, 1),
        r("VIS", 240, 49, 1, 6, 15),
        r("COL", 240, 58, 1, 8, 1),
        r("VIS", 240, 58, 1, 8, 15),
        r("COL", 233, 58, 7, 8, 1),
        r("VIS", 233, 58, 7, 8, 17),
        r("COL", 200, 58, 8, 8, 1),
        r("VIS", 200, 58, 8, 8, 17),
        r("META", 208, 65, 25, 2, 4),
        r("VIS", 208, 66, 25, 1, 11),
        r("COL", 200, 49, 1, 6, 1),
        r("VIS", 200, 49, 1, 6, 15),
        # Chunk (4,2): Santuario Profundo (sw_sanctum2, egg_cat_2, rel_dash) + owd_wyrm_cat_l en tx=160
        r("COL", 193, 58, 7, 8, 1),
        r("VIS", 193, 58, 7, 8, 17),
        r("COL", 190, 62, 3, 4, 1),
        r("VIS", 190, 62, 3, 4, 17),
        r("COL", 166, 58, 16, 1, 3),
        r("VIS", 166, 58, 16, 1, 5),
        r("VIS", 173, 59, 2, 7, 9),
        r("COL", 160, 49, 1, 14, 1),
        r("VIS", 160, 49, 1, 14, 15),
        # Chunk (5,1): Galería Invertida
        r("COL", 234, 35, 6, 1, 3),
        r("VIS", 234, 35, 6, 1, 5),
        r("META", 210, 42, 14, 2, 4),
        r("VIS", 210, 43, 14, 1, 11),
        r("COL", 200, 26, 1, 14, 1),
        r("VIS", 200, 26, 1, 14, 15),
        # Chunk (4,1): Cámara del Guardián (boss_catacomb -> door_boss2 en x=1280)
        r("VIS", 188, 27, 2, 16, 9),
        r("VIS", 172, 27, 2, 16, 9),
        r("COL", 160, 26, 1, 14, 1),
        r("VIS", 160, 26, 1, 14, 15),
    ]

    # --- Zona 3 (cx = 0..3, cy = 1..2): La Guarida del Wyrm ---
    rects += [
        "",
        "  # --- Zona 3 (cx=0..3, cy=1..2): La Guarida del Wyrm ---",
        # Chunk (3,1): Umbral del Wyrm (wall_entry3, ck_abismo3, egg_wyrm_1, pozo a (3,2))
        r("COL", 148, 26, 1, 14, 1),
        r("VIS", 148, 26, 1, 14, 15),
        r("COL", 140, 43, 6, 1, 0),
        r("VIS", 140, 43, 6, 1, 0),
        r("META", 140, 43, 6, 2, 4),
        r("VIS", 140, 44, 6, 1, 11),
        r("COL", 128, 35, 8, 1, 3),
        r("VIS", 128, 35, 8, 1, 5),
        r("COL", 124, 43, 4, 6, 0),
        r("VIS", 124, 43, 4, 6, 0),
        r("COL", 119, 26, 1, 14, 1),
        r("VIS", 119, 26, 1, 14, 15),
        # Chunk (3,2): Corredor inferior + pileta bajo (3,1) + escalones retorno + owd_wyrm_cat_l
        r("COL", 122, 63, 1, 3, 1),
        r("VIS", 122, 63, 1, 3, 17),
        r("COL", 129, 63, 1, 3, 1),
        r("VIS", 129, 63, 1, 3, 17),
        r("META", 123, 63, 6, 3, 2),
        r("COL", 120, 58, 3, 1, 3),
        r("VIS", 120, 58, 3, 1, 5),
        r("COL", 129, 51, 5, 1, 3),
        r("VIS", 129, 51, 5, 1, 5),
        r("COL", 119, 49, 1, 14, 1),
        r("VIS", 119, 49, 1, 14, 15),
        # Chunk (2,1): Abismo & Puentes (plat_wyrm sobre pinchos, pozo a (2,2), door_wyrm_seal)
        r("COL", 97, 43, 15, 1, 0),
        r("VIS", 97, 43, 15, 1, 0),
        r("META", 97, 43, 15, 2, 4),
        r("VIS", 97, 44, 15, 1, 11),
        r("COL", 91, 38, 5, 1, 3),
        r("VIS", 91, 38, 5, 1, 5),
        r("COL", 85, 43, 6, 6, 0),
        r("VIS", 85, 43, 6, 6, 0),
        r("COL", 80, 35, 5, 1, 3),
        r("VIS", 80, 35, 5, 1, 5),
        r("COL", 80, 26, 1, 6, 1),
        r("VIS", 80, 26, 1, 6, 15),
        r("COL", 80, 35, 1, 8, 1),
        r("VIS", 80, 35, 1, 8, 15),
        # Chunk (2,2): Pozo Invertido (pileta + escalones retorno + pinchos invertidos + wall_secret3)
        r("COL", 82, 63, 1, 3, 1),
        r("VIS", 82, 63, 1, 3, 17),
        r("COL", 91, 63, 1, 3, 1),
        r("VIS", 91, 63, 1, 3, 17),
        r("META", 83, 63, 8, 3, 2),
        r("COL", 81, 58, 4, 1, 3),
        r("VIS", 81, 58, 4, 1, 5),
        r("COL", 88, 51, 4, 1, 3),
        r("VIS", 88, 51, 4, 1, 5),
        r("META", 96, 65, 15, 2, 4),
        r("VIS", 96, 66, 15, 1, 11),
        r("COL", 80, 49, 1, 14, 1),
        r("VIS", 80, 49, 1, 14, 15),
        # Chunk (1,2): Santuario del Wyrm (imp_santuario, sw_seal3, lore_wyrm, egg_wyrm_2)
        r("COL", 43, 58, 13, 1, 3),
        r("VIS", 43, 58, 13, 1, 5),
        r("VIS", 48, 59, 2, 7, 9),
        r("COL", 40, 49, 1, 14, 1),
        r("VIS", 40, 49, 1, 14, 15),
        # Chunk (0,2): Cámara lateral / puente antiguo sobre abismo de pinchos
        r("COL", 10, 66, 18, 1, 0),
        r("VIS", 10, 66, 18, 1, 0),
        r("META", 10, 66, 18, 2, 4),
        r("VIS", 10, 67, 18, 1, 11),
        r("COL", 10, 62, 18, 1, 3),
        r("VIS", 10, 62, 18, 1, 5),
        r("VIS", 14, 63, 2, 4, 9),
        r("VIS", 22, 63, 2, 4, 9),
        # Chunk (1,1): Atajo directo a Zona 1 (repisa desde door_wyrm_seal + elev_wyrm_pozo)
        r("COL", 74, 35, 6, 1, 3),
        r("VIS", 74, 35, 6, 1, 5),
        r("COL", 40, 26, 1, 14, 1),
        r("VIS", 40, 26, 1, 14, 15),
        # Chunk (0,1): Arena del Wyrm + door_wyrm_exit + goal
        r("COL", 34, 36, 5, 1, 3),
        r("VIS", 34, 36, 5, 1, 5),
        r("VIS", 14, 27, 2, 16, 9),
        r("VIS", 29, 27, 2, 16, 9),
        r("COL", 7, 26, 1, 14, 1),
        r("VIS", 7, 26, 1, 14, 15),
        r("META", 2, 40, 4, 3, 3),
    ]

    # --- Zona 4 (cy=3): Archivo Sumergido + Zona 5 (cy=4): Torre de los Ecos ---
    rects += [
        "",
        "  # --- Zonas nuevas (cy=3 Aqueduct, cy=4 Tower) ---",
        # Pozo D1: descenso (1,2) -> (1,3) por el piso del Santuario (tiles 52..54, y 66..68)
        r("COL", 52, 66, 3, 3, 0),
        r("VIS", 52, 66, 3, 3, 0),
        # Pozo D2: descenso (7,3) -> (7,4) (tiles 284..286, y 89..94)
        r("COL", 284, 89, 3, 6, 0),
        r("VIS", 284, 89, 3, 6, 0),
        # Pileta W1 (entrada del Archivo, bajo D1): tiles 52..60 + muros 51/61
        r("META", 52, 86, 9, 3, 2),
        r("COL", 51, 86, 1, 3, 1),
        r("VIS", 51, 86, 1, 3, 17),
        r("COL", 61, 86, 1, 3, 1),
        r("VIS", 61, 86, 1, 3, 17),
        # Pileta W2 (decorado de la presa): tiles 204..209 + muros 203/210
        r("META", 204, 86, 6, 3, 2),
        r("COL", 203, 86, 1, 3, 1),
        r("VIS", 203, 86, 1, 3, 17),
        r("COL", 210, 86, 1, 3, 1),
        r("VIS", 210, 86, 1, 3, 17),
    ]

    # --- Entidades ---
    entities += [
        "# Zona 1 (El Pozo)",
        ent("spawn", "player_spawn", 32, 152),
        ent("npc_vigia", "npc", 64, 152),
        ent("switch_start", "switch", 96, 152),
        ent("egg_pozo_secreto", "collectible", 56, 40),
        ent("lore_techo", "lore", 96, 40),
        ent("door_start", "door", 200, 136),
        ent("owd_wyrm_pozo", "one_way_door", 408, 136),
        ent("ck_ledge", "checkpoint", 536, 56),
        ent("flood", "flood_switch", 776, 56),
        ent("ck_exit", "checkpoint", 1152, 56),
        ent("switch_well", "switch", 1112, 136),
        ent("door_well_hi", "door", 1128, 112),
        ent("door_well_lo", "door", 1128, 136),
        ent("egg_well", "collectible", 1440, 56),
        ent("ck_mover", "checkpoint", 1620, 56),
        ent("plat_h", "mover", 1648, 64, "w: 3, h: 1, to_x: 1760, to_y: 64, speed: 48, pause: 0.6, tile: 6"),
        ent("lift_v", "mover", 1808, 64, "w: 3, h: 1, to_x: 1808, to_y: 136, speed: 40, pause: 0.8, tile: 6"),
        ent("ck_grav", "checkpoint", 1964, 152),
        ent("flip_up", "gravity_flip", 1984, 152),
        ent("plat_inv", "mover", 2048, 16, "w: 3, h: 1, to_x: 2120, to_y: 16, speed: 40, pause: 0.6, tile: 6"),
        ent("flip_down", "gravity_flip", 2176, 28),
        ent("ck_boss", "checkpoint", 2264, 152),
        ent("boss_pozo", "boss", 2400, 160),
        ent("door_boss1", "door", 2480, 136),
        "",
        "  # Zona 2 (Las Catacumbas)",
        ent("ck_cat_drop", "checkpoint", 2456, 336),
        ent("npc_cronista", "npc", 2424, 336),
        ent("egg_cat_1", "collectible", 2384, 272),
        ent("ck_hub2", "checkpoint", 2192, 336),
        ent("sw_elev_cat", "switch", 1964, 336),
        ent("elev_cat_pozo", "elevator", 1928, 160, "w: 3, h: 1, to_x: 1928, to_y: 328, speed: 56, pause: 0.5, tile: 6"),
        ent("door_hub2", "door", 1920, 256),
        ent("ck_pit2", "checkpoint", 2176, 520),
        ent("ck_altar2", "checkpoint", 2272, 520),
        ent("rel_double_jump", "ability_double_jump", 2480, 520),
        ent("ck_east_low2", "checkpoint", 1888, 456),
        ent("plat_crypt2", "mover", 1832, 464, "w: 4, h: 1, to_x: 1664, to_y: 464, speed: 48, pause: 0.6, tile: 6"),
        ent("sw_sanctum2", "switch", 1472, 520),
        ent("lore_santuario2", "lore", 1436, 520),
        ent("egg_cat_2", "collectible", 1400, 456),
        ent("rel_dash", "ability_dash", 1352, 456),
        ent("owd_wyrm_cat_l", "one_way_door", 1280, 504),
        ent("ck_grav2", "checkpoint", 1848, 336),
        ent("flip_up2", "gravity_flip", 1816, 336),
        ent("flip_down2", "gravity_flip", 1656, 212),
        ent("wall_guardian", "break_wall", 1600, 320),
        ent("ck_boss2", "checkpoint", 1560, 336),
        ent("boss_catacomb", "boss", 1440, 344),
        ent("door_boss2", "door", 1280, 320),
        "",
        "  # Zona 3 (La Guarida del Wyrm)",
        ent("npc_centinela", "npc", 1244, 336),
        ent("wall_entry3", "break_wall", 1184, 320),
        ent("ck_abismo3", "checkpoint", 1096, 336),
        ent("egg_wyrm_1", "collectible", 1056, 272),
        ent("plat_wyrm", "mover", 872, 344, "w: 3, h: 1, to_x: 784, to_y: 344, speed: 44, pause: 0.5, tile: 6"),
        ent("door_wyrm_seal", "door", 640, 256),
        ent("ck_invertido3", "checkpoint", 928, 520),
        ent("flip_up3", "gravity_flip", 904, 520),
        ent("flip_down3", "gravity_flip", 752, 396),
        ent("wall_secret3", "break_wall", 640, 504),
        ent("ck_santuario3", "checkpoint", 616, 520),
        ent("imp_santuario", "imp", 560, 528),
        ent("sw_seal3", "switch", 504, 520),
        ent("lore_wyrm", "lore", 464, 520),
        ent("egg_wyrm_2", "collectible", 384, 456),
        ent("rel_wall_climb", "ability_wall_climb", 24, 520),
        ent("lore_abismo", "lore", 48, 520),
        ent("ck_atajo3", "checkpoint", 560, 336),
        ent("elev_wyrm_pozo", "elevator", 480, 328, "w: 3, h: 1, to_x: 480, to_y: 160, speed: 56, pause: 0.5, tile: 6"),
        ent("ck_wyrm_arena", "checkpoint", 300, 336),
        ent("wyrm", "boss_dragon", 176, 344),
        ent("door_wyrm_exit", "door", 56, 320),
        ent("goal", "goal", 32, 336),
        "",
        "  # Zona 4 (Archivo Sumergido) y Zona 5 (Torre de los Ecos)",
        ent("aq_ck_entrada", "checkpoint", 48, 704),
        ent("aq_lore_archivo", "lore", 800, 704),
        ent("aq_lore_presa", "lore", 1376, 704),
        ent("aq_ck_presa", "checkpoint", 1968, 704),
        ent("tw_lore_campana", "lore", 800, 888),
        ent("tw_ck", "checkpoint", 1376, 888),
    ]

    links += [
        "  - {from: switch_start,  to: door_start}",
        "  - {from: switch_well,   to: door_well_hi}",
        "  - {from: switch_well,   to: door_well_lo}",
        "  - {from: boss_pozo,     to: door_boss1}",
        "  - {from: sw_elev_cat,   to: elev_cat_pozo}",
        "  - {from: sw_sanctum2,   to: door_hub2}",
        "  - {from: boss_catacomb, to: door_boss2}",
        "  - {from: sw_seal3,      to: door_wyrm_seal}",
        "  - {from: wyrm,          to: door_wyrm_exit}",
    ]

    ropes += [
        "  - {x: 392,  y: 24,  segments: 16, seg_len: 6}",
        "  - {x: 1352, y: 24,  segments: 16, seg_len: 6}",
        "  - {x: 2372, y: 400, segments: 15, seg_len: 6}",
        "  - {x: 704,  y: 208, segments: 11, seg_len: 6}",
        "  - {x: 428,  y: 528, segments: 28, seg_len: 6}",
        "  - {x: 2284, y: 712, segments: 30, seg_len: 6}",
    ]

    water += [
        "  - {x: 232,  y: 136, w: 64,  h: 24, stiffness: 0.02, damping: 0.04}",
        "  - {x: 808,  y: 136, w: 320, h: 24, stiffness: 0.02, damping: 0.04}",
        "  - {x: 2472, y: 320, w: 48,  h: 24, stiffness: 0.02, damping: 0.04}",
        "  - {x: 2072, y: 504, w: 80,  h: 24, stiffness: 0.02, damping: 0.04}",
        "  - {x: 984,  y: 504, w: 48,  h: 24, stiffness: 0.02, damping: 0.04}",
        "  - {x: 664,  y: 504, w: 64,  h: 24, stiffness: 0.02, damping: 0.04}",
        "  - {x: 416,  y: 688, w: 72,  h: 24, stiffness: 0.02, damping: 0.04}",
        "  - {x: 1632, y: 688, w: 48,  h: 24, stiffness: 0.02, damping: 0.04}",
    ]

    bd_grid = [
        # cy = 0
        (0, 0, "pozo", 0),
        (1, 0, "raices", 7),
        (2, 0, "grieta", 14),
        (3, 0, "cisterna", 21),
        (4, 0, "nido", 28),
        (5, 0, "engranajes", 35),
        (6, 0, "invertido", 42),
        (7, 0, "arena", 49),
        # cy = 1
        (0, 1, "arena", 204),
        (1, 1, "raices", 203),
        (2, 1, "abismo", 202),
        (3, 1, "grieta", 201),
        (4, 1, "guarida", 104),
        (5, 1, "invertido", 103),
        (6, 1, "abismo", 102),
        (7, 1, "cripta", 101),
        # cy = 2
        (0, 2, "nido", 208),
        (1, 2, "santuario", 207),
        (2, 2, "invertido", 206),
        (3, 2, "cueva", 205),
        (4, 2, "santuario", 108),
        (5, 2, "engranajes", 107),
        (6, 2, "pozo", 106),
        (7, 2, "cisterna", 105),
        # cy = 3 (Archivo Sumergido / Aqueduct)
        (0, 3, "aqueduct", 300),
        (1, 3, "aqueduct", 301),
        (2, 3, "aqueduct", 302),
        (3, 3, "aqueduct", 303),
        (4, 3, "aqueduct", 304),
        (5, 3, "aqueduct", 305),
        (6, 3, "aqueduct", 306),
        (7, 3, "aqueduct", 307),
        # cy = 4 (Torre de los Ecos / Tower)
        (0, 4, "tower", 400),
        (1, 4, "tower", 401),
        (2, 4, "tower", 402),
        (3, 4, "tower", 403),
        (4, 4, "tower", 404),
        (5, 4, "tower", 405),
        (6, 4, "tower", 406),
        (7, 4, "tower", 407),
    ]
    for cx, cy, theme, seed in bd_grid:
        backdrops.append(f"  - {{cx: {cx}, cy: {cy}, theme: {theme}, seed: {seed}}}")

    dialogs += [
        '  - {entity: npc_vigia,        trigger: interact, speaker: "VIGIA DEL FAROL",             text: "BAJO EL POZO DUERMEN HUEVOS/QUE LOS HOMBRES CREEN PIEDRA.|SU CASCARA LATE TIBIA:/SON NIDADAS DE DRAGON.|TU FAROL LES DA CALOR./RECOGELAS Y SIGUE BAJANDO."}',
        '  - {entity: egg_pozo_secreto, trigger: pickup,   speaker: "ECO: HUEVO DE LA ALTURA",     text: "OCULTO EN EL TECHO DEL POZO,/SU CASCARA GUARDA ECOS VIEJOS.|SOLO QUIEN TREPA LOS MUROS/ALCANZA LA CUMBRE OLVIDADA."}',
        '  - {entity: lore_techo,       trigger: interact, speaker: "ESTELA DE LA BOVEDA",         text: "DESDE EL ABISMO DE ORIENTE/TRAJERON LAS GARRAS DE PIEDRA.|SUBE DONDE NACIO EL POZO/Y COMPLETA LA NIDADA."}',
        '  - {entity: egg_well,         trigger: pickup,   speaker: "ECO: HUEVO DE BRONCE",        text: "EL HUEVO DEL POZO RESPIRA./UNA BRASA ANTIGUA DESPIERTA|Y RECONOCE LA LUZ DE TU FAROL./NO SON JOYAS: SON DRAGONES."}',
        '  - {entity: npc_cronista,    trigger: interact, speaker: "CRONISTA DE LAS CATACUMBAS",  text: "AQUI SELLARON A LA SIERPE/Y ESCONDIERON SUS NIDADAS.|DESCIENDE AL FOSO POR LAS ALAS/ANTES DE SUBIR AL GUARDIAN."}',
        '  - {entity: egg_cat_1,       trigger: pickup,   speaker: "ECO: HUEVO DEL NIDO",         text: "LA NIDADA NO ERA UN TESORO:/ERA EL FUTURO DEL REINO.|LLEVA SU CALOR HACIA ARRIBA/PARA QUE DESPIERTE EL DRAGON."}',
        '  - {entity: rel_double_jump, trigger: pickup,   speaker: "RELIQUIA: ALAS DE DRAGON",    text: "ALAS DE DRAGON, AUN DE CRIA,/ABRAZAN TU CAPA EN EL AIRE.|PULSA SALTO EN PLENO VUELO/Y ALCANZA LAS REPISAS ALTAS."}',
        '  - {entity: lore_santuario2, trigger: interact, speaker: "ESTELA DEL SANTUARIO",        text: "CUANDO EL FUEGO SE APAGUE,/LOS HUEVOS DE DRAGON VOLVERAN.|CADA ESCAMA GUARDA EL PACTO/DE LOS ANTIGUOS GUARDIANES.|ACTIVA EL SELLO Y DESBLOQUEA/EL ASCENSOR DE RETORNO."}',
        '  - {entity: egg_cat_2,       trigger: pickup,   speaker: "ECO: HUEVO DE BRASA",         text: "SU CASCARA ARDE SIN QUEMAR:/LATEN ALAS BAJO LA PIEDRA.|FUE OCULTADO EN EL SANTUARIO/LEJOS DEL OJO DE LA BESTIA."}',
        '  - {entity: rel_dash,        trigger: pickup,   speaker: "RELIQUIA: GARRA DE BRASA",    text: "EL IMPETU DEL WYRM ARDE/EN TU EMBESTIDA!|PULSA E, X O K PARA EMBESTIR/Y ROMPER MUROS AGRIETADOS."}',
        '  - {entity: npc_centinela,   trigger: interact, speaker: "CENTINELA DEL FAROL",         text: "MAS ALLA AGUARDA EL WYRM,/GUARDIAN DEL ULTIMO SELLO.|USA TU EMBESTIDA EN LOS MUROS/Y SALTA SOBRE SUS LLAMAS.|TRAS SU GUARIDA UN ASCENSOR/SUBE DE VUELTA HACIA EL POZO."}',
        '  - {entity: egg_wyrm_1,      trigger: pickup,   speaker: "ECO: HUEVO DEL ABISMO",       text: "SU LATIDO RESPONDE AL POZO:/EL FAROL RECONOCE A LA CRIA."}',
        '  - {entity: lore_wyrm,       trigger: interact, speaker: "CRONICA DEL WYRM",            text: "CUATRO PISOTONES EN SU CORONA/QUEBRARAN SU CORAZA.|ATRAVIESA SUS BRASAS CON DASH/SI EL CIELO SE TORNA FUEGO."}',
        '  - {entity: egg_wyrm_2,       trigger: pickup,   speaker: "ECO: ULTIMO HUEVO",           text: "DOS ESCAMAS, UNA PROMESA:/LA NIDADA VUELVE TRAS EL WYRM"}',
        '  - {entity: rel_wall_climb,   trigger: pickup,   speaker: "RELIQUIA: GARRAS DEL POZO",   text: "ADQUIRISTE ESCALADA EN PARED!/DESLIZATE Y SALTA ENTRE MUROS.|VUELVE AL INICIO DE EL POZO:/SU TECHO ESCONDE UN SECRETO!"}',
        '  - {entity: lore_abismo,      trigger: interact, speaker: "ESTELA DEL PUENTE ANTIGUO",   text: "ANTES DEL POZO, LOS ALADOS/CRUZABAN ESTE PUENTE ANTIGUO.|LA NIDADA DESPERTARA CUANDO/LOS TRES GUARDIANES CAIGAN."}',
        '  - {entity: aq_lore_archivo,  trigger: interact, speaker: "ESTELA DEL ARCHIVO",         text: "BAJO LA GUARIDA DUERME/UN ARCHIVO SUMERGIDO.|SUS TUBOS AUN RESPIRAN/AGUA ANTIGUA Y FRIA."}',
        '  - {entity: aq_lore_presa,    trigger: interact, speaker: "ESTELA DE LA PRESA",         text: "LA PRESA CONTIENE EL OLVIDO./NO TODO SELLO ES DE PIEDRA.|SIGUE EL ECO HASTA EL FONDO/DE LA TORRE DE LAS CAMPANAS."}',
        '  - {entity: tw_lore_campana,  trigger: interact, speaker: "CAMPANA MAYOR",               text: "AQUI SUENAN LAS CAMPANAS/DE UNA ERA ANTERIOR AL POZO.|SU ECO GUARDA EL NOMBRE/DE LA PRIMERA NIDADA."}',
    ]
    dialogs = [d.rstrip() for d in dialogs]

    guides += [
        "  - {kind: shaft, x: 2504, up_x: 1940, split_y: 184, y_top: 152, y_bot: 320}",
        "  - {kind: shaft, x: 2112, up_x: 2096, split_y: 368, y_top: 336, y_bot: 464}",
        "  - {kind: shaft, x: 1008, up_x: 1044, split_y: 368, y_top: 336, y_bot: 464}",
        "  - {kind: shaft, x: 696,  up_x: 696,  split_y: 368, y_top: 336, y_bot: 464}",
        "  - {target: switch_start,    gate: door_start,     forward_x: 216}",
        "  - {target: flood,           forward_x: 1130}",
        "  - {target: boss_pozo,       gate: door_boss1,     forward_x: 2496}",
        "  - {target: rel_double_jump, style: ability,       below_y: 184}",
        "  - {target: sw_sanctum2,     gate: door_hub2,      below_y: 184}",
        "  - {target: rel_dash,        style: ability,       below_y: 184}",
        "  - {target: sw_seal3,        gate: door_wyrm_seal, below_y: 184}",
        "  - {target: wyrm,            gate: door_wyrm_exit, below_y: 184}",
        "  - {kind: shaft, x: 428,  up_x: 428,  split_y: 552, y_top: 528, y_bot: 712}",
        "  - {kind: shaft, x: 2284, up_x: 2284, split_y: 736, y_top: 712, y_bot: 896}",
    ]

    lines = [
        "# world.yaml — Mundo Metroidvania unificado (8x5 chunks = 320x115 tiles = 2560x920 px).",
        "# Generado automáticamente por tools/gen_world.py — NO editar a mano.",
        "name: world",
        "size_chunks: [8, 5]",
        "",
        "fill:",
        "  VIS: 0",
        "",
        "rects:",
        *rects,
        "",
        "entities:",
        *entities,
        "",
        "links:",
        *links,
        "",
        "ropes:",
        *ropes,
        "",
        "water:",
        *water,
        "",
        "backdrops:",
        *backdrops,
        "",
        "dialogs:",
        *dialogs,
        "",
        "guide:",
        *guides,
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--output", type=Path, help="escribe el archivo YAML generado")
    ap.add_argument("--check", type=Path, help="falla si el archivo difiere del generador")
    args = ap.parse_args()
    if not (args.output or args.check):
        ap.print_help()
        return 2

    generated = build_world_yaml()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding="utf-8", newline="\n")
        print(f"gen_world: {args.output} (8x5 chunks, {len(generated)} bytes)")

    if args.check:
        if not args.check.is_file():
            print(f"FAIL {args.check} no existe", file=sys.stderr)
            return 1
        actual = args.check.read_text(encoding="utf-8").replace("\r\n", "\n")
        if actual != generated:
            print(f"FAIL {args.check} difiere de tools/gen_world.py", file=sys.stderr)
            return 1
        print(f"gen_world: {args.check} OK")

    return 0


if __name__ == "__main__":
    sys.exit(main())
