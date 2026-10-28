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
./build/echo_engine --demo        # input automático (camina y salta)
```
Controles: **←/→** o **A/D** caminar, **Espacio/↑/Z** saltar (también nadar hacia arriba en el agua), **E/X/↓** activar el switch cercano, **ESC** salir. El build compila `assets/maps/src/hello.yaml` → `build/maps/hello.ewmap` (con CRC) y lo usa por defecto.

### Escena (MapLoader → render)
- `ew::game::Scene` carga el `.ewmap`, lee tiles vía `ChunkCache` y expone `solid_px()` (capa `COL`; fuera del mapa = sólido), entidades y una cámara que sigue al player clampada al mapa. El **chunk activo** sale de la posición del player.
- Pipeline: geometry pass → los tiles `COL`/`VIS` del chunk activo se suben como textura `RG8UI` 40×23 (solo cuando cambia el chunk) y el shader los dibuja con bisel por tile; marcadores (player, `switch`, `door`, `player_spawn`) van como uniforms → G-buffer 320×180 (albedo + normal + depth24) → light pass (ambiente + luz puntual que sigue al player) → blit NEAREST con escalado entero y letterbox.

### Switch → puerta
- Los `links:` del mapa (sección `LINK`) conectan `switch` → `door`. Con el player a ≤ 14 px del switch, **E/X/↓** lo alterna y abre/cierra todas sus puertas enlazadas.
- Puerta = 1×3 tiles (8×24 px) desde su `(x, y)`. **Cerrada** es sólida (`Scene::solid_tile` la suma a `COL`); **abierta** no colisiona.
- Feedback: el switch tiene luz **ámbar** (apagado) / **verde** (encendido); la puerta cerrada son 3 bloques violeta, abierta queda solo un marco tenue arriba.
- En `hello`, la puerta (x=280) bloquea el paso hacia el borde derecho hasta activar `switch_a` (x=96). `door_test` (ctest) lo verifica.

### Física Verlet
- `ew::physics::World`: puntos con `pos/prev` (Verlet posicional), gravedad 600 px/s², damping, constraints de distancia (`Stick`, con rigidez), pares círculo↔círculo y colisión círculo↔tiles sólidos (empuje por punto más cercano + fricción tangencial, sin rebote). 6 iteraciones por paso.
- `ew::game::Sim` une `Scene` + `World` a paso fijo 60 Hz: el player es un círculo r=4 que cae, camina (90 px/s) y salta (solo apoyado) contra la capa `COL`; una cuerda de 16 segmentos cuelga del techo y el player la empuja.
- `physics_test` (ctest) verifica apoyo en el piso, caminar, salto, paredes y largo de la cuerda.

### Agua Hooke
- Cada registro `water:` del mapa (sección `WATER`) crea un `ew::physics::WaterBody`: la superficie se divide en columnas (~2 px, máx. 64) y cada una es un resorte de Hooke `a = -k·x - d·v` con `k = stiffness` y `d = damping` del mapa (defaults 0.025 / 0.03 si vienen en 0). Las columnas se acoplan con sus vecinas (`spread`) para propagar ondas.
- Interacción con Verlet: empuje proporcional a la fracción sumergida (el player flota en la superficie), arrastre dentro del agua, salpicadura al cruzar la superficie y estela suave al nadar en horizontal.
- Render: el geometry pass recibe la altura de cada columna como uniform y dibuja el volumen + una línea de superficie clara con normales según la pendiente (el light pass la ilumina). El renderer dibuja un cuerpo de agua por frame (el primero del mapa).
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
