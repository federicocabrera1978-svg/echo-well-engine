# echo-well-engine

Motor 2D propietario en **C++20 / GLSL** para un prototipo de metroidvania atmosférico estilo *Animal Well*.

- Render **Deferred** a resolución interna **320×180**, escalado entero.
- Física de **Verlet** (cuerdas, objetos, partículas).
- **Agua** con resortes de **Hooke** (superficie dinámica).
- Mapas en binario propio **`.ewmap`**, cargados por chunks con streaming LRU.

> Estado: existen el formato `.ewmap` (`ewmapc` + `echo_map`) y `echo_engine`: ventana + loop + stub Deferred 320×180. Física y agua vienen después.

## Estructura
```
engine/core/         echo_engine: ventana GLFW + loop de frame
engine/render/       echo_render: loader GL 3.3 mínimo + DeferredRenderer (G-buffer stub)
assets/shaders/      GLSL 330: fullscreen.vert, gbuffer_stub.frag, light.frag
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
```
ESC o cerrar la ventana para salir. Pipeline: geometry pass procedural → G-buffer 320×180 (albedo RGBA8 + normal RGBA8 + depth24) → light pass (ambiente + luz puntual animada) → blit NEAREST con escalado entero y letterbox. El geometry pass es un stub hasta enganchar `MapLoader`/`ChunkCache`.

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
