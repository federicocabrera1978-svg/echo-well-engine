# Cómo armar una sala en `.ewmap`

Guía práctica para diseñar un nivel, en el estilo de *Animal Well*. La especificación
binaria está en [ewmap.md](ewmap.md). El ejemplo mínimo de esta página es
[`assets/maps/src/room.yaml`](../assets/maps/src/room.yaml) (1 chunk: spawn → salto →
meta). CI lo compila con `ewmapc` (CRC siempre on) y lo verifica con `ewmap_smoke`.

## Paso a paso

1. Copiá `assets/maps/src/room.yaml` a un YAML nuevo en la misma carpeta (o editá ese).
2. Dibujá el recinto en `rects` (capas `COL` / `VIS` / `FG` / `META`) y ubicá entidades,
   `links`, `water` y `ropes`. Coordenadas: tiles para los rects, **px** para lo demás.
3. Compilá con `ewmapc --crc` y verificá con `ewmap_smoke`.
4. Probá en el juego con `echo_engine --map`.
5. Si la sala queda, enganchala al build (CMake + ctest), como ya están `hello`, `slice`
   y `room`.

## Coordenadas

| Cosa | Unidad | Notas |
|---|---|---|
| Tile | 8×8 px | `COL`/`VIS`/`FG`/`META` se pintan en **tiles** (`rects`) |
| Chunk / pantalla | 40×23 tiles = 320×184 px | la vista es 320×180; hay 4 px de holgura vertical |
| Entidades, agua, cuerdas | **px** | origen arriba-izquierda, **Y hacia abajo** |
| Piso típico | tile `y=20` | la superficie queda en y=160 px; el player (radio 4) reposa en y≈156. Un spawn en y=152 cae un toque y se asienta |

Un mapa de `size_chunks: [W, H]` mide `W*320` × `H*184` px. El chunk `(cx, cy)` cubre
x=`cx*320`..`(cx+1)*320`, y=`cy*184`..`(cy+1)*184`. La cámara trata cada chunk como una
pantalla (estilo Animal Well) con dead-zone y slide en los seams.

## Fuente YAML

El parser (`yaml_lite`) es un subconjunto: mapas en flow (`- {k: v}`), sin anclas ni tipos
YAML avanzados. Comentarios con `#`.

```yaml
name: room
size_chunks: [1, 1]          # W×H chunks
# autotile: true             # default; `false` deja los ids de VIS/FG tal cual
fill:
  VIS: 0                     # relleno inicial por capa (opcional)
rects:
  - {layer: COL, x: 0, y: 20, w: 40, h: 3, tile: 1}
entities:
  - {name: spawn, type: player_spawn, x: 32, y: 152}
links:
  - {from: switch_a, to: door_a}
ropes:
  - {x: 128, y: 24, segments: 16, seg_len: 6}
water:
  - {x: 160, y: 136, w: 64, h: 24, stiffness: 0.02, damping: 0.04}
```

Los `rects` posteriores **pisan** a los anteriores (así se abren huecos: primero el piso,
después un rect de `tile: 0`). `COL` y `META` no pasan por el autotile.

## Capas

| Capa | Qué es | Runtime |
|---|---|---|
| `COL` | colisión (0 = vacío, ≠0 = sólido) | `Scene::solid_tile` |
| `VIS` | dibujo (atlas de tiles) | G-buffer |
| `FG` | primer plano, sobre los actores | canal B, depth 0.2 |
| `META` | marcadores de zona | ids reservados (abajo) |

### META

Solo estos ids tienen significado en el runtime (o en un editor). No inventes otros.

| Id | Uso |
|---|---|
| 2 | Agua (solo documentación / edición; el runtime usa la sección `WATER`) |
| 3 | Zona de meta (`Sim::kMetaGoal`), además de la entidad `goal` (alcance 16 px) |
| 4 | Peligro / pinchos (`Sim::kMetaHazard`): muerte y reaparición en el checkpoint |

Al diseñar un foso de pinchos, hacé la zona **más ancha** que el hueco: al caer, el player
sigue avanzando en horizontal (~50 px en una caída de un piso). Si la zona es estrecha,
aterriza más allá y no muere.

## Autotile (4 vecinos)

Por defecto `ewmapc` convierte cada tile de `VIS` y `FG` en `familia*16+máscara`:

- Familia = `(tile_yaml % 15) + 1` (1..15; el 0 queda vacío, ids compilados 16..255).
- Máscara: **N=1, E=2, S=4, W=8**. Dos tiles se consideran vecinos si tienen el mismo id
  fuente.
- El borde del mapa se considera **expuesto** (sin vecino).
- El atlas (`assets/atlas/tiles.png`, generado por `tools/gen_atlas.py`) pone la familia
  en la fila y la máscara en la columna; bisel solo en lados expuestos.

Para obtener la familia `F` (1..15) usá `tile: F-1` (si F>1) o `tile: 15` (familia 1).
Familias del atlas default:

| Familia | Nombre | YAML típico |
|---|---|---|
| 1 | stone | `15` |
| 2 | brick | `1` o `16` |
| 3 | dirt | `17` (piso de `hello` / `slice` / `room`) |
| 4 | grass | `3` o `18` (cap verde si N está expuesto) |
| 10 | moss | `9` (columnas VIS de `slice`) |
| 14 | darkwood | `13` (FG de `slice`) |

El resto, en orden 0..15: slate (fila 0 del atlas; no sale de un YAML autotileado),
stone, brick, dirt, grass, wood, metal, sand, ice, cave, moss, copper, crystal, snow,
darkwood, gold. Ver `tools/gen_atlas.py`.

`autotile: false` deja los ids del YAML (útil para decorado suelto o para debug).

## Entidades y links

Posición en **px**, `name` único (lo usan los `links`). El compilador acepta cualquier
`type` (queda como string); el runtime de `echo_game` solo entiende los de esta tabla.

| Tipo | Qué hace |
|---|---|
| `player_spawn` | Punto de aparición (obligatorio). R largo / menú “inicio” vuelven acá. |
| `switch` | Se activa a ≤ `Scene::kSwitchReach` (14 px) con E / X / ↓ / botón. |
| `door` | Bloquea un hueco de 1×3 tiles (`kDoorWTiles` × `kDoorHTiles`), esquina sup. izq. en (x, y). Sólida mientras `anim < kDoorSolidUntil` (0.5). |
| `checkpoint` | Al acercarse ≤ `Sim::kCheckpointReach` (20 px) queda como reaparición. R corto o morir vuelven acá. |
| `flood_switch` | Como `switch` (también se puede linkear a puertas). Activo, sube las piletas con **`x ≥ 400`** hasta `Sim::kFloodRestY` (72 px) a `kFloodSpeed` (48 px/s). Al apagarse, bajan a su `y` inicial. |
| `goal` | Meta, alcance `Sim::kGoalReach` (16 px) o pisar `META` 3. No exige collectibles. |
| `collectible` | Huevo, alcance `Sim::kCollectReach` (12 px) al centro del player. Sobrevive a morir y a R corto; R largo lo restablece. El HUD muestra n/total. |
| `mover` | Plataforma sólida de `w`×`h` **tiles** que recorre waypoints en ping-pong, a `speed` px/s (default 40) con `pause` s en cada extremo (default 0.5). `(x, y)` es la esquina **sup. izq.** (px) y el primer waypoint. El segundo sale de `to_x`/`to_y`, o una ruta más larga con `path: "x y x y ..."` (hasta 8 puntos en total, sin comas). `tile` es el id fuente del atlas (default 6, metal). Se dibuja con el atlas, con autotile 4 vecinos dentro del rect. El player parado encima viaja con ella (carry). No se atraviesa desde arriba, de costado ni desde abajo. **Aplastamiento:** la plataforma empuja; si después de empujar el player queda metido en un tile sólido o en otra plataforma, muere y reaparece en el checkpoint. `reset()` (R largo) y `respawn()` (R corto o muerte) la devuelven al primer waypoint. La cuerda colisiona contra ella; el agua no se toca. |

| `gravity_flip` | Orbe que **invierte la gravedad del player** al tocarlo (centro a ≤ `Sim::kFlipReach`, 10 px). Cada toque alterna normal ↔ invertida; el orbe se rearma cuando el player se aleja más de `Sim::kFlipRearm` (18 px), así que quedarse encima no lo repite. Con gravedad invertida el player cae hacia arriba, el techo funciona como piso (salto, `grounded`, carry de un `mover` por su cara de abajo, aplastamiento contra el piso) y se dibuja con los pies arriba. Ver “Gravedad invertida” abajo. |

| `boss` | Boss chiquito: caja de `Sim::kBossW`×`kBossH` (24×16 px); `(x, y)` es el **centro de los pies** (ponelo con `y` = superficie del piso). Patrón determinista en loop: camina `kBossWalkSec` (1.6 s) en ping-pong a ±`kBossRange` (64 px) de su posición, se queda quieto `kBossTellSec` (0.5 s, se ilumina: aviso) y salta (~33 px). Se vence con **3 pisotones** (`kBossHits`): caerle encima (moviéndose hacia abajo, centro por encima de su cabeza). Cada golpe lo hace más rápido y le da `kBossHurtSec` (1 s) de invulnerabilidad (parpadea; se lo puede atravesar). Tocarlo de cualquier otra forma **mata**. Con gravedad invertida el pisotón es desde abajo (en pleno salto). Al vencerlo se abren las puertas linkeadas (`links: - {from: boss, to: door}`) y, mientras haya un boss vivo, la meta **no cuenta**. Morir / R corto lo cura y lo devuelve a su lugar (si ya estaba vencido, queda vencido); R largo lo restablece entero. |

Ejemplo:

```yaml
  - {name: plat, type: mover, x: 96, y: 128, w: 3, h: 1, to_x: 216, to_y: 128, speed: 80, pause: 0.25, tile: 6}
  - {name: lift, type: mover, x: 80, y: 144, w: 2, h: 1, path: "80 64 160 64", speed: 40, pause: 0.5}
```

`links: - {from: nombre_switch, to: nombre_door}` abre la puerta cuando el switch (o
`flood_switch`) está activo. Un mismo switch puede apuntar a varias puertas.

`flood_switch` solo afecta agua cuyo rectángulo tiene `x ≥ 400` (la pileta del chunk 0
de `slice` no sube). Si tu pileta inundable está en el chunk 0, no va a subir.

## Agua y cuerdas

- **Agua:** `x, y, w, h` en px (la superficie de reposo es `y`; el fondo es `y+h`).
  `stiffness` / `damping` de los resortes Hooke (default del compilador: 0.02 / 0.04).
  El player nada: el salto en el agua empuja hacia arriba. Pintá también `META` 2 si
  querés ver la zona en un editor. El agua no sale sola de su rectángulo: cerrala con
  `COL` en los bordes, como en `hello.yaml` / `slice.yaml`.
- **Cuerdas:** `x, y` = ancla fija (px), `segments` (1..256), `seg_len` (default 6).
  Se agarra con C / Shift (o LB / Y), alcance `Sim::kGrabRange` (10 px). Colgado: ←/→
  balancean, ↑/↓ trepan, salto suelta con impulso. El nodo agarrado usa el radio del
  player (`kPlayerRadius` 4) para no atravesar paredes.

## Gravedad invertida

- Solo se invierte la gravedad **del player** (`Point::gscale = -1`). Las cuerdas siguen
  colgando hacia abajo y el agua no se mueve.
- **Salto / nado:** empujan contra la gravedad del player (invertido, hacia +Y). La altura
  del salto es la misma (~44 px).
- **Movers:** invertido, el player se apoya en la cara de **abajo** de una plataforma y
  viaja con ella; si la plataforma lo aprieta contra un sólido, muere igual que siempre.
- **Cuerda:** se agarra igual. Colgado, manda la cuerda; al soltar (o saltar, que empuja
  hacia +Y) vuelve a caer hacia el techo. Ojo: la punta de una cuerda frena al player que
  sube por debajo.
- **Agua:** el empuje va siempre **contra** la gravedad del player; invertido, el agua lo
  hunde hacia el fondo de la pileta (es su “arriba”). Mejor no mezclar agua con gravedad
  invertida en una sala.
- **Checkpoint:** guarda la gravedad que tenías al tocarlo. Morir o R corto reaparece con
  esa gravedad; sin checkpoint (spawn) y con R largo, gravedad normal y orbes como al inicio.
- Diseño: ubicá el orbe donde el player llega caminando y dejá un techo sólido encima
  (fuera del mapa cuenta como sólido, así que el borde superior también sirve). Un `checkpoint` en el techo se toma con el centro a ≤ 20 px: ponelo a
  `y = techo + 4`.
- Mapa de prueba: `assets/maps/src/gravity.yaml` (`gravity_test`).

## Boss

- Arena: dejá piso plano sin pinchos en toda la patrulla (`x ± 64` px) y un margen a
  cada lado para que el player espere. El boss no choca contra tiles: no pongas paredes
  dentro de la patrulla.
- Checkpoint **antes** de la arena, fuera del alcance del boss.
- La salida va detrás de una `door` linkeada al boss; la meta, detrás de la puerta.
- Para pegarle: esperá a que camine hacia vos, saltá cuando esté a ~40 px y caele en la
  cabeza. Mapa de prueba: `assets/maps/src/boss.yaml` (`boss_test`, que incluye una pelea
  jugada con input).

## Compilar y probar

```sh
# Compilar (CRC siempre en CI). Sale un .ewmap listo para el loader.
./build/ewmapc --crc assets/maps/src/room.yaml -o build/maps/room.ewmap

# Verificar estructura (geometría, chunks, entidades, CRC). Flags opcionales de cobertura:
./build/ewmap_smoke build/maps/room.ewmap \
    --name room --chunks 1 --ents 2 --has player_spawn --has goal

# Jugar (el engine busca el mapa junto al binario o por --map).
./build/echo_engine --map build/maps/room.ewmap
./build/echo_engine --map build/maps/room.ewmap --start 160 152   # aparece en (x, y)
./build/echo_engine --map build/maps/room.ewmap --frames 90 --demo --dump frame.ppm --nosound
```

En el árbol de build, CMake ya compila `hello`, `slice` y `room` hacia `build/maps/*.ewmap`.
`ctest` corre `ewmapc_room` + `ewmap_load_room` (y el job `build` de CI lo vuelve a
compilar y pasar por `ewmap_smoke`).

## Enganchar la sala al juego

El prototipo arranca en **`slice`** (`EW_DEFAULT_MAP`). Para probar la tuya no hace falta
cambiar el default:

```sh
./build/echo_engine --map build/maps/room.ewmap
```

Para que CMake la compile siempre, como `room.yaml`:

1. Dejá la fuente en `assets/maps/src/<nombre>.yaml`.
2. En `CMakeLists.txt`, copiá el bloque de `EW_ROOM_MAP` (custom command + `ew_maps` +
   `add_test` `ewmapc_*` / `ewmap_load_*` con `ewmap_smoke --name … --chunks … --has …`).
3. En `.github/workflows/ci.yml`, en el paso “Compile example maps”, agregá el par
   `ewmapc --crc` + `ewmap_smoke` con los mismos flags.
4. Si querés que viaje en el paquete portable, sumala a `install(FILES …)`.

`echo_engine --map` alcanza para jugarla; el default del binario se deja en `slice`.

## Ejemplo mínimo (`room`)

`assets/maps/src/room.yaml`: un solo chunk, piso con un hueco de 4 tiles (32 px, se salta
caminando + Espacio), paredes, `player_spawn` en (32, 152), `goal` en (288, 152) y `META` 3
junto a la meta. Es el esqueleto de una sala: a partir de acá se pueden agregar un
`switch`+`door`, una cuerda, una pileta, un checkpoint o un `collectible` copiando los
bloques de `hello.yaml` / `slice.yaml`.

## Consejos de diseño (estilo Animal Well)

- **Una idea por pantalla.** Cada chunk es una vista de 320×180. Que se lea de un vistazo:
  silueta clara, un problema (un salto, una puerta, una cuerda) y el camino a la siguiente
  pantalla. El decorado (`VIS` de fondo, `FG` adelante) no debería tapar la lectura del
  piso y los peligros.
- **Ritmo.** Alterná tensión (pinchos, un salto justo, nadar) con un tramo seguro. En
  `slice`, el switch y la puerta del chunk 0 enseñan la mecánica en piso firme; la cuerda
  y los pinchos llegan después.
- **Checkpoints antes del peligro.** Poné un `checkpoint` *antes* del foso o del tramo
  difícil, no después de morirte tres veces para llegar. En `slice`, `ck_ledge` está en la
  cornisa, justo antes del hueco con `META` 4.
- **Enseñá y después combiná.** Un switch solo, después una cuerda sola, después agua que
  sube. El huevo de la sala 3 reusa mecánicas ya vistas por un camino alternativo (pileta
  baja + cuerda 2); la meta no lo exige.
- **Plataformas sobre un foso (sala 4 de `slice`).** Checkpoint en la repisa de entrada,
  y las plataformas arrancan **pegadas** a la repisa / pilar (sin hueco de 1 tile: el
  círculo del player mide 8 px y se cae por un hueco de 8). El foso de pinchos (`META` 4)
  cubre todo el piso; el ascensor baja hasta 16 px sobre el piso seguro. `slice_test`
  resuelve la sala con un control que espera a cada plataforma (`solve_room4`).
- **Gravedad invertida (sala 5 de `slice`).** El checkpoint va **antes** del orbe y a más
  de 20 px de él (guarda la gravedad del momento en que se activa). El orbe del piso está
  antes del foso: el player “cae” al techo y se mueve ~55 px mientras sube, así que el
  techo tiene que seguir sólido ese tramo. El hueco del techo lleva `META` 4 arriba y una
  plataforma con la cara de abajo a ras del techo (`y` = fondo del techo − 8). El orbe
  del techo va **después** del foso del piso. `slice_test` la resuelve con `solve_room5`.
- **El secreto es opcional.** Un `collectible` se esconde al costado o “para atrás”
  (agua baja, puertas en la pileta). El HUD n/total informa; no bloquees el `goal`.
- **Diseñá para la cámara.** Los seams son cada 320 px. Un salto que empieza en un chunk
  y termina en el siguiente se siente distinto porque la cámara desliza. Dejá margen en
  los bordes (paredes, cornisas) para que el player no quede cortado.

## Trampas frecuentes

- Mezclar tiles y px: un spawn en `x: 4` queda *dentro* de la pared izquierda (4 px), no
  en el tile 4.
- El hueco de una puerta son **1×3 tiles** con origen en la posición de la entidad
  (px → tile). Si el piso no tiene un hueco ahí, la puerta no deja pasar aunque esté
  “abierta” visualmente.
- `goal`, `checkpoint`, `switch` y `collectible` miden distancia al **centro** del player,
  no al tile. Alcances: switch 14 px, collectible 12 px, goal 16 px, checkpoint 20 px.
- Familia de autotile: dos rects de VIS con tiles distintos (`17` vs `5`) no se fusionan,
  aunque se toquen. Usá el mismo id fuente para un mismo material. `tile: 17` es dirt
  (familia 3), no stone (familia 1, `tile: 15`).
- `fill` no pisa los `rects`; el orden de los `rects` sí importa.
- Fuera del mapa, `COL` se considera sólido (el player no se cae al vacío del mundo; para
  un foso mortal usá `META` 4). Si el centro pasa de `height_px + radio`, `Sim` también
  mata (caída fuera).
- `flood_switch` ignora piletas con `x < 400`. En un mapa de un solo chunk, el agua no va
  a subir.
- Un `mover` mide `w` y `h` en **tiles**, pero `(x, y)` y `to_x`/`to_y`/`path` van en **px**.
  `path` es una lista de enteros separados por espacios (`x y x y ...`), no usa comas (el
  parser de flow maps parte por coma). Sin `to_*` ni `path` el compilador rechaza la entidad.
  `w`/`h` van de 1 a 64, `speed` en (0, 1000], `pause` en [0, 60], como máximo 8 waypoints.
