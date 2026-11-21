# echo-well-engine

Motor 2D propietario en **C++20 / GLSL** para un prototipo de metroidvania atmosférico estilo *Animal Well*.

- Render **Deferred** a resolución interna **320×180**, escalado entero.
- Física de **Verlet** (cuerdas, objetos, partículas).
- **Agua** con resortes de **Hooke** (superficie dinámica).
- Mapas en binario propio **`.ewmap`**, cargados por chunks con streaming LRU.

> Estado: `.ewmap` (`ewmapc` + `echo_map`), escena (`echo_game`: MapLoader → colisión/entidades/cámara), física Verlet + agua Hooke (`echo_physics`) y `echo_engine` jugable sobre el Deferred 320×180. Mapa por defecto: **`slice`** (camino corto start→goal).

## Estructura
```
engine/core/         echo_engine: ventana GLFW + loop de frame
engine/game/         echo_game: Scene (MapLoader + ChunkCache → tiles COL/VIS, entidades, cámara, chunk activo)
engine/physics/      echo_physics: Verlet (puntos, sticks, damping, colisión círculo↔tiles) + WaterBody (Hooke)
engine/render/       echo_render: loader GL 3.3 mínimo + DeferredRenderer
assets/shaders/      GLSL 330: fullscreen.vert, gbuffer_map.frag, light.frag
assets/atlas/         tiles.png default (generado por tools/gen_atlas.py)
tools/gen_atlas.py    script reproducible del atlas PNG
tests/               tests sin GL (ctest)
engine/map/          echo_map: formato .ewmap, MapLoader, ChunkCache (LRU), CRC-32
tools/ewmapc/        compilador YAML -> .ewmap (+ smoke test)
assets/maps/src/     fuentes de mapas (YAML)
assets/maps/bin/     mapas compilados (generados, ignorados por git)
docs/ewmap.md        especificación binaria
docs/level-design.md guía para armar salas (YAML, capas, entidades)
```

## Build
Requisitos: CMake ≥ 3.20 y un compilador C++20 (GCC 11+, Clang 14+, MSVC 19.30+). `echo_map`/`ewmapc` no tienen dependencias externas. `echo_engine` usa **GLFW 3.4** (descargado por CMake `FetchContent`, hash fijado) y OpenGL 3.3 core; sin GLAD (loader propio en `engine/render/gl_loader.*`). En Linux: `sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`. Para compilar solo herramientas: `-DEW_BUILD_ENGINE=OFF`.
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # compila hello/slice/room/mover y los verifica con ewmap_smoke
```

## Correr el prototipo

**QA:** cómo instalar los paquetes, qué probar sala por sala y cómo reportar bugs: [docs/qa-checklist.md](docs/qa-checklist.md) (versión actual **v0.1.0**, `echo_engine --version`).

```sh
./build/echo_engine              # ventana 1280×720 (320×180 ×4), mapa slice (camino start→goal)
./build/echo_engine --scale 3    # 960×540
./build/echo_engine --frames 120 # sale tras N frames (smoke test / headless con xvfb-run)
./build/echo_engine --map otro.ewmap --dump frame.ppm  # otro mapa; vuelca el último frame 320×180
./build/echo_engine --map build/maps/hello.ewmap       # mapa de pruebas (2 chunks, 2 cuerdas)
./build/echo_engine --map build/maps/room.ewmap        # ejemplo mínimo de la guía (1 chunk, salto → meta)
./build/echo_engine --map build/maps/mover.ewmap       # prueba de plataforma móvil (no es sala de slice)
./build/echo_engine --demo        # input automático (camina, salta y activa el switch)
./build/echo_engine --start 330 150  # aparece en (x, y) px; p. ej. chunk 1
```
Controles: **←/→** o **A/D** caminar, **Espacio/↑/Z** saltar (también nadar hacia arriba en el agua, y **doble salto** aéreo una vez desbloqueada la reliquia), **E/X/↓** activar el switch cercano, **C/Shift izq.** agarrar/soltar la cuerda, **R** corto vuelve al último checkpoint (o al spawn), **R** largo (~0.8 s) reinicia todo al inicio, **ESC** o **Start** abre el menú de pausa (seguir, continuar la partida guardada, reiniciar desde checkpoint, volumen/mute, controles, salir). Gamepad (Xbox/SDL): stick o d-pad caminar, **A**/RB saltar, **X**/**B** switch, **LB**/**Y** agarrar cuerda, stick Y / d-pad trepar, **Start** abre el menú (el pad no tiene R: usá REINICIO del menú). `--nogamepad` o `EW_NOGAMEPAD=1` lo apaga. **Gravedad invertida:** los orbes violetas (`gravity_flip`) invierten la gravedad del player al tocarlos (pies arriba); ver la guía de diseño. `./build/echo_engine --map build/maps/gravity.ewmap` la prueba. **Habilidades (Doble Salto):** la reliquia cian (`ability_double_jump`) desbloquea 1 salto extra en el aire (se recarga al apoyar, agarrar una cuerda, pisar un boss o entrar al agua). Mapa de prueba: `--map build/maps/ability.ewmap` (`ability_test`). **Boss:** se le pega cayéndole encima (3 pisotones); tocarlo de otra forma mata; al vencerlo se abre la puerta de la meta. Arena de prueba: `--map build/maps/boss.ewmap`. SFX procedural (beeps) si hay dispositivo; `--nosound` o `EW_NOSOUND=1` lo apaga (CI headless). El build compila `assets/maps/src/{slice,hello,room,mover,gravity,boss,ability}.yaml` → `build/maps/*.ewmap` (con CRC) y usa **slice** por defecto. Cómo armar una sala nueva: [docs/level-design.md](docs/level-design.md).

### Controles configurables
Menú de pausa → **CONTROLES**: una fila por acción (izquierda, derecha, arriba, abajo, salto, acción, cuerda, reinicio) con sus teclas y, para salto / acción / cuerda, sus botones del pad. **Enter**/**A** en una fila espera la tecla o el botón nuevo (**Esc** cancela); la acción queda con esa sola tecla y, si otra acción la usaba, recibe la anterior (intercambio), así ninguna queda sin tecla. **POR DEFECTO** vuelve a los controles de arriba. Esc/Enter, Start/Back/Guide y el d-pad no se asignan (son del menú y del movimiento). El menú siempre se navega con flechas/WASD, Enter/Espacio/Z y d-pad/A/B. Los controles se guardan en el mismo `save.ewsv` (formato v2; los saves v1 se siguen leyendo con controles por defecto) apenas se cambian. Lógica sin GLFW en `engine/game/bindings.hpp` (test `bindings_test`).

### Partida guardada
Un slot, autoguardado: se escribe al tomar un checkpoint, agarrar un huevo, desbloquear una habilidad, vencer al boss, pasar al siguiente nivel, cambiar el volumen o los controles. Guarda mapa, checkpoint (+ gravedad), estado de cada entidad (puertas, switches, inundación, huevos, reliquias, boss), habilidades desbloqueadas, volumen/mute y controles. Si al arrancar hay un save del mapa actual o de otro nivel disponible de la campaña, el menú abre en **CONTINUAR** (carga el mapa del save y aparecés en el checkpoint guardado); **SEGUIR** arranca de cero y el save se reemplaza recién cuando progresás. **R** largo vuelve a un spawn limpio.

- Archivo `save.ewsv` en el directorio de datos del usuario: `$XDG_DATA_HOME/echo-well` o `~/.local/share/echo-well` (Linux), `%APPDATA%\EchoWell` (Windows), `~/Library/Application Support/EchoWell` (macOS); `EW_SAVE_DIR` lo cambia. Si no se puede escribir, usa la carpeta del binario; si tampoco, no guarda.
- Formato binario versionado (`EWSV`, v3; lee v1, v2 y v3) con bounds en cada lectura, tamaño exacto y CRC-32; un save corrupto, truncado o de otro mapa se ignora. Escritura atómica (tmp + rename). Layout en `engine/game/save.hpp`.
- `--nosave` / `EW_NOSAVE=1` lo apaga; `--demo`, `--start` y `--frames` (sin `EW_SAVE_DIR`) tampoco guardan.
- Tests: `save_test` (round-trip, controles v2, habilidades v3 y compatibilidad v1/v2, apply sobre `slice`, rechazo de bytes cambiados/truncados y campos fuera de rango, archivo y directorio) y `fuzz_save` (CI: 60 s con libFuzzer + smoke en ctest).

### Vertical slice (`slice`)
Camino **start → goal** en 8×1 chunks (2560×184 px) que junta las mecánicas existentes:

1. **Chunk 0:** spawn, **switch → puerta**, pileta de **agua** (hay que nadar). Seam en x=320.
2. **Chunk 1:** **cuerda** (sección ROPE) para subir una pared que no se salta, cornisa con **atlas** de tiles. Seam en x=640. La cámara usa dead-zone + slide en los seams.
3. **Chunk 2:** **checkpoint** en la cornisa, **hueco con pinchos** (META 4) y un **`flood_switch`**.
4. **Chunk 3:** pileta cuyo nivel **sube** al activar el switch (de y=136 a y=72, `Sim::kFloodRestY`) para nadar hasta la cornisa de salida. Otro checkpoint en la salida.
5. **Chunk 4 — sala del huevo (opcional):** con el agua **baja** (sin activar el `flood_switch`), un `switch_well` dentro de la pileta abre dos **puertas** en su pared derecha; se nada al pasillo bajo la cornisa de salida, se usa una **segunda cuerda** para trepar a una repisa y se agarra el **huevo** (`type: collectible`). A la repisa también se llega por arriba, desde la cornisa de salida, saltando el hueco de la cuerda; la repisa sigue por un hueco en la pared derecha hacia la sala 4.
6. **Chunk 5 — sala 4, plataformas móviles:** checkpoint `ck_mover` en la repisa de entrada; el piso entero es un **foso de pinchos**. Una plataforma horizontal (`plat_h`) lleva hasta un pilar; desde ahí un ascensor vertical (`lift_v`) baja al piso seguro, que sigue por una puerta abierta hacia la sala 5.
7. **Chunk 6 — sala 5, gravedad invertida:** checkpoint `ck_grav` al entrar; un orbe en el piso (`flip_up`) invierte la gravedad justo antes de un foso de pinchos que no se salta. Se camina por el **techo**, se cruza un hueco del techo (con pinchos arriba) **colgado de la cara de abajo** de una plataforma móvil (`plat_inv`), y un orbe del techo (`flip_down`) devuelve la gravedad sobre el piso seguro, que sigue por una puerta abierta a la sala final.
8. **Chunk 7 — sala 6 final, boss:** checkpoint `ck_boss` al entrar; el **boss** patrulla el medio de la arena (caminar → aviso → salto, más rápido con cada golpe). Se le pega **cayéndole encima** (3 pisotones); tocarlo de otra forma mata y morir lo cura. Al vencerlo se abre la puerta de la derecha (`door_boss`) y detrás está la **meta** (goal + META 3, x=2520), que cierra la demo con el fade. Con el boss vivo, la meta no cuenta.

**Collectibles:** HUD arriba a la izquierda con huevo + **n/total** (dorado al completar, destello + beep al agarrar). Lo agarrado **sobrevive** a morir y a **R** corto (checkpoint); **R** largo (spawn limpio) vuelve a 0. La meta **no** requiere el huevo (se puede terminar 0/1).
Al tocar la meta, se abre la **pantalla de final pixel-art** (`EndScreen` en `engine/game/end_screen.hpp`, `end_screen_test`) con el resumen del nivel (huevos, muertes y habilidades) y las opciones **SIGUIENTE NIVEL** (cuando el siguiente mapa de la campaña en `engine/game/campaign.hpp` o `--next-map FILE` existe en disco, transfiriendo las habilidades desbloqueadas), **REINICIAR NIVEL**, **MENU DE PAUSA** y **SALIR**. **R** corto sigue volviendo al último checkpoint y **R** largo (~0.8 s) al spawn limpio. `slice_test` recorre todo el camino (incluye inundación, la sala 4 con las plataformas, la sala 5 con gravedad invertida y la pelea con el boss de la sala 6, con muertes en los fosos y contra el boss) y resuelve la sala del huevo (switch_well → puertas → cuerda 2 → huevo → salas 4, 5 y 6 → meta 1/1) sin GL.

`slice_test` (ctest, sin GL) carga el mapa, verifica el loader (entidades, capas COL/VIS/FG/META, cuerda, agua) y recorre el camino con input scriptado hasta la meta. `hello` sigue como mapa de los demás tests.

### Escena (MapLoader → render)
- `ew::game::Scene` carga el `.ewmap`, lee tiles vía `ChunkCache` y expone `solid_px()` (capa `COL`; fuera del mapa = sólido), entidades y una cámara que sigue al player clampada al mapa. El **chunk activo** sale de la posición del player.
- Pipeline: geometry pass → los tiles `COL`/`VIS`/`FG` del vecindario 3×3 se suben como textura `RGB8UI` 120×69 (solo cuando cambia el chunk) y el shader dibuja COL/VIS con bisel, luego actores, y **FG por encima** (primer plano); marcadores (player, `switch`, `door`, `player_spawn`) van como uniforms → G-buffer 320×180 (albedo + normal + depth24) → light pass (ambiente + luz puntual que sigue al player) → blit NEAREST con escalado entero y letterbox.

### Chunks vecinos y cámara por pantallas
- Cada chunk (40×23 tiles = 320×184 px) es **una pantalla**, como en *Animal Well*. El renderer recibe el **vecindario 3×3** del chunk activo (textura `RGB8UI` 120×69 (R=COL, G=VIS, B=FG), re-subida solo al cambiar de chunk), así que se dibujan el activo y sus 8 vecinos (incluye los 4 ortogonales); los que caen fuera del mapa quedan vacíos.
- **Elección de cámara: slide corto, no snap.** Al cruzar un seam el chunk activo cambia en el acto (colisión/streaming) y la cámara **desliza** hacia la nueva pantalla con un ease exponencial (`kCamSlide = 12/s`, ~0.3 s; termina exacto en el origen del chunk). Dentro de una pantalla la cámara usa una **dead-zone** centrada (`kDeadHalfW = 48`, `kDeadHalfH = 24` px): mientras el player esté adentro no se mueve; al salir, la arrastra lo justo y se clampa a la pantalla del chunk. Al entrar a un chunk nuevo (o en snap) se re-centra clampado y el slide del seam sigue igual. Con chunks de 320×184 el efecto real es solo vertical (holgura 4 px); en chunks más grandes que la vista funciona en ambos ejes. Lógica pura en `Scene::dead_zone_step` (testeada en `scene_test`). Se redondea a px enteros al render (pixel-perfect). `follow(x, y)` sin `dt` hace snap (carga / `--start`).
- `hello` ahora es 2×1 chunks: el chunk 1 tiene plataformas escalonadas; se llega abriendo la puerta con el switch. Probar: activar el switch, caminar a la derecha y cruzar x=320, o `--start 330 150`.

### Switch → puerta
- Los `links:` del mapa (sección `LINK`) conectan `switch` → `door`. Con el player a ≤ 14 px del switch, **E/X/↓** lo alterna y abre/cierra todas sus puertas enlazadas.
- Puerta = 1×3 tiles (8×24 px) desde su `(x, y)`. **Cerrada** es sólida (`Scene::solid_tile` la suma a `COL`); **abierta** no colisiona. La apertura se anima en 0.25 s (`anim` 0→1, `Scene::tick`); colisiona mientras `anim < 0.5`. Al cerrarse, la colisión vuelve al instante y el visual se desliza hacia abajo.
- Feedback: el switch tiene luz **ámbar** (apagado) / **verde** (encendido); la puerta se **desliza hacia arriba** al abrir (3 bloques violeta) y queda un marco tenue; al agarrar la cuerda el nodo brilla y un anillo se expande (~0.3 s, `grab_flash`). Trepar no reinicia el destello.
- En `hello`, la puerta (x=280) bloquea el paso hacia el borde derecho hasta activar `switch_a` (x=96). `door_test` (ctest) lo verifica.

### Física Verlet
- `ew::physics::World`: puntos con `pos/prev` (Verlet posicional), gravedad 600 px/s², damping, constraints de distancia (`Stick`, con rigidez), pares círculo↔círculo y colisión círculo↔tiles sólidos (empuje por punto más cercano + fricción tangencial, sin rebote). 6 iteraciones por paso.
- `ew::game::Sim` une `Scene` + `World` a paso fijo 60 Hz: el player es un círculo r=4 que cae, camina (90 px/s) y salta (solo apoyado) contra la capa `COL`; una cuerda de 16 segmentos cuelga del techo y el player la empuja.
- `physics_test` (ctest) verifica apoyo en el piso, caminar, salto, paredes y largo de la cuerda.

### Cuerda jugable
- Saltá hacia la punta de la cuerda (x≈128) y apretá **C** o **Shift**: el player se engancha al nodo más cercano (≤10 px).
- Colgado: **←/→** empujan el nodo (balanceo con física Verlet; el player pesa 4× un nodo), **↑/↓** o **W/S** trepan/bajan nodo a nodo (40 px/s), **Espacio/Z** suelta con el impulso del balanceo + salto, **C/Shift** suelta sin impulso.
- Mientras está colgado, ↑ no salta y ↓ no activa switches.
- `rope_test` (ctest) verifica agarre por rango, amplitud de balanceo, trepar/bajar, soltar con salto y largo de la cuerda.
- Las cuerdas salen del mapa: `ropes: - {x, y, segments, seg_len}` en el YAML (ancla en px) → sección `ROPE` del `.ewmap`. `hello` trae 2 (x=128 chunk 0, x=440 chunk 1). Se agarra cualquiera; trepar se queda en la misma cuerda.
- Los segmentos de la cuerda **no atraviesan tiles sólidos**: si un stick cruza el interior de un tile, se empuja (mínima traslación en un eje) para envolverse en la esquina o quedar del mismo lado. `rope_wall_test` lo verifica (stick que ya atraviesa, balanceo contra una pared, envolverse en un bloque). El nodo agarrado usa el radio del player (4), así que colgado no se mete en las paredes; al soltar, el nodo vuelve a r=1.5.

### Agua Hooke
- Cada registro `water:` del mapa (sección `WATER`) crea un `ew::physics::WaterBody`: la superficie se divide en columnas (~2 px, máx. 64) y cada una es un resorte de Hooke `a = -k·x - d·v` con `k = stiffness` y `d = damping` del mapa (defaults 0.025 / 0.03 si vienen en 0). Las columnas se acoplan con sus vecinas (`spread`) para propagar ondas.
- Interacción con Verlet: empuje proporcional a la fracción sumergida (el player flota en la superficie), arrastre dentro del agua, salpicadura al cruzar la superficie y estela suave al nadar en horizontal.
- Render: el geometry pass recibe, por cuerpo, rect + n° de columnas + altura de cada columna (uniform arrays, stride 64) y dibuja el volumen + una línea de superficie clara con normales según la pendiente (el light pass la ilumina). Hasta **4 cuerpos por frame** (`kMaxWaterBodies`); se mandan solo los que tocan la vista.
- Contención: en `hello` cada pileta tiene piso y bordes `COL` de 3 tiles a ambos lados hasta la superficie (pileta 1 entre tiles x=19/28 en el chunk 0, pileta 2 entre x=62/71 en el chunk 1). La superficie además se clampa a ±90 % de la profundidad. `water_test` verifica que todo cuerpo del mapa esté contenido.
- `water_test` (ctest) verifica propagación, amortiguación, salpicadura y flotación.

### Capas `FG` y `META`
- **`FG`** (primer plano): se empaqueta en el canal B de la textura del vecindario (`RGB8UI`) y el geometry pass la dibuja **sobre los actores** (depth 0.2, mismo atlas que `VIS`). `slice` trae una franja de decorado FG en el chunk 2; `slice_test` verifica el tile y el packed.
- **`META`**: marcadores de zona. El runtime usa **id 3** como zona de meta (`Sim::kMetaGoal`), **id 4** como peligro / pinchos (`Sim::kMetaHazard`) y **id 2** documenta agua (el runtime usa la sección `WATER`). Ver [docs/ewmap.md](docs/ewmap.md).

### Fondos por sala (backdrops)
Cada pantalla tiene un fondo procedural propio, estilo Animal Well, dibujado en el geometry pass (sin imágenes ni dependencias): pozo con haz de luz y gotas, raíces, roca partida con brasas, cisterna con caños, nichos con huevos, engranajes que giran, arquitectura espejada para la gravedad invertida y la arena del boss con el ojo de la bestia y antorchas. Se eligen en el YAML (`backdrops: - {cx, cy, theme}`) y viajan en la sección opcional `BACKDROP`; los mapas sin la sección usan `cueva`. Detalle y lista de temas en [docs/level-design.md](docs/level-design.md#fondos-backdrops). `backdrop_test` (ctest) cubre la tabla de temas, el vecindario 3×3 y la retrocompatibilidad; `tools/room_shots.py` saca una captura por sala (CI: artifact `fondos-salas`).

## Formato `.ewmap` (resumen)
- Magic `EWMP`, little-endian, secciones alineadas a 8 bytes, versión 1.0.
- Tile 8×8 px, chunk 40×23 tiles; capas `COL`, `VIS`, `FG`, `META`.
- Secciones: `TILE=1`, `ENT=2`, `LINK=3`, `WATER=4`, `STRTAB=5`, `CRC=6`, `ROPE=7`, `MOVER=8`, `BACKDROP=9` (las tres últimas, opcionales).
- Chunks raw en v1 (flag `compressed_chunks` reservado). CRC opcional; en CI siempre activo.

Detalle completo en [docs/ewmap.md](docs/ewmap.md).

## Uso de `ewmapc`
```sh
./build/ewmapc --crc assets/maps/src/hello.yaml -o assets/maps/bin/hello.ewmap
# ewmapc: assets/maps/src/hello.yaml -> assets/maps/bin/hello.ewmap (7728 bytes, 1 chunks, 3 entities, crc)
```
`ewmapc` valida su propia salida con `MapLoader` antes de escribirla.
`ewmap_smoke FILE` carga cualquier `.ewmap` y verifica estructura (geometría, chunks, entidades, links, agua, cuerdas, plataformas móviles, CRC, cache). Flags opcionales: `--name`, `--chunks`, `--ents`, `--links`, `--water`, `--ropes`, `--movers`, `--backdrops`, `--has TYPE` (se puede repetir). CI y ctest lo corren sobre `hello`, `slice`, `room` y `mover`.

## Cargar un mapa (C++)
```cpp
#include "map/chunk_cache.hpp"
ew::map::MapLoader map;
if (auto e = map.load_file("assets/maps/bin/hello.ewmap"); e != ew::map::MapError::None)
  log(ew::map::to_string(e));
for (const auto& ent : map.entities()) spawn(map.string_at(ent.type_str), ent.x, ent.y);
ew::map::ChunkCache cache(map, /*capacity=*/9);
if (auto* c = cache.acquire(0, 0)) upload(c->layers[uint32_t(ew::map::Layer::VIS)]);
```

## Atlas de tiles

`echo_engine` carga por defecto **`assets/atlas/tiles.png`**: arte pixel de 128×128 (16 familias × 16
máscaras de 4 vecinos, tiles 8×8) generado por el script reproducible
[`tools/gen_atlas.py`](tools/gen_atlas.py) (`python3 tools/gen_atlas.py -o assets/atlas/tiles.png` y
`--check` para auditar que el PNG commiteado no se desfasó). RGB = albedo, A = altura.

`engine/render/tile_atlas.{hpp,cpp}` también genera un atlas procedural 128×128 RGBA8 (sin GL)
y puede **cargar cualquier PNG** del mismo tamaño (`load_tile_atlas_file` / `load_tile_atlas_mem`, con
`stb_image` v2.32 vendorizado, solo PNG). Si el archivo falta, está roto o no mide 128×128, se usa el
procedural (nunca queda un atlas vacío). `echo_engine --atlas FILE.png` o la env `EW_ATLAS` pisan el default.
`ewmapc` aplica **autotiling de 4 vecinos** (bits N=1, E=2, S=4, W=8) sobre `VIS` y `FG`: el tile del YAML
es la familia (`(tile%15)+1`, 1..15) y el `.ewmap` guarda `familia*16+máscara` (16..255; el 0 queda vacío).
El atlas pone la familia en la fila y la máscara en la columna; bisel (albedo y altura) solo en lados sin
vecino. El shader `gbuffer_map.frag` deriva la normal de la altura y aplica el mismo bisel según la máscara.
Estilos por familia (`id/16 % 4`): liso, ladrillo, granulado, vetas. En el YAML, `autotile: false` lo apaga.
Tests: `autotile_test`, `atlas_test` (`EW_ATLAS_PPM=atlas.ppm ./build/atlas_test` vuelca el atlas;
carga `tests/fixtures/atlas_128.png` y el default del repo, comprueba el fallback). `atlas_gen_check` corre `gen_atlas.py --check`. `fuzz_atlas` cubre el loader con libFuzzer.

El job `engine` de CI corre el prototipo headless (`xvfb-run`, `--frames 90 --demo --dump frame.ppm --nosound`) y sube el PPM 320×180 como artifact `engine-frame` (se puede abrir con cualquier visor; `EW_ATLAS_PPM` hace lo mismo con el atlas).

Los jobs `package-linux` y `package-windows` (push a `main`, tags y PRs) publican **artifacts de CI**
(`echo-well-linux-x86_64`, `echo-well-windows-x86_64`): `echo_engine`, `ewmapc`, shaders, atlas y mapas
compilados, listos para bajar desde la pestaña Actions. **No** son un release público ni una publicación
en itch.io. El binario resuelve assets junto a sí mismo (`assets/…`); ver `assets/DIST.txt`. Instalación
local: `cmake --install build --prefix dist/echo-well`.

Perf: el timestep es fijo (`Sim::kDt` = 1/60, sin reloj de pared). `bench_test` verifica determinismo y reporta us/paso (umbral holgado 2 ms, CI). `echo_engine --frames N` imprime ms/frame al salir. Colisión de puertas usa un cache de AABBs (sin `strcmp` por tile).

Menú de pausa: `PauseMenu` en `engine/game/menu.hpp` (sin GL, testeable: `menu_test`). Esc/Start abre y cierra; ↑/↓ navega, Enter/Espacio/A confirma, ←/→ ajusta el volumen (pasos de 0.1), confirmar en Volumen mutea. El mixer aplica ganancia/mute (thread-safe). La simulación se congela mientras el menú está abierto; `--demo` no lo abre.

SFX: `engine/audio` genera beeps de seno (salto, muerte, checkpoint, meta, switch, splash, pickup, habilidad). Test `sfx_test` (sin dispositivo). `echo_engine` los reproduce con miniaudio (se descarga al compilar, no está vendorizado) y si no hay hardware o se pasa `--nosound`/`EW_NOSOUND` sigue en silencio. Se desactiva con `-DEW_AUDIO=OFF`.

Seguridad (fuzzing, sanitizers, branch protection): ver [docs/security.md](docs/security.md).
