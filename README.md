# echo-well-engine

Motor 2D propietario en **C++20 / GLSL** para un prototipo de metroidvania atmosférico estilo *Animal Well*.

- Render **Deferred** a resolución interna **320×180**, escalado entero.
- Física de **Verlet** (cuerdas, objetos, partículas).
- **Agua** con resortes de **Hooke** (superficie dinámica).
- Mapas en binario propio **`.ewmap`**, cargados por chunks con streaming LRU.

> Estado: `.ewmap` (`ewmapc` + `echo_map`), escena (`echo_game`: MapLoader → colisión/entidades/cámara), física Verlet + agua Hooke (`echo_physics`) y `echo_engine` jugable sobre el Deferred 320×180.

## Estructura
```
engine/core/         echo_engine: ventana GLFW + loop de frame
engine/game/         echo_game: Scene (MapLoader + ChunkCache → tiles COL/VIS, entidades, cámara, chunk activo)
engine/physics/      echo_physics: Verlet (puntos, sticks, damping, colisión círculo↔tiles) + WaterBody (Hooke)
engine/render/       echo_render: loader GL 3.3 mínimo + DeferredRenderer
assets/shaders/      GLSL 330: fullscreen.vert, gbuffer_map.frag, light.frag
tests/               tests sin GL (ctest)
engine/map/          echo_map: formato .ewmap, MapLoader, ChunkCache (LRU), CRC-32
tools/ewmapc/        compilador YAML -> .ewmap (+ smoke test)
assets/maps/src/     fuentes de mapas (YAML)
assets/maps/bin/     mapas compilados (generados, ignorados por git)
docs/ewmap.md        especificación binaria
```

## Build
Requisitos: CMake ≥ 3.20 y un compilador C++20 (GCC 11+, Clang 14+, MSVC 19.30+). `echo_map`/`ewmapc` no tienen dependencias externas. `echo_engine` usa **GLFW 3.4** (descargado por CMake `FetchContent`, hash fijado) y OpenGL 3.3 core; sin GLAD (loader propio en `engine/render/gl_loader.*`). En Linux: `sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`. Para compilar solo herramientas: `-DEW_BUILD_ENGINE=OFF`.
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # compila hello.yaml y lo carga
```

## Correr el prototipo
```sh
./build/echo_engine              # ventana 1280×720 (320×180 ×4)
./build/echo_engine --scale 3    # 960×540
./build/echo_engine --frames 120 # sale tras N frames (smoke test / headless con xvfb-run)
./build/echo_engine --map otro.ewmap --dump frame.ppm  # otro mapa; vuelca el último frame 320×180
./build/echo_engine --demo        # input automático (camina, salta y activa el switch)
./build/echo_engine --start 330 150  # aparece en (x, y) px; p. ej. chunk 1 de hello
```
Controles: **←/→** o **A/D** caminar, **Espacio/↑/Z** saltar (también nadar hacia arriba en el agua), **E/X/↓** activar el switch cercano, **C/Shift** agarrar/soltar la cuerda, **ESC** salir. El build compila `assets/maps/src/hello.yaml` → `build/maps/hello.ewmap` (con CRC) y lo usa por defecto.

### Escena (MapLoader → render)
- `ew::game::Scene` carga el `.ewmap`, lee tiles vía `ChunkCache` y expone `solid_px()` (capa `COL`; fuera del mapa = sólido), entidades y una cámara que sigue al player clampada al mapa. El **chunk activo** sale de la posición del player.
- Pipeline: geometry pass → los tiles `COL`/`VIS` del vecindario 3×3 se suben como textura `RG8UI` 120×69 (solo cuando cambia el chunk) y el shader los dibuja con bisel por tile; marcadores (player, `switch`, `door`, `player_spawn`) van como uniforms → G-buffer 320×180 (albedo + normal + depth24) → light pass (ambiente + luz puntual que sigue al player) → blit NEAREST con escalado entero y letterbox.

### Chunks vecinos y cámara por pantallas
- Cada chunk (40×23 tiles = 320×184 px) es **una pantalla**, como en *Animal Well*. El renderer recibe el **vecindario 3×3** del chunk activo (textura `RG8UI` 120×69, re-subida solo al cambiar de chunk), así que se dibujan el activo y sus 8 vecinos (incluye los 4 ortogonales); los que caen fuera del mapa quedan vacíos.
- **Elección de cámara: slide corto, no snap.** Al cruzar un seam el chunk activo cambia en el acto (colisión/streaming) y la cámara **desliza** hacia la nueva pantalla con un ease exponencial (`kCamSlide = 12/s`, ~0.3 s; termina exacto en el origen del chunk). Dentro de una pantalla la cámara usa una **dead-zone** centrada (`kDeadHalfW = 48`, `kDeadHalfH = 24` px): mientras el player esté adentro no se mueve; al salir, la arrastra lo justo y se clampa a la pantalla del chunk. Al entrar a un chunk nuevo (o en snap) se re-centra clampado y el slide del seam sigue igual. Con chunks de 320×184 el efecto real es solo vertical (holgura 4 px); en chunks más grandes que la vista funciona en ambos ejes. Lógica pura en `Scene::dead_zone_step` (testeada en `scene_test`). Se redondea a px enteros al render (pixel-perfect). `follow(x, y)` sin `dt` hace snap (carga / `--start`).
- `hello` ahora es 2×1 chunks: el chunk 1 tiene plataformas escalonadas; se llega abriendo la puerta con el switch. Probar: activar el switch, caminar a la derecha y cruzar x=320, o `--start 330 150`.

### Switch → puerta
- Los `links:` del mapa (sección `LINK`) conectan `switch` → `door`. Con el player a ≤ 14 px del switch, **E/X/↓** lo alterna y abre/cierra todas sus puertas enlazadas.
- Puerta = 1×3 tiles (8×24 px) desde su `(x, y)`. **Cerrada** es sólida (`Scene::solid_tile` la suma a `COL`); **abierta** no colisiona.
- Feedback: el switch tiene luz **ámbar** (apagado) / **verde** (encendido); la puerta cerrada son 3 bloques violeta, abierta queda solo un marco tenue arriba.
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
- Límites: el player colgado no colisiona con tiles por sí mismo (lo hace el nodo, r=1.5), así que puede rozar paredes; sin animación de agarre.

### Agua Hooke
- Cada registro `water:` del mapa (sección `WATER`) crea un `ew::physics::WaterBody`: la superficie se divide en columnas (~2 px, máx. 64) y cada una es un resorte de Hooke `a = -k·x - d·v` con `k = stiffness` y `d = damping` del mapa (defaults 0.025 / 0.03 si vienen en 0). Las columnas se acoplan con sus vecinas (`spread`) para propagar ondas.
- Interacción con Verlet: empuje proporcional a la fracción sumergida (el player flota en la superficie), arrastre dentro del agua, salpicadura al cruzar la superficie y estela suave al nadar en horizontal.
- Render: el geometry pass recibe, por cuerpo, rect + n° de columnas + altura de cada columna (uniform arrays, stride 64) y dibuja el volumen + una línea de superficie clara con normales según la pendiente (el light pass la ilumina). Hasta **4 cuerpos por frame** (`kMaxWaterBodies`); se mandan solo los que tocan la vista.
- Contención: en `hello` cada pileta tiene piso y bordes `COL` de 3 tiles a ambos lados hasta la superficie (pileta 1 entre tiles x=19/28 en el chunk 0, pileta 2 entre x=62/71 en el chunk 1). La superficie además se clampa a ±90 % de la profundidad. `water_test` verifica que todo cuerpo del mapa esté contenido.
- `water_test` (ctest) verifica propagación, amortiguación, salpicadura y flotación.

## Formato `.ewmap` (resumen)
- Magic `EWMP`, little-endian, secciones alineadas a 8 bytes, versión 1.0.
- Tile 8×8 px, chunk 40×23 tiles; capas `COL`, `VIS`, `FG`, `META`.
- Secciones: `TILE=1`, `ENT=2`, `LINK=3`, `WATER=4`, `STRTAB=5`, `CRC=6`.
- Chunks raw en v1 (flag `compressed_chunks` reservado). CRC opcional; en CI siempre activo.

Detalle completo en [docs/ewmap.md](docs/ewmap.md).

## Uso de `ewmapc`
```sh
./build/ewmapc --crc assets/maps/src/hello.yaml -o assets/maps/bin/hello.ewmap
# ewmapc: assets/maps/src/hello.yaml -> assets/maps/bin/hello.ewmap (7728 bytes, 1 chunks, 3 entities, crc)
```
`ewmapc` valida su propia salida con `MapLoader` antes de escribirla.

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

`engine/render/tile_atlas.{hpp,cpp}` genera un atlas procedural 128×128 RGBA8 (16×16 tiles de 8×8, sin GL).
El id de tile es el valor **VIS** del mapa: `tile_uv(id) = ((id%16)*8, (id/16)*8)`. RGB = albedo, A = altura
(el shader `gbuffer_map.frag` deriva la normal de la altura + bisel de borde). Estilos por `id % 4`: liso,
ladrillo, granulado, vetas. Test: `atlas_test` (`EW_ATLAS_PPM=atlas.ppm ./build/atlas_test` vuelca el atlas).
