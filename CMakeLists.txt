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
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(WIN32)
  # windows.h define min/max como macros y rompe std::min/std::max.
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
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
add_test(NAME ewmapc_room
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/room.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/room.ewmap)
add_test(NAME ewmapc_mover
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/mover.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/mover.ewmap)
add_test(NAME ewmapc_gravity
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/gravity.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/gravity.ewmap)
add_test(NAME ewmapc_boss
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/boss.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/boss.ewmap)
add_test(NAME ewmapc_ability
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/ability.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/ability.ewmap)
add_test(NAME ewmapc_dash
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/dash.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/dash.ewmap)
add_test(NAME ewmapc_wallclimb
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/wallclimb.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/wallclimb.ewmap)
add_test(NAME ewmapc_level2
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/level2.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/level2.ewmap)
add_test(NAME ewmapc_level3
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/level3.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/level3.ewmap)
add_test(NAME ewmapc_world
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/world.yaml
          -o ${CMAKE_CURRENT_BINARY_DIR}/world.ewmap)
add_test(NAME ewmap_load_hello
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/hello.ewmap
          --name hello --chunks 2 --ents 3 --links 1 --water 2 --ropes 2 --backdrops 0 --guides 0
          --has player_spawn --has switch --has door)
add_test(NAME ewmap_load_slice
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/slice.ewmap
          --name slice --chunks 8 --ents 22 --links 4 --water 2 --ropes 2 --movers 3 --has mover --has gravity_flip --has boss --has checkpoint --has flood_switch --has goal --has collectible
          --has player_spawn --has switch --has door --has goal --has npc --backdrops 8 --dialogs 2 --guides 2)
add_test(NAME ewmap_load_room
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/room.ewmap
          --name room --chunks 1 --ents 2 --has player_spawn --has goal)
add_test(NAME ewmap_load_mover
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/mover.ewmap
          --name mover --chunks 1 --ents 3 --ropes 1 --water 1 --movers 2 --backdrops 1
          --has player_spawn --has mover)
add_test(NAME ewmap_load_gravity
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/gravity.ewmap
          --name gravity --chunks 1 --ents 6 --ropes 1 --water 1 --movers 2 --backdrops 1
          --has player_spawn --has gravity_flip --has checkpoint --has mover)
set_tests_properties(ewmap_load_gravity PROPERTIES DEPENDS ewmapc_gravity)
add_test(NAME ewmap_load_boss
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/boss.ewmap
          --name boss --chunks 1 --ents 5 --links 1
          --has player_spawn --has boss --has door --has goal --has checkpoint --backdrops 1)
set_tests_properties(ewmap_load_boss PROPERTIES DEPENDS ewmapc_boss)
add_test(NAME ewmap_load_ability
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/ability.ewmap
          --name ability --chunks 1 --ents 4 --ropes 1 --backdrops 1
          --has player_spawn --has ability_double_jump --has checkpoint --has goal)
set_tests_properties(ewmap_load_ability PROPERTIES DEPENDS ewmapc_ability)
add_test(NAME ewmap_load_dash
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/dash.ewmap
          --name dash --chunks 1 --ents 6 --ropes 1 --backdrops 1
          --has player_spawn --has ability_dash --has break_wall --has switch --has checkpoint --has goal)
set_tests_properties(ewmap_load_dash PROPERTIES DEPENDS ewmapc_dash)
add_test(NAME ewmap_load_wallclimb
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/wallclimb.ewmap
          --name wallclimb --chunks 1 --ents 4 --backdrops 1
          --has player_spawn --has ability_wall_climb --has checkpoint --has goal)
set_tests_properties(ewmap_load_wallclimb PROPERTIES DEPENDS ewmapc_wallclimb)
add_test(NAME ewmap_load_level2
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/level2.ewmap
          --name level2 --chunks 8 --ents 22 --links 2 --water 1 --ropes 1 --movers 1 --backdrops 8 --dialogs 6 --guides 5
          --has player_spawn --has ability_double_jump --has ability_dash --has break_wall --has collectible --has switch --has door
          --has mover --has gravity_flip --has boss --has checkpoint --has goal --has npc --has lore)
set_tests_properties(ewmap_load_level2 PROPERTIES DEPENDS ewmapc_level2)
add_test(NAME ewmap_load_level3
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/level3.ewmap
          --name level3 --chunks 6 --ents 22 --links 2 --water 1 --ropes 1 --movers 1 --backdrops 6 --dialogs 4 --guides 3
          --has player_spawn --has ability_double_jump --has ability_dash --has break_wall --has collectible --has switch --has door
          --has mover --has gravity_flip --has boss_dragon --has imp --has checkpoint --has goal --has npc --has lore)
set_tests_properties(ewmap_load_level3 PROPERTIES DEPENDS ewmapc_level3)
add_test(NAME ewmap_load_world
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/world.ewmap
          --name world --chunks 24 --ents 71 --links 9 --water 6 --ropes 4 --movers 7 --backdrops 24 --dialogs 16 --guides 12
          --has player_spawn --has ability_double_jump --has ability_dash --has ability_wall_climb --has break_wall --has collectible --has switch --has flood_switch --has door
          --has one_way_door --has mover --has elevator --has gravity_flip --has boss --has boss_dragon --has imp --has checkpoint --has goal --has npc --has lore)
set_tests_properties(ewmap_load_world PROPERTIES DEPENDS ewmapc_world)
# Fondos (BACKDROP): fuente mínima válida + fuentes que ewmapc tiene que rechazar.
set(EW_BD_FIX ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/backdrops)
add_test(NAME ewmapc_backdrop_ok
  COMMAND ewmapc --crc ${EW_BD_FIX}/ok.yaml -o ${CMAKE_CURRENT_BINARY_DIR}/backdrop_ok.ewmap)
add_test(NAME ewmap_load_backdrop_ok
  COMMAND ewmap_smoke ${CMAKE_CURRENT_BINARY_DIR}/backdrop_ok.ewmap --name backdrop_ok --chunks 2 --ents 1 --backdrops 1)
set_tests_properties(ewmap_load_backdrop_ok PROPERTIES DEPENDS ewmapc_backdrop_ok)
foreach(bad bad_theme out_of_range duplicate bad_seed)
  add_test(NAME ewmapc_backdrop_rejects_${bad}
    COMMAND ewmapc --crc ${EW_BD_FIX}/${bad}.yaml -o ${CMAKE_CURRENT_BINARY_DIR}/backdrop_${bad}.ewmap)
  set_tests_properties(ewmapc_backdrop_rejects_${bad} PROPERTIES WILL_FAIL TRUE)
endforeach()
set_tests_properties(ewmap_load_hello PROPERTIES DEPENDS ewmapc_hello)
set_tests_properties(ewmap_load_slice PROPERTIES DEPENDS ewmapc_slice)
set_tests_properties(ewmap_load_room PROPERTIES DEPENDS ewmapc_room)
set_tests_properties(ewmap_load_mover PROPERTIES DEPENDS ewmapc_mover)

# --- echo_game: escena sin GL (MapLoader -> colisión, entidades, cámara) -----
add_library(echo_physics engine/physics/verlet.cpp engine/physics/water.cpp)
target_include_directories(echo_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)

add_library(echo_game engine/game/scene.cpp engine/game/sim.cpp engine/game/save.cpp engine/game/level_flow.cpp)
target_link_libraries(echo_game PUBLIC echo_map echo_physics)

# Mapas compilados en el build (CRC siempre on). world = mapa unificado de campaña por defecto.
set(EW_HELLO_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/hello.ewmap)
set(EW_SLICE_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/slice.ewmap)
set(EW_ROOM_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/room.ewmap)
set(EW_MOVER_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/mover.ewmap)
set(EW_GRAVITY_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/gravity.ewmap)
set(EW_BOSS_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/boss.ewmap)
set(EW_ABILITY_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/ability.ewmap)
set(EW_DASH_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/dash.ewmap)
set(EW_WALLCLIMB_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/wallclimb.ewmap)
set(EW_LEVEL2_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/level2.ewmap)
set(EW_LEVEL3_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/level3.ewmap)
set(EW_WORLD_MAP ${CMAKE_CURRENT_BINARY_DIR}/maps/world.ewmap)
add_custom_command(OUTPUT ${EW_HELLO_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/hello.yaml -o ${EW_HELLO_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/hello.yaml)
add_custom_command(OUTPUT ${EW_SLICE_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/slice.yaml -o ${EW_SLICE_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/slice.yaml)
add_custom_command(OUTPUT ${EW_ROOM_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/room.yaml -o ${EW_ROOM_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/room.yaml)
add_custom_command(OUTPUT ${EW_MOVER_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/mover.yaml -o ${EW_MOVER_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/mover.yaml)
add_custom_command(OUTPUT ${EW_GRAVITY_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/gravity.yaml -o ${EW_GRAVITY_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/gravity.yaml)
add_custom_command(OUTPUT ${EW_BOSS_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/boss.yaml -o ${EW_BOSS_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/boss.yaml)
add_custom_command(OUTPUT ${EW_ABILITY_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/ability.yaml -o ${EW_ABILITY_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/ability.yaml)
add_custom_command(OUTPUT ${EW_DASH_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/dash.yaml -o ${EW_DASH_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/dash.yaml)
add_custom_command(OUTPUT ${EW_WALLCLIMB_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/wallclimb.yaml -o ${EW_WALLCLIMB_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/wallclimb.yaml)
add_custom_command(OUTPUT ${EW_LEVEL2_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/level2.yaml -o ${EW_LEVEL2_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/level2.yaml)
add_custom_command(OUTPUT ${EW_LEVEL3_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/level3.yaml -o ${EW_LEVEL3_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/level3.yaml)
add_custom_command(OUTPUT ${EW_WORLD_MAP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/maps
  COMMAND ewmapc --crc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/world.yaml -o ${EW_WORLD_MAP}
  DEPENDS ewmapc ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/world.yaml)
add_custom_target(ew_maps ALL DEPENDS ${EW_HELLO_MAP} ${EW_SLICE_MAP} ${EW_ROOM_MAP} ${EW_MOVER_MAP} ${EW_GRAVITY_MAP} ${EW_BOSS_MAP} ${EW_ABILITY_MAP} ${EW_DASH_MAP} ${EW_WALLCLIMB_MAP} ${EW_LEVEL2_MAP} ${EW_LEVEL3_MAP} ${EW_WORLD_MAP})

add_executable(scene_test tests/scene_test.cpp)
target_link_libraries(scene_test PRIVATE echo_game)
add_dependencies(scene_test ew_maps)
add_test(NAME scene_hello COMMAND scene_test ${EW_HELLO_MAP})
add_executable(physics_test tests/physics_test.cpp)
target_link_libraries(physics_test PRIVATE echo_game)
add_dependencies(physics_test ew_maps)
add_test(NAME physics_hello COMMAND physics_test ${EW_HELLO_MAP})
add_executable(mover_test tests/mover_test.cpp)
target_link_libraries(mover_test PRIVATE echo_game)
add_dependencies(mover_test ew_maps)
add_test(NAME physics_movers COMMAND mover_test ${EW_MOVER_MAP})
add_executable(gravity_test tests/gravity_test.cpp)
target_link_libraries(gravity_test PRIVATE echo_game)
add_dependencies(gravity_test ew_maps)
add_test(NAME gravity_flip COMMAND gravity_test ${EW_GRAVITY_MAP})
add_executable(boss_test tests/boss_test.cpp)
target_link_libraries(boss_test PRIVATE echo_game)
add_dependencies(boss_test ew_maps)
add_test(NAME boss_fight COMMAND boss_test ${EW_BOSS_MAP})
add_executable(boss_victory_test tests/boss_victory_test.cpp)
target_link_libraries(boss_victory_test PRIVATE echo_game)
add_dependencies(boss_victory_test ew_maps)
add_test(NAME boss_victory_seq COMMAND boss_victory_test ${EW_SLICE_MAP} ${EW_LEVEL2_MAP} ${EW_LEVEL3_MAP})
add_executable(ability_test tests/ability_test.cpp)
target_link_libraries(ability_test PRIVATE echo_game)
add_dependencies(ability_test ew_maps)
add_test(NAME ability_double_jump COMMAND ability_test ${EW_ABILITY_MAP})
add_executable(game_feel_test tests/game_feel_test.cpp)
target_link_libraries(game_feel_test PRIVATE echo_game)
add_dependencies(game_feel_test ew_maps)
add_test(NAME game_feel COMMAND game_feel_test ${EW_ABILITY_MAP} ${EW_BOSS_MAP} ${EW_DASH_MAP})
add_executable(dash_test tests/dash_test.cpp)
target_link_libraries(dash_test PRIVATE echo_game)
add_dependencies(dash_test ew_maps)
add_test(NAME dash_breakwall COMMAND dash_test ${EW_DASH_MAP})
add_executable(wallclimb_test tests/wallclimb_test.cpp)
target_link_libraries(wallclimb_test PRIVATE echo_game echo_audio)
add_dependencies(wallclimb_test ew_maps)
add_test(NAME wall_climb_secret COMMAND wallclimb_test ${EW_WALLCLIMB_MAP})
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
add_executable(level2_test tests/level2_test.cpp)
target_link_libraries(level2_test PRIVATE echo_game)
add_dependencies(level2_test ew_maps)
add_test(NAME level2_path COMMAND level2_test ${EW_LEVEL2_MAP} ${EW_SLICE_MAP})
add_executable(level3_test tests/level3_test.cpp)
target_link_libraries(level3_test PRIVATE echo_game)
add_dependencies(level3_test ew_maps)
add_test(NAME level3_path COMMAND level3_test ${EW_LEVEL3_MAP} ${EW_LEVEL2_MAP})
add_executable(stage9_test tests/stage9_test.cpp)
target_link_libraries(stage9_test PRIVATE echo_game)
add_dependencies(stage9_test ew_maps)
add_test(NAME minimap_imps_victory COMMAND stage9_test ${EW_SLICE_MAP} ${EW_LEVEL3_MAP})
add_executable(minimap_test tests/minimap_test.cpp)
target_link_libraries(minimap_test PRIVATE echo_game)
add_dependencies(minimap_test ew_maps)
add_test(NAME minimap_hud COMMAND minimap_test ${EW_SLICE_MAP} ${EW_LEVEL2_MAP} ${EW_LEVEL3_MAP})
add_executable(metroidvania_test tests/metroidvania_test.cpp)
target_link_libraries(metroidvania_test PRIVATE echo_game)
add_dependencies(metroidvania_test ew_maps)
add_test(NAME metroidvania_campaign COMMAND metroidvania_test ${EW_SLICE_MAP} ${EW_LEVEL2_MAP} ${EW_LEVEL3_MAP})
add_executable(world_test tests/world_test.cpp)
target_link_libraries(world_test PRIVATE echo_game echo_audio)
add_dependencies(world_test ew_maps)
add_test(NAME world_metroidvania COMMAND world_test ${EW_WORLD_MAP})
add_executable(rewards_test tests/rewards_test.cpp)
target_link_libraries(rewards_test PRIVATE echo_game echo_audio)
add_dependencies(rewards_test ew_maps)
add_test(NAME collectible_rewards COMMAND rewards_test ${EW_WORLD_MAP})
add_executable(backdrop_test tests/backdrop_test.cpp)
target_link_libraries(backdrop_test PRIVATE echo_game)
add_dependencies(backdrop_test ew_maps)
add_test(NAME backdrops COMMAND backdrop_test ${EW_SLICE_MAP} ${EW_HELLO_MAP} ${CMAKE_CURRENT_BINARY_DIR}/backdrop_ok.ewmap ${EW_LEVEL2_MAP})
set_tests_properties(backdrops PROPERTIES DEPENDS ewmapc_backdrop_ok)
add_executable(lighting_test tests/lighting_test.cpp)
target_link_libraries(lighting_test PRIVATE echo_game)
add_dependencies(lighting_test ew_maps)
add_test(NAME room_lighting COMMAND lighting_test ${EW_SLICE_MAP} ${EW_LEVEL2_MAP} ${EW_LEVEL3_MAP} ${EW_WORLD_MAP})
add_executable(bench_test tests/bench_test.cpp)
target_link_libraries(bench_test PRIVATE echo_game)
add_test(NAME bench COMMAND bench_test ${EW_SLICE_MAP})
add_executable(rope_wall_test tests/rope_wall_test.cpp)
target_link_libraries(rope_wall_test PRIVATE echo_physics)
add_test(NAME rope_wall COMMAND rope_wall_test)

# --- echo_atlas: atlas de tiles y sprites procedurales (sin GL, testeable en CI) --------
add_library(echo_atlas engine/render/tile_atlas.cpp engine/render/sprite_atlas.cpp)
target_include_directories(echo_atlas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine
                                      PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
add_executable(atlas_test tests/atlas_test.cpp)
target_link_libraries(atlas_test PRIVATE echo_atlas)
add_test(NAME tile_atlas COMMAND atlas_test)
set(EW_DEFAULT_ATLAS "${CMAKE_CURRENT_SOURCE_DIR}/assets/atlas/tiles.png")
set(EW_DEFAULT_SPRITES "${CMAKE_CURRENT_SOURCE_DIR}/assets/atlas/sprites.png")
set_tests_properties(tile_atlas PROPERTIES
  ENVIRONMENT "EW_TEST_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures;EW_DEFAULT_ATLAS=${EW_DEFAULT_ATLAS}")
add_executable(sprite_test tests/sprite_test.cpp)
target_link_libraries(sprite_test PRIVATE echo_atlas echo_game)
add_dependencies(sprite_test ew_maps)
add_test(NAME protagonist_sprite COMMAND sprite_test ${EW_ABILITY_MAP})
set_tests_properties(protagonist_sprite PROPERTIES
  ENVIRONMENT "EW_TEST_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures;EW_DEFAULT_SPRITES=${EW_DEFAULT_SPRITES}")
find_program(EW_PYTHON python3)
if(EW_PYTHON)
  add_test(NAME atlas_gen_check
    COMMAND ${EW_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_atlas.py --check ${EW_DEFAULT_ATLAS})
  add_test(NAME sprites_gen_check
    COMMAND ${EW_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_sprites.py --check ${EW_DEFAULT_SPRITES})
  add_test(NAME world_gen_check
    COMMAND ${EW_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_world.py --check ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/src/world.yaml)
endif()
add_executable(autotile_test tests/autotile_test.cpp)
target_link_libraries(autotile_test PRIVATE echo_map)
add_test(NAME autotile COMMAND autotile_test)
# --- echo_audio: SFX procedural, sin dispositivo (testeable en CI) ----------
add_library(echo_audio engine/audio/sfx.cpp)
target_include_directories(echo_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
add_executable(sfx_test tests/sfx_test.cpp)
target_link_libraries(sfx_test PRIVATE echo_audio)
add_test(NAME sfx COMMAND sfx_test)

add_executable(audio_feel_test tests/audio_feel_test.cpp)
target_link_libraries(audio_feel_test PRIVATE echo_game echo_audio)
add_dependencies(audio_feel_test ew_maps)
add_test(NAME audio_feel COMMAND audio_feel_test ${EW_WORLD_MAP} ${EW_MOVER_MAP})

add_executable(gamepad_test tests/gamepad_test.cpp)
target_link_libraries(gamepad_test PRIVATE echo_game)
add_test(NAME gamepad COMMAND gamepad_test)

add_executable(save_test tests/save_test.cpp)
target_link_libraries(save_test PRIVATE echo_game)
add_dependencies(save_test ew_maps)
add_test(NAME save_game COMMAND save_test ${EW_SLICE_MAP})

add_executable(bindings_test tests/bindings_test.cpp)
target_link_libraries(bindings_test PRIVATE echo_game)
add_test(NAME controls_remap COMMAND bindings_test)

add_executable(menu_test tests/menu_test.cpp)
target_link_libraries(menu_test PRIVATE echo_game)
add_test(NAME pause_menu COMMAND menu_test)

add_executable(wind_test tests/wind_test.cpp)
target_link_libraries(wind_test PRIVATE echo_game)
add_dependencies(wind_test ew_maps)
add_test(NAME wind_breeze COMMAND wind_test ${EW_SLICE_MAP})

add_executable(end_screen_test tests/end_screen_test.cpp)
target_link_libraries(end_screen_test PRIVATE echo_game)
add_dependencies(end_screen_test ew_maps)
add_test(NAME end_screen COMMAND end_screen_test ${EW_SLICE_MAP} ${EW_ABILITY_MAP})

add_executable(level_flow_test tests/level_flow_test.cpp)
target_link_libraries(level_flow_test PRIVATE echo_game)
add_dependencies(level_flow_test ew_maps)
add_test(NAME level_flow COMMAND level_flow_test ${EW_SLICE_MAP} ${EW_LEVEL2_MAP})

add_executable(dialogue_test tests/dialogue_test.cpp tools/ewmapc/writer.cpp)
target_include_directories(dialogue_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools/ewmapc)
target_link_libraries(dialogue_test PRIVATE echo_game ew_yaml_lite)
add_dependencies(dialogue_test ew_maps)
add_test(NAME dialogue_lore COMMAND dialogue_test ${EW_SLICE_MAP} ${EW_LEVEL2_MAP} ${EW_HELLO_MAP})

add_executable(guide_test tests/guide_test.cpp)
target_link_libraries(guide_test PRIVATE echo_game)
add_dependencies(guide_test ew_maps)
add_test(NAME guide_arrows COMMAND guide_test ${EW_SLICE_MAP} ${EW_LEVEL2_MAP} ${EW_LEVEL3_MAP} ${EW_HELLO_MAP})

add_executable(mechanisms_crt_test tests/mechanisms_crt_test.cpp)
target_link_libraries(mechanisms_crt_test PRIVATE echo_game echo_audio)
add_dependencies(mechanisms_crt_test ew_maps)
add_test(NAME mechanisms_crt COMMAND mechanisms_crt_test ${EW_SLICE_MAP} ${EW_LEVEL3_MAP})

add_executable(world_mechanisms_test tests/world_mechanisms_test.cpp tools/ewmapc/writer.cpp)
target_include_directories(world_mechanisms_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools/ewmapc)
target_link_libraries(world_mechanisms_test PRIVATE echo_game echo_audio ew_yaml_lite)
add_test(NAME world_mechanisms COMMAND world_mechanisms_test)

add_executable(paths_test tests/paths_test.cpp)
target_include_directories(paths_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/engine)
add_test(NAME asset_paths COMMAND paths_test)

# --- fuzzing: MapLoader (.ewmap), yaml_lite, atlas PNG y save (.ewsv) ----------------------
# Siempre se compilan con un driver standalone (mutaciones deterministas) y
# corren en ctest; con EW_LIBFUZZER=ON se linkean contra libFuzzer.
add_library(ew_yaml_lite tools/ewmapc/yaml_lite.cpp)
target_include_directories(ew_yaml_lite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tools/ewmapc)
set(EW_FUZZ_CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/corpus)
foreach(t map_loader yaml_lite atlas save)
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
target_link_libraries(fuzz_atlas PRIVATE echo_atlas)
target_link_libraries(fuzz_save PRIVATE echo_game)
if(NOT EW_LIBFUZZER)
  add_test(NAME fuzz_map_loader_smoke COMMAND fuzz_map_loader -runs=20000 ${EW_FUZZ_CORPUS}/ewmap)
  add_test(NAME fuzz_yaml_lite_smoke  COMMAND fuzz_yaml_lite  -runs=20000 ${EW_FUZZ_CORPUS}/yaml)
  add_test(NAME fuzz_atlas_smoke      COMMAND fuzz_atlas      -runs=20000 ${EW_FUZZ_CORPUS}/png)
  add_test(NAME fuzz_save_smoke       COMMAND fuzz_save       -runs=20000 ${EW_FUZZ_CORPUS}/save)
endif()

# --- echo_engine: ventana + loop + stub Deferred (GLFW 3.4 vía FetchContent) --
option(EW_BUILD_ENGINE "Compilar echo_engine (requiere OpenGL + headers X11/Wayland)" ON)
option(EW_AUDIO "SFX con miniaudio en echo_engine (fail-soft si no hay dispositivo)" ON)
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
    EW_DEFAULT_MAP="${EW_WORLD_MAP}"
    EW_DEFAULT_ATLAS="${EW_DEFAULT_ATLAS}"
    EW_DEFAULT_SPRITES="${EW_DEFAULT_SPRITES}"
    EW_VERSION="${PROJECT_VERSION}")
  target_link_libraries(echo_engine PRIVATE echo_render echo_game echo_audio glfw)
  add_dependencies(echo_engine ew_maps)
  # device.cpp siempre (stubs si EW_AUDIO=OFF). miniaudio no se vendoriza (~4 MiB);
  # se descarga al build y, si falla el dispositivo, el motor sigue en silencio.
  target_sources(echo_engine PRIVATE engine/audio/device.cpp)
  if(EW_AUDIO)
    set(_MA_H ${CMAKE_BINARY_DIR}/miniaudio.h)
    if(NOT EXISTS "${_MA_H}")
      if(DEFINED ENV{EW_MINIAUDIO} AND EXISTS "$ENV{EW_MINIAUDIO}")
        file(COPY "$ENV{EW_MINIAUDIO}" DESTINATION ${CMAKE_BINARY_DIR})
      elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/miniaudio/miniaudio.h")
        file(COPY "${CMAKE_SOURCE_DIR}/third_party/miniaudio/miniaudio.h" DESTINATION ${CMAKE_BINARY_DIR})
      else()
        file(DOWNLOAD
          https://raw.githubusercontent.com/mackron/miniaudio/0.11.22/miniaudio.h
          ${_MA_H} STATUS _MA_ST TIMEOUT 60
          EXPECTED_HASH SHA256=9019743287e443c55e5737a7297f38e5e358561701d6db2d905afb114390c410)
        list(GET _MA_ST 0 _MA_CODE)
        if(NOT _MA_CODE EQUAL 0)
          message(WARNING "no se pudo descargar miniaudio.h: ${_MA_ST} — SFX solo stubs")
          set(EW_AUDIO OFF)
        endif()
      endif()
    endif()
  endif()
  if(EW_AUDIO AND EXISTS "${CMAKE_BINARY_DIR}/miniaudio.h")
    target_compile_definitions(echo_engine PRIVATE EW_AUDIO=1)
    target_include_directories(echo_engine PRIVATE ${CMAKE_BINARY_DIR})
    if(MSVC)
      set_source_files_properties(engine/audio/device.cpp PROPERTIES COMPILE_OPTIONS /W0)
    else()
      set_source_files_properties(engine/audio/device.cpp PROPERTIES COMPILE_OPTIONS -w)
    endif()
    if(UNIX)
      find_package(Threads REQUIRED)
      target_link_libraries(echo_engine PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
  endif()
endif()

# --- instalación portable (artifacts de CI, no un release público) ----------
# Prefijo típico: cmake --install build --prefix dist/echo-well
# Layout: echo_engine, ewmapc, README, assets/{shaders,atlas,maps/bin}.
install(TARGETS ewmapc ewmap_smoke RUNTIME DESTINATION .)
if(EW_BUILD_ENGINE)
  install(TARGETS echo_engine RUNTIME DESTINATION .)
endif()
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders DESTINATION assets)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/atlas/tiles.png
              ${CMAKE_CURRENT_SOURCE_DIR}/assets/atlas/sprites.png
        DESTINATION assets/atlas)
install(FILES ${EW_HELLO_MAP} ${EW_SLICE_MAP} ${EW_ROOM_MAP} ${EW_MOVER_MAP} ${EW_GRAVITY_MAP} ${EW_BOSS_MAP} ${EW_ABILITY_MAP} ${EW_DASH_MAP} ${EW_WALLCLIMB_MAP} ${EW_LEVEL2_MAP} ${EW_LEVEL3_MAP} ${EW_WORLD_MAP}
        DESTINATION assets/maps/bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION .)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/docs/qa-checklist.md DESTINATION docs)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/DIST.txt DESTINATION .)
