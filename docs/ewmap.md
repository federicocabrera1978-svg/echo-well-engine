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

## Fuente YAML (v1)
Ver `assets/maps/src/hello.yaml` y `assets/maps/src/slice.yaml`. Claves: `name`, `size_chunks: [w, h]`, `fill: {CAPA: tile}`, `rects: - {layer, x, y, w, h, tile}` (en tiles), `autotile` (`true` por defecto: aplica bitmask de 4 vecinos a `VIS`/`FG`; `false` deja los ids del YAML), `entities: - {name, type, x, y}` (px), `links: - {from, to}` (por nombre), `water: - {x, y, w, h, stiffness, damping}`, `ropes: - {x, y, segments, seg_len}` (seg_len por defecto 6).
El id fuente de `VIS`/`FG` es la familia (`(tile%15)+1`, 1..15); el compilador escribe `familia*16+máscara` (bits N=1, E=2, S=4, W=8). Dos tiles conectan si tienen el mismo id fuente; el borde del mapa se considera expuesto. `COL` y `META` no se tocan.
Tipos de entidad usados: `player_spawn`, `switch`, `door`, `checkpoint` (al acercarse a ≤ 20 px queda como punto de reaparición; **R** corto o morir vuelve ahí), `goal` (meta; `Sim` la considera alcanzada a ≤ 16 px, o al pisar un tile `META` con id **3**). Ids de `META` reservados: **2** = zona de agua (documentación / edición; el runtime usa la sección `WATER`), **3** = zona de meta, **4** = peligro / pinchos (`Sim::kMetaHazard`).
El parser es un subconjunto mínimo de YAML (listas como flow maps `- {k: v}`). El adapter TMX queda para después.
