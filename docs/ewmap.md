# Formato `.ewmap` v1

Binario de mapas de echo-well-engine. Lo genera `ewmapc` a partir de YAML (`assets/maps/src/*.yaml`) y lo lee `ew::map::MapLoader` (`engine/map/`). Definición canónica: `engine/map/ewmap_format.hpp`.

## Reglas generales
- **Little-endian**, magic **`EWMP`**, versión `1.0` (el loader rechaza otro `version_major`).
- Todo offset de sección es **múltiplo de 8**; cada payload se rellena con ceros hasta 8 bytes.
- Geometría fija: **tile 8×8 px**, **chunk 40×23 tiles** (320×184 px, cubre la vista de 320×180).
- **Compresión:** v1 es raw. El flag `compressed_chunks` está reservado; si viene en 1, el loader devuelve `CompressedUnsupported`.
- **CRC:** opcional (`--crc`). En CI siempre activado.

## Layout del archivo
```
[EwMapHeader 40 B][SectionDesc × N][pad→8][payload 1][pad]...[payload N-1][pad][CRC payload]
```

### EwMapHeader (40 bytes)
| off | tipo | campo | valor |
|---|---|---|---|
| 0 | char[4] | magic | `EWMP` |
| 4 | u16 | version_major | 1 |
| 6 | u16 | version_minor | 0 |
| 8 | u32 | flags | bit0 `crc`, bit1 `compressed_chunks` (reservado) |
| 12 | u32 | section_count | N |
| 16 | u16 | tile_size | 8 |
| 18 | u16 | chunk_w | 40 |
| 20 | u16 | chunk_h | 23 |
| 22 | u16 | map_w_chunks | |
| 24 | u16 | map_h_chunks | |
| 26 | u16 | reserved0 | 0 |
| 28 | u32 | reserved1 | 0 |
| 32 | u64 | section_table_offset | 40 |

### SectionDesc (24 bytes)
| off | tipo | campo |
|---|---|---|
| 0 | u32 | type |
| 4 | u32 | flags (0 en v1) |
| 8 | u64 | offset (desde inicio de archivo, múltiplo de 8) |
| 16 | u64 | size (bytes útiles, sin padding) |

## Secciones
| id | nombre | contenido |
|---|---|---|
| 1 | `TILE` | `u32 chunk_count, u32 pad`, luego `chunk_count` chunks |
| 2 | `ENT` | `u32 count, u32 pad`, luego `EntityRecord[count]` |
| 3 | `LINK` | `u32 count, u32 pad`, luego `LinkRecord[count]` |
| 4 | `WATER` | `u32 count, u32 pad`, luego `WaterRecord[count]` |
| 5 | `STRTAB` | strings UTF-8 terminados en `\0`; offset 0 = `""`, offset 1 = nombre del mapa |
| 7 | `ROPE` | `u32 count, u32 pad`, luego `RopeRecord[count]` (opcional; mapas viejos sin ROPE = sin cuerdas) |
| 8 | `MOVER` | `u32 count, u32 pad`, luego `MoverRecord[count]` (opcional; mapas viejos sin MOVER = sin plataformas móviles) |
| 9 | `BACKDROP` | `u32 count, u32 pad`, luego `BackdropRecord[count]` (opcional; sin la sección = todo `cueva`). Solo se escribe si el YAML trae `backdrops` |
| 10 | `DIALOG` | `u32 count, u32 pad`, luego `DialogRecord[count]` (opcional; sin la sección = sin diálogos). Solo se escribe si el YAML trae `dialogs` |
| 6 | `CRC` | `u32` CRC-32 IEEE de los bytes `[0, offset de CRC)`; siempre la última sección |

### Chunk (7368 bytes, múltiplo de 8)
`ChunkHeader { u16 cx, u16 cy, u32 layer_count = 4 }` + 4 capas × 920 `u16` (fila mayor, 40 por fila), en este orden:

| idx | capa | uso |
|---|---|---|
| 0 | `COL` | colisión (0 = vacío) |
| 1 | `VIS` | tile visual (G-buffer, pasada deferred). Tras el autotiling 4 vecinos: `familia*16+máscara` (N=1,E=2,S=4,W=8). |
| 2 | `FG` | primer plano, sobre los actores. El runtime lo sube en el canal B (`RGB8UI`) y el geometry pass lo dibuja encima (depth 0.2, atlas de `VIS`). Mismo autotiling que `VIS`. |
| 3 | `META` | marcadores de zona. Runtime: id **2** = agua (documentación; simula la sección `WATER`), id **3** = meta (`Sim::kMetaGoal`), id **4** = peligro / pinchos (`Sim::kMetaHazard`: al pisarlo, muerte y reaparición en el checkpoint). |

El streaming en runtime usa `ew::map::ChunkCache` (LRU con capacidad configurable) sobre `MapLoader::chunk_layer()`.

### Registros
- `EntityRecord` (16 B): `u32 name_str, u32 type_str` (offsets en STRTAB), `i32 x, i32 y` (px).
- `LinkRecord` (8 B): `u32 from_entity, u32 to_entity` (índices en ENT; ej. switch → puerta).
- `WaterRecord` (24 B): `i32 x, y, w, h` (px), `f32 stiffness, damping` (resortes Hooke).
- `RopeRecord` (16 B): `i32 x, y` (ancla fija, px), `u32 segments` (1..256), `f32 seg_len` (px).
- `MoverRecord` (88 B): `u32 entity` (índice en ENT, type `mover`), `u16 w_tiles, h_tiles` (1..64), `f32 speed` (px/s), `f32 pause` (s en cada waypoint), `u16 n_wp` (2..8), `u16 tile` (id fuente YAML), `u32 pad`, `i32 wp_x[8], wp_y[8]` (px, esquina sup. izq. de cada waypoint). Recorre la ruta en ping-pong.
- `BackdropRecord` (8 B): `u16 cx, cy` (chunk), `u16 theme` (`ew::map::BackdropTheme`, `engine/map/backdrop.hpp`: 0 `cueva`, 1 `pozo`, 2 `raices`, 3 `grieta`, 4 `cisterna`, 5 `nido`, 6 `engranajes`, 7 `invertido`, 8 `arena`, 9 `vacio`, 10 `cripta`, 11 `guarida`, 12 `santuario`, 13 `abismo`; desconocido = `cueva`), `u16 seed` (variación del dibujo). Un registro por chunk; los chunks sin registro usan `cueva`. Ver [level-design.md](level-design.md#fondos-backdrops).
- `DialogRecord` (16 B): `u32 entity` (índice en ENT), `u32 speaker_str` y `u32 text_str` (offsets en STRTAB; páginas con `|`, saltos de línea con `/`), `u16 trigger` (`0` = `interact`, `1` = `pickup`, `2` = `proximity`), `u16 flags` (`bit0` = `kDialogFlagOnce`: se dispara una sola vez).

## Fuente YAML (v1)
Ver `assets/maps/src/hello.yaml` y `assets/maps/src/slice.yaml`. Claves: `name`, `size_chunks: [w, h]`, `fill: {CAPA: tile}`, `rects: - {layer, x, y, w, h, tile}` (en tiles), `autotile` (`true` por defecto: aplica bitmask de 4 vecinos a `VIS`/`FG`; `false` deja los ids del YAML), `entities: - {name, type, x, y}` (px; un `mover` suma `w`, `h` en tiles, `to_x`/`to_y` o `path: "x y ..."`, `speed`, `pause`, `tile`), `links: - {from, to}` (por nombre), `water: - {x, y, w, h, stiffness, damping}`, `ropes: - {x, y, segments, seg_len}` (seg_len por defecto 6), `backdrops: - {cx, cy, theme, seed}` (fondo procedural por chunk; `seed` por defecto `cx*7 + cy*13`), `dialogs: - {entity, trigger, speaker, text, once}` (strings entre comillas `"..."`, páginas separadas por `|`, líneas por `/`).

Tipos de entidad que entiende el runtime (`echo_game`): `player_spawn`, `switch`, `flood_switch`, `door` (1×3 tiles, esquina sup. izq.), `break_wall` (muro agrietado de 1×3 tiles, esquina sup. izq.: sólido hasta que una embestida `Dash` lo rompe, `active = true`), `checkpoint`, `goal`, `collectible` (huevo de dragón: se agarra a ≤ 12 px del centro del player; persiste en respawn, se resetea con R largo; HUD n/total), `ability_double_jump` (reliquia cian: se agarra a ≤ 12 px y desbloquea Doble Salto; persiste en respawn y save v3, se resetea con R largo), `ability_dash` (reliquia ígnea: se agarra a ≤ 12 px y desbloquea Embestida / Dash para atravesar vacíos y romper `break_wall`; persiste en respawn y save v3), `npc` y `lore` (personaje cronista o estela dracónica con los que el player interactúa a ≤ 16 px si tienen un registro en `DIALOG`), `mover` (plataforma de `w`×`h` tiles que recorre waypoints en ping-pong; el player viaja encima; si queda aplastado contra un sólido, muere; `reset`/`respawn` la devuelven al inicio), `gravity_flip` (orbe: tocarlo a ≤ 10 px alterna la gravedad del player normal ↔ invertida; se rearma al alejarse > 18 px; el checkpoint guarda la gravedad), `boss` (`(x, y)` = centro de los pies; patrón determinista caminar → aviso → salto; 3 pisotones lo vencen y abren sus puertas linkeadas — `links: {from: boss, to: door}` —; tocarlo de otra forma mata; con un boss vivo la meta no cuenta). Un switch puede tener varios `links` (abre todas sus puertas).
El id fuente de `VIS`/`FG` es la familia (`(tile%15)+1`, 1..15); el compilador escribe `familia*16+máscara` (bits N=1, E=2, S=4, W=8). Dos tiles conectan si tienen el mismo id fuente; el borde del mapa se considera expuesto. `COL` y `META` no se tocan.
Tipos de entidad usados: `player_spawn`, `switch`, `door`, `checkpoint` (al acercarse a ≤ 20 px queda como punto de reaparición; **R** corto o morir vuelve ahí), `flood_switch` (como `switch`, y al estar activo sube el agua de las piletas con `x ≥ 400` hasta `Sim::kFloodRestY`; al apagarse baja), `goal` (meta; `Sim` la considera alcanzada a ≤ 16 px, o al pisar un tile `META` con id **3**). Ids de `META` reservados: **2** = zona de agua (documentación / edición; el runtime usa la sección `WATER`), **3** = zona de meta, **4** = peligro / pinchos (`Sim::kMetaHazard`).
El parser es un subconjunto mínimo de YAML (listas como flow maps `- {k: v}`). El adapter TMX queda para después.
