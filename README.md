# echo-well-engine

Motor 2D propietario en **C++20 / GLSL** para un prototipo de metroidvania atmosférico estilo *Animal Well*.

- Render **Deferred** a resolución interna **320×180**, escalado entero.
- Física de **Verlet** (cuerdas, objetos, partículas).
- **Agua** con resortes de **Hooke** (superficie dinámica).
- Mapas en binario propio **`.ewmap`**, cargados por chunks con streaming LRU.

> Estado: `.ewmap` (`ewmapc` + `echo_map`), escena (`echo_game`: MapLoader → colisión/entidades/cámara), física Verlet + agua Hooke (`echo_physics`) y `echo_engine` jugable sobre el Deferred 320×180. Mapa por defecto: **`world`** (`EL REINO DEL POZO`, mundo unificado `8×3` chunks = `2560×552 px`, 24 salas conectadas entre El Pozo, Las Catacumbas y La Guarida del Wyrm, generado por `tools/gen_world.py`); `slice`, `level2` y `level3` siguen disponibles con `--map`.

## Estructura
```
engine/core/         echo_engine: ventana GLFW, input, audio y loop de frame (< 450 líneas)
engine/game/         echo_game (sin GL): Scene, Sim, LevelFlow, BossVictorySeq, WindField, Guide, Minimap (engine/game/minimap.hpp), Hud y FrameData
engine/physics/      echo_physics: Verlet (puntos, sticks, damping, colisión círculo↔tiles) + WaterBody (Hooke)
engine/render/       echo_render: loader GL 3.3 mínimo + DeferredRenderer
assets/shaders/      GLSL 330: fullscreen.vert, gbuffer_map.frag, light.frag
assets/atlas/         tiles.png y sprites.png default (generados por tools/gen_atlas.py y gen_sprites.py)
tools/gen_atlas.py    script reproducible del atlas de tiles PNG
tools/gen_sprites.py  script reproducible del atlas de sprites PNG (Sr. del Farol, La Zarigüeya, El Castor)
tools/gen_world.py    script reproducible del mundo unificado 8×3 (assets/maps/src/world.yaml)
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
ctest --test-dir build --output-on-failure   # compila los mapas .ewmap y corre toda la suite headless
```

## Correr el prototipo

**QA:** cómo instalar los paquetes, qué probar sala por sala y cómo reportar bugs: [docs/qa-checklist.md](docs/qa-checklist.md) (versión actual **v0.1.0**, `echo_engine --version`).

```sh
./build/echo_engine                            # ventana 1280×720 (320×180 ×4), mapa world (EL REINO DEL POZO, 8×3)
./build/echo_engine --scale 3                  # 960×540
./build/echo_engine --frames 120               # sale tras N frames (smoke test / headless con xvfb-run)
./build/echo_engine --map build/maps/slice.ewmap   # capítulo 1 suelto (también level2.ewmap, level3.ewmap)
./build/echo_engine --map otro.ewmap --dump frame.ppm  # otro mapa; vuelca el último frame 320×180
./build/echo_engine --map build/maps/hello.ewmap       # mapa de pruebas (2 chunks, 2 cuerdas)
./build/echo_engine --map build/maps/room.ewmap        # ejemplo mínimo de la guía (1 chunk, salto → meta)
./build/echo_engine --map build/maps/mover.ewmap       # prueba de plataforma móvil (no es sala de slice)
./build/echo_engine --demo        # input automático (camina, salta y activa el switch)
./build/echo_engine --start 330 150  # aparece en (x, y) px; p. ej. chunk 1
./build/echo_engine --hero zarigueya # inicia directo con zarigueya|farol|castor
```
Controles: **←/→** o **A/D** caminar, **Espacio/Z** (y **↑/W** fuera de la cuerda, con detección de flanco independiente para salto y arriba) saltar (también nadar hacia arriba en el agua, y **doble salto** aéreo una vez desbloqueada la reliquia, `kDoubleJumpSpeed = 245` px/s), **E/X** (y **↓/S** fuera de la cuerda) activar el switch o estela cercano, **E/X/K/Shift der.** embestida (**Dash**) una vez desbloqueada su reliquia (si hay un switch/diálogo cerca, E/X prioriza interactuar), **C/Shift izq.** agarrar/soltar la cuerda, **R** corto vuelve al último checkpoint (o al spawn), **R** largo (~0.8 s) reinicia todo al inicio, **ESC** o **Start** abre el menú de pausa (seguir, continuar la partida guardada, reiniciar desde checkpoint, volumen/mute, controles, **BRISA SI/NO**, **HEROE**, salir). Al arrancar sin partida guardada se abre el selector **`ELIGE TU HEROE`** para elegir entre **Sr. del Farol**, **La Zarigüeya** y **El Castor** (y podés cambiarlo cuando quieras desde Pausa → **HEROE** o con `--hero farol|zarigueya|castor`). Gamepad (Xbox/SDL): stick o d-pad caminar, **A**/RB saltar, **X**/**B** acción / Dash, **LB**/**Y** agarrar cuerda, stick Y / d-pad trepar, **Start** abre el menú (el pad no tiene R: usá REINICIO del menú). `--nogamepad` o `EW_NOGAMEPAD=1` lo apaga. **Gravedad invertida:** los orbes violetas (`gravity_flip`) invierten la gravedad del player al tocarlos (pies arriba); ver la guía de diseño. `./build/echo_engine --map build/maps/gravity.ewmap` la prueba. **Habilidades (Doble Salto, Dash y Trepar / Salto en Pared):** la reliquia cian (`ability_double_jump`) desbloquea 1 salto extra en el aire, la reliquia ígnea (`ability_dash`) desbloquea la **embestida horizontal** sin caída que destruye muros agrietados (`break_wall`) —ambas se recargan al apoyar, agarrar una cuerda o entrar al agua— y la reliquia esmeralda (`ability_wall_climb`, `kAbilityWallClimb`) habilita **deslizamiento en pared** (`kWallSlideSpeed = 36 px/s`) y **salto en pared** (`kWallJumpVx = 138 px/s`, `kWallJumpVy = 232 px/s`, coyote `6` frames) **preservando el Doble Salto**. Mapas de prueba: `--map build/maps/ability.ewmap` (`ability_test`), `--map build/maps/dash.ewmap` (`dash_test`) y `--map build/maps/wallclimb.ewmap` (`wall_climb_secret`). **Boss:** se le pega cayéndole encima (3 pisotones; 4 en el Wyrm); tocarlo de otra forma mata; al vencerlo se abre la puerta de la meta y corre la secuencia CRT + diálogo (`BossVictorySeq`). Arena de prueba: `--map build/maps/boss.ewmap`. SFX procedural (beeps) si hay dispositivo; `--nosound` o `EW_NOSOUND=1` lo apaga (CI headless). El build compila `assets/maps/src/{world,slice,level2,level3,hello,room,mover,gravity,boss,ability,dash,wallclimb}.yaml` → `build/maps/*.ewmap` (con CRC) y usa **`world`** por defecto (`EL REINO DEL POZO`, `8×3` chunks = `2560×552 px`, 24 salas conectadas entre El Pozo, Las Catacumbas y La Guarida del Wyrm, generado por `tools/gen_world.py` y auditado con `--check` / `world_gen_check`); los capítulos sueltos (`slice` → `level2` → `level3`) siguen disponibles con `--map build/maps/slice.ewmap`, etc. Cómo armar una sala nueva: [docs/level-design.md](docs/level-design.md).

### Controles configurables
Menú de pausa → **CONTROLES**: una fila por acción (izquierda, derecha, arriba, abajo, salto, acción, cuerda, reinicio) con sus teclas y, para salto / acción / cuerda, sus botones del pad. **Enter**/**A** en una fila espera la tecla o el botón nuevo (**Esc** cancela); la acción queda con esa sola tecla y, si otra acción la usaba, recibe la anterior (intercambio), así ninguna queda sin tecla. **POR DEFECTO** vuelve a los controles de arriba. Esc/Enter, Start/Back/Guide y el d-pad no se asignan (son del menú y del movimiento). El menú siempre se navega con flechas/WASD, Enter/Espacio/Z y d-pad/A/B. Los controles se guardan en el mismo `save.ewsv` (formato v4; los saves v1, v2 y v3 se siguen leyendo con retrocompatibilidad) apenas se cambian. Lógica sin GLFW en `engine/game/bindings.hpp` (test `bindings_test`).

### Partida guardada
Un slot, autoguardado: se escribe al tomar un checkpoint, agarrar un huevo, desbloquear una habilidad, elegir personaje, vencer al boss, pasar al siguiente nivel, cambiar el volumen o los controles. Guarda mapa, personaje elegido (`HeroKind`), checkpoint (+ gravedad), estado de cada entidad (puertas, switches, inundación, huevos, reliquias, boss), habilidades desbloqueadas, volumen/mute y controles. Si al arrancar hay un save del mapa actual o de otro nivel disponible de la campaña, el menú abre en **CONTINUAR** (carga el mapa del save y aparecés en el checkpoint guardado); **SEGUIR** arranca de cero y el save se reemplaza recién cuando progresás. **R** largo vuelve a un spawn limpio.

- Archivo `save.ewsv` en el directorio de datos del usuario: `$XDG_DATA_HOME/echo-well` o `~/.local/share/echo-well` (Linux), `%APPDATA%\EchoWell` (Windows), `~/Library/Application Support/EchoWell` (macOS); `EW_SAVE_DIR` lo cambia. Si no se puede escribir, usa la carpeta del binario; si tampoco, no guarda.
- Formato binario versionado (`EWSV`, v4; lee v1, v2, v3 y v4) con bounds en cada lectura, tamaño exacto y CRC-32; un save corrupto, truncado o de otro mapa se ignora. Escritura atómica (tmp + rename). Layout en `engine/game/save.hpp`.
- `--nosave` / `EW_NOSAVE=1` lo apaga; `--demo`, `--start` y `--frames` (sin `EW_SAVE_DIR`) tampoco guardan.
- Tests: `save_test` (round-trip, controles v2, habilidades v3, héroe v4 y compatibilidad v1/v2/v3, apply sobre `slice`, rechazo de bytes cambiados/truncados y campos fuera de rango, archivo y directorio) y `fuzz_save` (CI: 60 s con libFuzzer + smoke en ctest).

### Vertical slice (`slice`)
Camino **start → goal** en 8×1 chunks (2560×184 px) que junta las mecánicas existentes:

1. **Chunk 0:** spawn, **switch → puerta**, pileta de **agua** (hay que nadar). Seam en x=320.
2. **Chunk 1:** **cuerda** (sección ROPE) para subir una pared que no se salta, cornisa con **atlas** de tiles. Seam en x=640. La cámara usa dead-zone + slide en los seams.
3. **Chunk 2:** **checkpoint** en la cornisa, **hueco con pinchos** (META 4) y un **`flood_switch`**.
4. **Chunk 3:** pileta cuyo nivel **sube** al activar el switch (de y=136 a y=72, `Sim::kFloodRestY`) para nadar hasta la cornisa de salida. Otro checkpoint en la salida.
5. **Chunk 4 — sala del huevo (opcional):** con el agua **baja** (sin activar el `flood_switch`), un `switch_well` dentro de la pileta abre dos **puertas** en su pared derecha; se nada al pasillo bajo la cornisa de salida, se usa una **segunda cuerda** para trepar a una repisa y se agarra el **huevo** (`type: collectible`). A la repisa también se llega por arriba, desde la cornisa de salida, saltando el hueco de la cuerda; la repisa sigue por un hueco en la pared derecha hacia la sala 4.
6. **Chunk 5 — sala 4, plataformas móviles:** checkpoint `ck_mover` en la repisa de entrada; el piso entero es un **foso de pinchos**. Una plataforma horizontal (`plat_h`) lleva hasta un pilar; desde ahí un ascensor vertical (`lift_v`) baja al piso seguro, que sigue por una puerta abierta hacia la sala 5.
7. **Chunk 6 — sala 5, gravedad invertida:** checkpoint `ck_grav` al entrar; un orbe en el piso (`flip_up`) invierte la gravedad justo antes de un foso de pinchos que no se salta. Se camina por el **techo**, se cruza un hueco del techo (con pinchos arriba) **colgado de la cara de abajo** de una plataforma móvil (`plat_inv`), y un orbe del techo (`flip_down`) devuelve la gravedad sobre el piso seguro, que sigue por una puerta abierta a la sala final.
8. **Chunk 7 — sala 6 final, boss:** checkpoint `ck_boss` al entrar; el **boss** patrulla el medio de la arena (caminar → aviso → salto, más rápido con cada golpe). Se le pega **cayéndole encima** (3 pisotones); tocarlo de otra forma mata y morir lo cura. Al vencerlo se abre la puerta de la derecha (`door_boss`), corre la secuencia CRT + pausa lenta + diálogo (`BossVictorySeq`, `kBossBurstFrames = 180` / ~3,0 s + `kSfxBossCrt`), y detrás está la **meta** (goal + META 3, x=2520), que cierra la demo con el fade. Con el boss vivo, la meta no cuenta.

**Collectibles, habilidades y mini-mapa:** HUD arriba a la izquierda con corazones de vida (`hp()`/`max_hp()`), huevo + **n/total** (dorado al completar, destello + beep al agarrar) e íconos de habilidades desbloqueadas; arriba a la derecha un **mini-mapa exploratorio** (`engine/game/minimap.hpp` — `compute_minimap` / `MinimapState`, volcado a `FrameData` vía `populate_hud_and_overlays` en `engine/game/hud.hpp`, test `minimap_hud`) oculta las salas no visitadas (`!vis`), resalta la sala actual en mapas multi-pantalla (`world`, `slice`, `level2`, `level3`), marca sobre las salas exploradas las puertas cerradas o muros rompibles intactos (`door`, `one_way_door`, `break_wall` con `!active`) en **carmesí** y los huevos pendientes (`collectible` con `!active`) en **dorado**, soporta hasta **64 chunks** mediante bitmasks `lo/hi` (`visited_mask`/`visited_mask_hi`, `blocked_doors_lo`/`blocked_doors_hi`, `missing_eggs_lo`/`missing_eggs_hi`) y muestra el **porcentaje explorado (`%`, `explored_pct`)**. Lo agarrado **sobrevive** a morir y a **R** corto (checkpoint); **R** largo (spawn limpio) vuelve a 0. La meta **no** requiere el huevo (se puede terminar 0/1).
- **Recompensas por huevos opcionales y final alternativo:** además de contar para el `100%`, reunir huevos desbloquea mejoras progresivas notificadas en `HudEvents` (`new_extra_hp`, `new_dash_upgrade`): con **`Sim::kEggsForExtraHp = 2`** huevos ganás **`+1` vida máxima** (`2` corazones en HUD, `take_hit` permite absorber 1 golpe con `Sim::kHitInvulnFrames = 60` frames de invulnerabilidad y se cura al tocar un checkpoint o hacer `respawn`), y con **`Sim::kEggsForDashUpgrade = 4`** huevos el Dash evoluciona a **Dash II** (`dash_upgraded()`: `Sim::kDashUpgradedCharges = 2` cargas aéreas, `Sim::kDashUpgradedFrames = 14` frames de duración y `Sim::kDashUpgradedCooldown = 10` frames de cooldown). Al completar la campaña con todos los huevos (`all_eggs_collected()`, p. ej. `6/6` en `world`), `EndStats::true_ending` activa la pantalla de **final alternativo** **`FINAL VERDADERO`** / **`REINO RENACIDO: 100% HUEVOS`** (en lugar de `VICTORIA: REINO DESPIERTO`) mostrando `DASH II` y `+1HP`; cubierto por el test headless `collectible_rewards` (`tests/rewards_test.cpp`).
Al tocar la meta, se abre la **pantalla de final pixel-art** (`EndScreen` en `engine/game/end_screen.hpp`, `end_screen_test`) con el resumen del nivel (huevos, muertes, habilidades, **tiempo y rango S/A/B**, título **`VICTORIA: REINO DESPIERTO`** al completar la campaña estándar o **`FINAL VERDADERO`** / **`REINO RENACIDO: 100% HUEVOS`** con `true_ending` al reunir el `100%` de los huevos) y las opciones **SIGUIENTE NIVEL** (cuando el siguiente mapa de la campaña en `engine/game/campaign.hpp` o `--next-map FILE` existe en disco, transfiriendo las habilidades desbloqueadas vía `LevelFlow` en `engine/game/level_flow.hpp`, test `level_flow`), **REINICIAR NIVEL**, **MENU DE PAUSA** y **SALIR**. **R** corto sigue volviendo al último checkpoint y **R** largo (~0.8 s) al spawn limpio. `slice_test` recorre todo el camino (incluye inundación, la sala 4 con las plataformas, la sala 5 con gravedad invertida y la pelea con el boss de la sala 6, con muertes en los fosos y contra el boss) y resuelve la sala del huevo (switch_well → puertas → cuerda 2 → huevo → salas 4, 5 y 6 → meta 1/1) sin GL.

`slice_test` (ctest, sin GL) carga el mapa, verifica el loader (entidades, capas COL/VIS/FG/META, cuerda, agua) y recorre el camino con input scriptado hasta la meta. `hello` sigue como mapa de los demás tests.

### Nivel 2 — Las Catacumbas (`level2`)
Mapa metroidvania no lineal de **4×2 chunks** (160×46 tiles = 1280×368 px, `assets/maps/src/level2.yaml`) que se carga automáticamente al elegir **SIGUIENTE NIVEL** tras completar `slice` (o directo con `./build/echo_engine --map build/maps/level2.ewmap`):

1. **Planta alta (`cy = 0`):**
   - **Chunk `(0,0)` — Entrada:** `spawn` y una repisa alta (`y = 96`, 64 px sobre el piso) con el primer huevo (`egg_catacomb_1`), visible desde el arranque pero inalcanzable con un salto simple (~44 px).
   - **Chunk `(1,0)` — Bóveda Central (Hub):** checkpoint `ck_hub`, pozo vertical abierto hacia `(1,1)` y la **Galería Alta** a la derecha (64 px de altura, requiere **Doble Salto**) cerrada además por `door_hub_gallery`.
   - **Chunk `(2,0)` — Galería Invertida:** checkpoint `ck_grav2`, orbes `flip_up2` / `flip_down2` para cruzar por el techo sobre un foso de pinchos.
   - **Chunk `(3,0)` — Cámara del Guardián:** muro rompible **`wall_guardian`** (`break_wall`, exige **Dash**), checkpoint `ck_boss2`, `boss_catacomb` (3 pisotones) → `door_boss2` → `goal`.
2. **Planta baja (`cy = 1`):**
   - **Chunk `(1,1)` — El Foso:** pileta de agua que amortigua la caída vertical desde el Hub, checkpoint `ck_pit` y dos escalones de retorno (Escalón 1 en `y = 280` y Escalón 2 en `y = 224`) que exigen **Doble Salto** para volver a la planta alta.
   - **Chunk `(0,1)` — Cisterna del Altar:** checkpoint `ck_altar`, foso corto con cuerda y la reliquia **`rel_double_jump`** (`ability_double_jump`).
   - **Chunk `(2,1)` — Galería Subterránea Este:** accesible subiendo al Escalón 1 con Doble Salto; checkpoint `ck_east_low` y plataforma móvil `plat_crypt` sobre un foso de pinchos.
   - **Chunk `(3,1)` — Santuario Profundo:** `sw_sanctum` (abre `door_hub_gallery` en el Hub `(1,0)`) y altar alto con el segundo huevo (`egg_catacomb_2`) y la reliquia ígnea **`rel_dash`** (`ability_dash`).

`level2_test` (ctest, sin GL) verifica que las repisas altas de `(0,0)` y `(1,0)` son inalcanzables sin la habilidad, desciende por el pozo vertical a `(1,1)`, desbloquea el Doble Salto en `(0,1)`, explora el ala subterránea `(2,1) → (3,1)` para abrir la puerta del Hub, tomar `egg_catacomb_2` y desbloquear **`rel_dash`**, hace **backtracking vertical y horizontal** hasta `(0,0)` para recoger `egg_catacomb_1` (2/2 huevos), rompe **`wall_guardian`** embistiendo con Dash y completa `(2,0) → (3,0)` venciendo al boss hasta la meta (`DOBLE SALTO + DASH`), desbloqueando el paso a **`level3`**.

### Nivel 3 — La Guarida del Wyrm (`level3`)
Mapa final de campaña de **3×2 chunks** (120×46 tiles = 960×368 px, `assets/maps/src/level3.yaml`) al que se accede tras completar `level2` (o directo con `./build/echo_engine --map build/maps/level3.ewmap`):
1. **Umbral y Puentes `(0,0) → (1,0)`:** rompe `wall_entry` con Dash, recoge `egg_wyrm_1` (`1/2`) sobre la repisa alta y cruza el abismo sobre `plat_wyrm` / cuerda.
2. **Pozo Invertido y Santuario `(1,1) → (0,1)`:** cae al agua del foso, cruza el techo invertido con `flip_up3` / `flip_down3`, rompe `wall_secret3` embistiendo, derrota a `imp_santuario` (pisotón o Dash) para activar `sw_seal3` (abre `door_wyrm_seal`) y recoge `egg_wyrm_2` (`2/2`).
3. **Arena del Wyrm `(2,0)`:** enfrenta al jefe dragón multipatrón **`wyrm`** (`boss_dragon`, 4 pisotones sobre su corona dorada, esquivando su embestida rasa y atravesando sus bolas de fuego con Dash) para abrir `door_wyrm_exit` y cruzar la meta final (`level3_test`, `stage9_test`).

### Escena (MapLoader → render)
- `ew::game::Scene` carga el `.ewmap`, lee tiles vía `ChunkCache` y expone `solid_px()` (capa `COL`; fuera del mapa = sólido), entidades y una cámara que sigue al player clampada al mapa. El **chunk activo** sale de la posición del player.
- Pipeline: geometry pass → los tiles `COL`/`VIS`/`FG` del vecindario 3×3 se suben como textura `RGB8UI` 120×69 (solo cuando cambia el chunk) y el shader dibuja COL/VIS con bisel, luego actores, y **FG por encima** (primer plano); marcadores (player, `switch`, `door`, `player_spawn`) van como uniforms → G-buffer 320×180 (albedo + normal + depth24) → light pass (ambiente + luz puntual que sigue al player) → blit NEAREST con escalado entero y letterbox.

### Chunks vecinos y cámara por pantallas
- Cada chunk (40×23 tiles = 320×184 px) es **una pantalla**, como en *Animal Well*. El renderer recibe el **vecindario 3×3** del chunk activo (textura `RGB8UI` 120×69 (R=COL, G=VIS, B=FG), re-subida solo al cambiar de chunk), así que se dibujan el activo y sus 8 vecinos (incluye los 4 ortogonales); los que caen fuera del mapa quedan vacíos.
- **Elección de cámara: slide corto, no snap.** Al cruzar un seam el chunk activo cambia en el acto (colisión/streaming) y la cámara **desliza** hacia la nueva pantalla con un ease exponencial (`kCamSlide = 12/s`, ~0.3 s; termina exacto en el origen del chunk). Dentro de una pantalla la cámara usa una **dead-zone** centrada (`kDeadHalfW = 48`, `kDeadHalfH = 24` px): mientras el player esté adentro no se mueve; al salir, la arrastra lo justo y se clampa a la pantalla del chunk. Al entrar a un chunk nuevo (o en snap) se re-centra clampado y el slide del seam sigue igual. Con chunks de 320×184 el efecto real es solo vertical (holgura 4 px); en chunks más grandes que la vista funciona en ambos ejes. Lógica pura en `Scene::dead_zone_step` (testeada en `scene_test`). Se redondea a px enteros al render (pixel-perfect). `follow(x, y)` sin `dt` hace snap (carga / `--start`).
- `hello` ahora es 2×1 chunks: el chunk 1 tiene plataformas escalonadas; se llega abriendo la puerta con el switch. Probar: activar el switch, caminar a la derecha y cruzar x=320, o `--start 330 150`.

### Mundo unificado (`world`)
Mapa por defecto (`EL REINO DEL POZO`, `assets/maps/src/world.yaml` generado por [`tools/gen_world.py`](tools/gen_world.py) — auditable con `--check` / test `world_gen_check`): grilla de **8×3 chunks** (`320×69` tiles = **2560×552 px**, 24 salas) que conecta **El Pozo** (`cy=0`, `cx=0..7`), **Las Catacumbas** (`cx=4..7, cy=1..2`) y **La Guarida del Wyrm** (`cx=0..3, cy=1..2`) en un solo mundo Metroidvania continuo (6 huevos, 3 bosses, 6 piletas, 4 cuerdas, 7 plataformas/ascensores) con atajos unidireccionales (`owd_wyrm_cat_l`, `owd_wyrm_pozo`), ascensores de retorno (`elev_cat_pozo` desbloqueado por `sw_elev_cat`, y `elev_wyrm_pozo`) y la **chimenea secreta en el techo de Zona 1 `(0,0)`** (`egg_pozo_secreto` + estela `lore_techo`, **6/6 huevos** en `world`), alcanzable únicamente haciendo backtracking tras recoger `rel_wall_climb` (`ability_wall_climb`) en Zona 3 `(0,2)`. Reunir los hitos de huevos otorga `+1` vida máxima a los `2` (`Sim::kEggsForExtraHp`) y **Dash II** a los `4` (`Sim::kEggsForDashUpgrade`), y completar `world` con `6/6` (`all_eggs_collected()`) desbloquea el final alternativo **`FINAL VERDADERO`** (`REINO RENACIDO: 100% HUEVOS`, verificado por `collectible_rewards` en `tests/rewards_test.cpp`). Los capítulos individuales (`slice`, `level2`, `level3`) siguen disponibles con `--map build/maps/<mapa>.ewmap`.

### Altares, palancas, flechas guía y puertas
- **Palancas / abre-puertas (`switch`, `flood_switch`) y puertas (`door`):** los `links:` del mapa (sección `LINK`) conectan `switch` → `door` (o múltiples puertas). Con el player a ≤ `Scene::kSwitchReach` (14 px), **E/X/↓** alterna la palanca y abre/cierra todas sus puertas enlazadas.
- Puerta = 1×3 tiles (8×24 px) desde su `(x, y)`. **Cerrada** es sólida (`Scene::solid_tile` la suma a `COL`); **abierta** no colisiona. La apertura se anima en 0.25 s (`anim` 0→1, `Scene::tick`); colisiona mientras `anim < 0.5`. Al cerrarse, la colisión vuelve al instante y el visual se desliza hacia abajo.
- **Puertas de atajo de un solo sentido (`one_way_door`):** puertas de **1×3 tiles** (`8×24 px`) que bloquean el paso desde el lado cerrado y se abren de forma permanente al empujar o interactuar (**E/X/↓**) desde el lado desbloqueable (`Scene::try_open_one_way`): desde la **derecha** por defecto, o desde la **izquierda** si el nombre termina en `_l` o contiene `_from_left` (`one_way_unlocks_from_left`). Interactuar desde el lado bloqueado emite un clink metálico (`kSfxHit`) sin abrirla; una vez abierta persiste tras morir, `R` corto y en `save.ewsv`.
- **Ascensores (`elevator`):** usan registros `MOVER` en el `.ewmap` (`MoverRecord`) entre dos waypoints. Si tienen un `switch` enlazado en `links:` arrancan bloqueados hasta activarlo (sin link arrancan desbloqueados). Una vez desbloqueados, se activan al pisarlos o invocándolos con **E/X/↓** (`Sim::call_elevator`), persisten desbloqueados tras `respawn()` y en `save.ewsv` (tests `world_mechanisms`, `world_metroidvania`).
- **Altares de checkpoint (`checkpoint`, `shape 2`) y palancas elevadas (`shape 3`):** el altar se activa a ≤ `Sim::kCheckpointReach` (20 px) pasando de cristal cian `(0.25, 0.55, 0.70)` a llama verde `(0.30, 1.0, 0.55)` y guarda la gravedad actual; las palancas elevadas brillan en **ámbar** con una flecha verde (`shape 4`) encima mientras están inactivas y pasan a **verde** al accionarse. Ambos despejan automáticamente el pasto cercano (`near_mech` en `gbuffer_map.frag`).
- **Flechas guía (`engine/game/guide.hpp`):** leen la sección opcional `GUIDE = 11` (objetivos `switch` ámbar o `ability` cian y pozos `shaft`) con fallback automático por `links:` en mapas viejos; se ocultan a ≤ 24 px o al terminar el nivel (test `guide_arrows`).
- En `hello`, la puerta (x=280) bloquea el paso hacia el borde derecho hasta activar `switch_a` (x=96). `door_test`, `mechanisms_crt` (`tests/mechanisms_crt_test.cpp`) y `world_mechanisms` (`tests/world_mechanisms_test.cpp`) verifican alcance, múltiples puertas, `flood_switch`, `one_way_door`, `elevator`, altares, `imp` y marcadores en `FrameData`.

### Física Verlet
- `ew::physics::World`: puntos con `pos/prev` (Verlet posicional), gravedad 600 px/s², damping, constraints de distancia (`Stick`, con rigidez), pares círculo↔círculo y colisión círculo↔tiles sólidos (empuje por punto más cercano + fricción tangencial, sin rebote). 6 iteraciones por paso.
- `ew::game::Sim` une `Scene` + `World` a paso fijo 60 Hz: el player es un círculo r=4 que cae, camina (90 px/s) y salta (apoyado o dentro del margen de coyote time) contra la capa `COL`; una cuerda de 16 segmentos cuelga del techo y el player la empuja.
- **Game feel (Etapa 12):** **coyote time** (`Sim::kCoyoteFrames = 6` frames al dejar un borde), **jump buffer** (`Sim::kJumpBufferFrames = 6` frames al apretar salto antes de aterrizar), **squash & stretch** (`squash_x`/`squash_y` en `Sim` → `FrameData::player_squash_x`/`player_squash_y`), partículas de polvo (`Sim::DustParticle`, pool hasta `kMaxDust = 24`) al aterrizar, saltar, hacer doble salto o saltar desde pared al trepar (`wall-jump`), y estela de partículas + afterimage residual durante y al salir de la embestida (`Dash`, `Sim::kDashFlashFrames = 14`); cubiertos por el test headless `game_feel` (`tests/game_feel_test.cpp`).
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
- **`META`**: marcadores de zona. El runtime usa **id 3** como zona de meta (`Sim::kMetaGoal`), **id 4** como peligro / pinchos (`Sim::kMetaHazard`, renderizados como púas pixel-art metálicas con punta carmesí) y **id 2** documenta agua (el runtime usa la sección `WATER`). Ver [docs/ewmap.md](docs/ewmap.md).

### Fondos por sala (backdrops)
Cada pantalla tiene un fondo procedural propio, estilo *Animal Well* y fantasía oscura dracónica, dibujado en el geometry pass (sin imágenes ni dependencias): pozo con haz de luz y gotas, raíces, roca partida con brasas, cisterna con caños, nichos con huevos, engranajes que giran, arquitectura espejada para la gravedad invertida, arena del boss, y los temas de `level2`/`level3` (`cripta` gótica, `abismo` de obsidiana en `c10`, `guarida` fósil del wyrm y `santuario` rúnico). Se eligen en el YAML (`backdrops: - {cx, cy, theme}`) y viajan en la sección opcional `BACKDROP`; los mapas sin la sección usan `cueva`. Detalle y lista de temas en [docs/level-design.md](docs/level-design.md#fondos-backdrops). `backdrop_test` (ctest) cubre la tabla de temas, el vecindario 3×3 y la retrocompatibilidad; `tools/room_shots.py` captura y valida las **46 salas** de `slice` (8), `level2` (8), `level3` (6) y `world` (24) contra su versión en `vacio` (CI: artifact `fondos-salas`, con `sala*.ppm`, `level2_*.ppm`, `level3_*.ppm` y `world_*.ppm`, sin `*_vacio.ppm`).

### Vegetación, brisa y secuencia CRT post-boss
- **Pasto pixel-art con brisa viajera (`engine/game/wind.hpp`):** `WindField` genera ráfagas globales y hasta 4 impulsos locales (estela al caminar, aterrizajes, Dash, `imp` y bosses) que inclinan pasto, hongos, flores, telarañas y estandartes y empujan las cuerdas Verlet; se despeja automáticamente alrededor de altares y palancas (`near_mech`). Se activa/desactiva desde pausa con **`BRISA SI/NO`** (tests `wind_breeze`, `pause_menu`).
- **Efectos CRT y secuencia ordenada (`engine/game/boss_victory.hpp`):** al vencer a cualquiera de los 3 bosses corre `BossVictorySeq`: `DefeatAnim` (~3,0 s / `kBossBurstFrames = 180` con scanlines CRT en `FrameData::boss_count`, destello `boss_flash`, 14 chispas radiales, sacudida de cámara y `SfxId::BossCrt`) → `AftermathPause` (~0,75 s con el jugador libre en cámara lenta) → diálogo único de progresión (`boss_victory_seq`, `mechanisms_crt`).

## Formato `.ewmap` (resumen)
- Magic `EWMP`, little-endian, secciones alineadas a 8 bytes, versión 1.0.
- Tile 8×8 px, chunk 40×23 tiles; capas `COL`, `VIS`, `FG`, `META`.
- **11 secciones:** `TILE=1`, `ENT=2`, `LINK=3`, `WATER=4`, `STRTAB=5`, `CRC=6`, `ROPE=7`, `MOVER=8`, `BACKDROP=9`, `DIALOG=10`, `GUIDE=11` (`ROPE`/`MOVER`/`BACKDROP`/`DIALOG`/`GUIDE` opcionales; los mapas anteriores siguen cargando sin cambios).
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

## Atlas de sprites (protagonista y objetos)

`echo_engine` carga **`assets/atlas/sprites.png`** (256×128 RGBA8, 16×8 celdas de 16×16 px; 88 sprites generados por [`tools/gen_sprites.py`](tools/gen_sprites.py) y auditable con `--check`; también acepta PNGs legacy 128×128 rellenando cols 0..7; override por `--sprites FILE.png` o env `EW_SPRITES`).
- **3 personajes elegibles (`engine/game/hero.hpp`, filas `0..5`, 12×12 px en celda 16×16):** **Sr. del Farol** (`HeroKind::Lantern`, cols `0..3`), **La Zarigüeya** (`HeroKind::Opossum`, cols `4..7`) y **El Castor** (`HeroKind::Beaver`, cols `8..11`). Todos mantienen la colisión física de radio `4 px`, animaciones deterministas en `Sim::player_anim()` (`Idle`, `Walk`, `Jump`, `DoubleJump` con alas dracónicas cian, `Climb`, `Swim`), giro horizontal (`player_facing()`) e inversión vertical en gravedad invertida. El canal A guarda el relieve/altura para normales 3D en el G-buffer (`A > 240` aporta auto-emisión suave en ojo, farol y núcleo).
- **Objetos animados (fila 6):** huevo de dragón con escamas y fisuras incandescentes (`collectible`, cols 0..3) y reliquia alada de Doble Salto (`ability_double_jump`, cols 4..7).
- **Atlas de sprites (`assets/atlas/sprites.png`):** los 3 héroes en filas `0..5` (12 columnas × 6 estados = 72 sprites), huevos de dragón escamados + reliquia alada + ígnea + NPC/Estela en filas `6..7`.
- **Diálogos e historia de los huevos de dragón (`engine/game/dialogue.hpp`, sección `.ewmap` `DIALOG = 10`):** al conversar con `npc`/`lore` (`E/X/↓`) o al recoger huevos y reliquias (`trigger: pickup`), se abre una caja de diálogo paginada (`|`, `/`) en `light.frag` revelando el origen de los huevos en `slice` y `level2` (testeado sin GL en `dialogue_test`).

El job `engine` de CI corre el prototipo headless (`xvfb-run`, `--frames 90 --demo --dump frame.ppm --nosound`) y sube el PPM 320×180 como artifact `engine-frame` (se puede abrir con cualquier visor; `EW_ATLAS_PPM` hace lo mismo con el atlas).

Los jobs `package-linux` y `package-windows` (push a `main`, tags y PRs) publican **artifacts de CI**
(`echo-well-linux-x86_64`, `echo-well-windows-x86_64`): `echo_engine`, `ewmapc`, shaders, atlas y mapas
compilados, listos para bajar desde la pestaña Actions. **No** son un release público ni una publicación
en itch.io. El binario resuelve assets junto a sí mismo (`assets/…`); ver `assets/DIST.txt`. Instalación
local: `cmake --install build --prefix dist/echo-well`.

Perf: el timestep es fijo (`Sim::kDt` = 1/60, sin reloj de pared). `bench_test` verifica determinismo y reporta us/paso (umbral holgado 2 ms, CI). `echo_engine --frames N` imprime ms/frame al salir. Colisión de puertas usa un cache de AABBs (sin `strcmp` por tile).

Menú de pausa: `PauseMenu` en `engine/game/menu.hpp` (sin GL, testeable: `pause_menu` / `menu_test.cpp`). Esc/Start abre y cierra; ↑/↓ navega entre `SEGUIR`, `CONTINUAR`, `REINICIO`, `VOLUMEN`, `CONTROLES`, **`BRISA SI/NO`**, **`HEROE`** y `SALIR`; Enter/Espacio/A confirma, ←/→ ajusta el volumen (pasos de 0.1) o alterna `BRISA SI/NO` sin cerrar el menú, confirmar en Volumen mutea y confirmar en `HEROE` abre el selector visual de personaje (`Sr. del Farol`, `La Zarigüeya`, `El Castor`). El mixer aplica ganancia/mute (thread-safe). La simulación se congela mientras el menú o el selector está abierto; `--demo` no lo abre.

SFX: `engine/audio` genera SFX procedurales (salto, muerte, checkpoint, meta, switch, splash, pickup, habilidad, embestida/dash, rotura de muro, impacto en enemigos y descarga CRT `SfxId::BossCrt`). Tests headless destacados: `sfx`, `boss_victory_seq`, `wind_breeze`, `guide_arrows`, `level_flow`, `mechanisms_crt`, `metroidvania_campaign` (campaña completa `slice → level2 → level3` y anti-softlock), `world_gen_check` (`tools/gen_world.py --check`), `world_mechanisms` (`tests/world_mechanisms_test.cpp`), `world_metroidvania` (`tests/world_test.cpp`) y `collectible_rewards` (`tests/rewards_test.cpp`). `echo_engine` reproduce los SFX con miniaudio (se descarga al compilar, no está vendorizado) y si no hay hardware o se pasa `--nosound`/`EW_NOSOUND` sigue en silencio. Se desactiva con `-DEW_AUDIO=OFF`.

Seguridad (fuzzing, sanitizers, branch protection): ver [docs/security.md](docs/security.md).
