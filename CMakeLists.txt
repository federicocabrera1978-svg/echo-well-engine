cmake_minimum_required(VERSION 3.20)
project(echo_well_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizers (ASan + UBSan) para CI/debug: -DEW_SANITIZE=ON
option(EW_SANITIZE "Compilar con AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
# Fuzz targets con libFuzzer (requiere clang): -DEW_LIBFUZZER=ON
option(EW_LIBFUZZER "Linkear fuzz targets con libFuzzer (clang)" OFF)
if(EW_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- echo_map: formato .ewmap + MapLoader -----------------------------------
add_library(echo_map
  engine/map/crc32.cpp
  engine/map/map_loader.cpp
  engine/map/chunk_cache.cpp)
target_include_directories(echo_map PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)

# --- ewmapc: compilador YAML -> .ewmap --------------------------------------
add_executable(ewmapc
  tools/ewmapc/main.cpp
  tools/ewmapc/yaml_lite.cpp
  tools/ewmapc/writer.cpp)
target_link_libraries(ewmapc PRIVATE echo_map)

# --- smoke test: compila hello.yaml y lo carga con MapLoader -----------------
enable_testing()
add_executable(ewmap_smoke tools/ewmapc/smoke_test.cpp)
target_link_libraries(ewmap_smoke PRIVATE echo_map)
add_test(NAME ewmapc_hello
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/hello.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/hello.ewmap)
add_test(NAME ewmapc_slice
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/slice.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/slice.ewmap)
add_test(NAME ewmap_load_hello
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/hello.ewmap
          --name hello --chunks 2 --ents 3 --links 1 --water 2 --ropes 2
          --has player_spawn --has switch --has door)
add_test(NAME ewmap_load_slice
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/slice.ewmap
          --name slice --chunks 3 --ents 5 --links 1 --water 1 --ropes 1 --has checkpoint
          --has player_spawn --has switch --has door --has goal)
set_tests_properties(ewmap_load_hello PROPERTIES DEPENDS ewmapc_hello)
set_tests_properties(ewmap_load_slice PROPERTIES DEPENDS ewmapc_slice)

# --- echo_game: escena sin GL (MapLoader -> colisión, entidades, cámara) -----
add_library(echo_physics engine/physics/verlet.cpp engine/physics/water.cpp)
target_include_directories(echo_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)

add_library(echo_game engine/game/scene.cpp engine/game/sim.cpp)
target_link_libraries(echo_game PUBLIC echo_map echo_physics)

# Mapas compilados en el build (CRC siempre on). slice = camino jugable por defecto.
set(EW_HELLO_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/hello.ewmap)
set(EW_SLICE_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/slice.ewmap)
add_custom_command(OUTPUT ${EW_HELLO_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/hello.yaml -o ${EW_HELLO_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/hello.yaml)
add_custom_command(OUTPUT ${EW_SLICE_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/slice.yaml -o ${EW_SLICE_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/slice.yaml)
add_custom_target(ew_maps ALL DEPENDS ${EW_HELLO_MAP} ${EW_SLICE_MAP})

add_executable(scene_test tests/scene_test.cpp)
target_link_libraries(scene_test PRIVATE echo_game)
add_dependencies(scene_test ew_maps)
add_test(NAME scene_hello COMMAND scene_test ${EW_HELLO_MAP})
add_executable(physics_test tests/physics_test.cpp)
target_link_libraries(physics_test PRIVATE echo_game)
add_dependencies(physics_test ew_maps)
add_test(NAME physics_hello COMMAND physics_test ${EW_HELLO_MAP})
add_executable(water_test tests/water_test.cpp)
target_link_libraries(water_test PRIVATE echo_game)
add_dependencies(water_test ew_maps)
add_test(NAME water_hello COMMAND water_test ${EW_HELLO_MAP})
add_executable(door_test tests/door_test.cpp)
target_link_libraries(door_test PRIVATE echo_game)
add_dependencies(door_test ew_maps)
add_test(NAME door_hello COMMAND door_test ${EW_HELLO_MAP})
add_executable(rope_test tests/rope_test.cpp)
target_link_libraries(rope_test PRIVATE echo_game)
add_dependencies(rope_test ew_maps)
add_test(NAME rope_hello COMMAND rope_test ${EW_HELLO_MAP})
add_executable(slice_test tests/slice_test.cpp)
target_link_libraries(slice_test PRIVATE echo_game)
add_dependencies(slice_test ew_maps)
add_test(NAME slice_path COMMAND slice_test ${EW_SLICE_MAP})
add_executable(rope_wall_test tests/rope_wall_test.cpp)
target_link_libraries(rope_wall_test PRIVATE echo_physics)
add_test(NAME rope_wall COMMAND rope_wall_test)

# --- echo_atlas: atlas de tiles procedural (sin GL, testeable en CI) --------
add_library(echo_atlas engine/render/tile_atlas.cpp)
target_include_directories(echo_atlas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
add_executable(atlas_test tests/atlas_test.cpp)
target_link_libraries(atlas_test PRIVATE echo_atlas)
add_test(NAME tile_atlas COMMAND atlas_test)
add_executable(autotile_test tests/autotile_test.cpp)
target_link_libraries(autotile_test PRIVATE echo_map)
add_test(NAME autotile COMMAND autotile_test)

# --- fuzzing: MapLoader (.ewmap) y yaml_lite ---------------------------------
# Siempre se compilan con un driver standalone (mutaciones deterministas) y
# corren en ctest; con EW_LIBFUZZER=ON se linkean contra libFuzzer.
add_library(ew_yaml_lite tools/ewmapc/yaml_lite.cpp)
target_include_directories(ew_yaml_lite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tools/ewmapc)
set(EW_FUZZ_CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/corpus)
foreach(t map_loader yaml_lite)
  if(EW_LIBFUZZER)
    add_executable(fuzz_${t} tests/fuzz/fuzz_${t}.cpp)
    target_compile_options(fuzz_${t} PRIVATE -fsanitize=fuzzer)
    target_link_options(fuzz_${t} PRIVATE -fsanitize=fuzzer)
  else()
    add_executable(fuzz_${t} tests/fuzz/fuzz_${t}.cpp tests/fuzz/fuzz_driver.cpp)
  endif()
endforeach()
target_link_libraries(fuzz_map_loader PRIVATE echo_map)
target_link_libraries(fuzz_yaml_lite PRIVATE ew_yaml_lite)
if(NOT EW_LIBFUZZER)
  add_test(NAME fuzz_map_loader_smoke COMMAND fuzz_map_loader -runs=20000 ${EW_FUZZ_CORPUS}/ewmap)
  add_test(NAME fuzz_yaml_lite_smoke  COMMAND fuzz_yaml_lite  -runs=20000 ${EW_FUZZ_CORPUS}/yaml)
endif()

# --- echo_engine: ventana + loop + stub Deferred (GLFW 3.4 vía FetchContent) --
option(EW_BUILD_ENGINE "Compilar echo_engine (requiere OpenGL + headers X11/Wayland)" ON)
if(EW_BUILD_ENGINE)
  include(FetchContent)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9)
  FetchContent_MakeAvailable(glfw)
  if(NOT MSVC)
    target_compile_options(glfw PRIVATE -w)  # sin ruido de warnings de terceros
  endif()
  find_package(OpenGL REQUIRED)

  add_library(echo_render engine/render/gl_loader.cpp engine/render/deferred.cpp)
  target_include_directories(echo_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
  target_link_libraries(echo_render PUBLIC OpenGL::GL echo_atlas)

  add_executable(echo_engine engine/core/main.cpp)
  target_compile_definitions(echo_engine PRIVATE
    EW_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
    EW_DEFAULT_MAP="${EW_SLICE_MAP}")
  target_link_libraries(echo_engine PRIVATE echo_render echo_game glfw)
  add_dependencies(echo_engine ew_maps)
endif()
