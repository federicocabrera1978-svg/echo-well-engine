cmake_minimum_required(VERSION 3.20)
project(echo_well_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
add_test(NAME ewmap_load_hello
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/hello.ewmap)
set_tests_properties(ewmap_load_hello PROPERTIES DEPENDS ewmapc_hello)
