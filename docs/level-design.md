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
backdrops:                   # opcional: fondo procedural por chunk (ver "Fondos")
  - {cx: 0, cy: 0, theme: pozo}
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
| 4 | Peligro / pinchos (`Sim::kMetaHazard`): muerte y reaparición en el checkpoint; el renderer los dibuja como púas metálicas pixel-art con punta carmesí |

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
| `switch` | Palanca abre-puertas elevada (`shape 3`, ámbar con flecha `shape 4` encima mientras `!active` → verde al activarse; despeja el pasto cercano con `near_mech`). Se activa a ≤ `Scene::kSwitchReach` (14 px) con E / X / ↓ / botón. |
| `door` | Bloquea un hueco de 1×3 tiles (`kDoorWTiles` × `kDoorHTiles`), esquina sup. izq. en (x, y). Sólida mientras `anim < kDoorSolidUntil` (0.5). |
| `one_way_door` | Puerta de atajo de 1×3 tiles (`kDoorWTiles` × `kDoorHTiles`), esquina sup. izq. en `(x, y)`. Evaluada por `Scene::try_open_one_way(x, y, push_touch)`: bloquea el paso desde su lado cerrado (interactuar desde ese lado devuelve `-2` y emite `kSfxHit`) y se abre permanentemente (`active = true`, `kSfxSwitch`) al empujar o pulsar E / X / ↓ desde la **derecha** por defecto, o desde la **izquierda** si el nombre termina en `_l` o contiene `_from_left` (`one_way_unlocks_from_left`). Persiste tras muerte / R corto y en `save.ewsv` (carga con `anim = 1`). |
| `checkpoint` | Altar de santuario (`shape 2`, cristal cian inactivo `(0.25, 0.55, 0.70)` → llama verde `(0.30, 1.0, 0.55)` al activarse; despeja el pasto cercano con `near_mech`). Al acercarse ≤ `Sim::kCheckpointReach` (20 px) queda como reaparición (y guarda la gravedad actual). R corto o morir vuelven acá. |
| `flood_switch` | Como `switch` (`shape 3`, también se puede linkear a puertas). Activo, sube las piletas con **`x ≥ 400`** hasta `Sim::kFloodRestY` (72 px) a `kFloodSpeed` (48 px/s). Al apagarse, bajan a su `y` inicial. |
| `goal` | Meta, alcance `Sim::kGoalReach` (16 px) o pisar `META` 3. No exige collectibles. |
| `collectible` | Huevo de dragón escamado, alcance `Sim::kCollectReach` (12 px) al centro del player. Sobrevive a morir y a R corto; R largo lo restablece. El HUD muestra n/total y `hp`, y puede disparar un eco de diálogo al recogerlo (`trigger: pickup`). Recompensas progresivas: con `2+` huevos (`Sim::kEggsForExtraHp = 2`) suma `+1` vida máxima (`max_hp() = 2` corazones, `take_hit` permite absorber 1 golpe con `Sim::kHitInvulnFrames = 60` frames de invulnerabilidad y se cura en checkpoints/`respawn`); con `4+` huevos (`Sim::kEggsForDashUpgrade = 4`) desbloquea **Dash II** (`dash_upgraded()`, `2` cargas aéreas `kDashUpgradedCharges`, `14` frames de duración `kDashUpgradedFrames` y `10` de cooldown `kDashUpgradedCooldown`). Reunir todos los huevos (`all_eggs_collected()`, `100%`) activa `EndStats::true_ending` (`FINAL VERDADERO` / `NIDADA A SALVO: TODOS LOS HUEVOS`). |
| `npc` / `lore` | Vigía/cronista (`npc`) o estela rúnica (`lore`) que muestran texto al pulsar E / X / ↓ a ≤ `Sim::kDialogReach` (16 px) si tienen registro en `dialogs:`. No bloquean el movimiento fuera de la caja de diálogo. |
| `ability_double_jump` | Reliquia cian (alcance `Sim::kAbilityReach`, 12 px) que desbloquea **Doble Salto** (`kAbilityDoubleJump`). Permite 1 salto extra en el aire (`Sim::kDoubleJumpSpeed`, 245 px/s, respetando gravedad invertida) y se recarga al apoyar en piso/mover, agarrar una cuerda, pisar un boss o entrar al agua. Sobrevive a morir y a R corto; se guarda en `save.ewsv` v4 (retrocompatible con v1–v3) y se limpia con R largo. Mapa de prueba: `assets/maps/src/ability.yaml` (`ability_test`). |
| `ability_dash` | Reliquia ígnea dorado-carmesí (alcance `Sim::kAbilityReach`, 12 px) que desbloquea **Embestida / Dash** (`kAbilityDash`). Con **E / X / K / Shift der.** (o **X / B** en pad) impulsa horizontalmente a `Sim::kDashSpeed` (240 px/s) durante `Sim::kDashFrames` (10 pasos) sin caída por gravedad (normal o invertida); 1 embestida aérea recargable al pisar suelo, nadar o colgarse. Si hay un switch o diálogo interactivo al alcance, E/X prioriza la interacción y no dispara Dash. Mapa de prueba: `assets/maps/src/dash.yaml` (`dash_test`). |
| `ability_wall_climb` | Reliquia esmeralda (alcance `Sim::kAbilityReach`, 12 px) que desbloquea **Trepar / Salto en Pared** (`kAbilityWallClimb`). Al rozar una pared sólida en el aire frena el descenso a `Sim::kWallSlideSpeed` (36 px/s) y habilita el salto en pared (`Sim::kWallJumpVx` = 138 px/s, `Sim::kWallJumpVy` = 232 px/s, coyote `Sim::kWallCoyoteFrames` = 6 frames, mezcla horizontal `Sim::kWallLockFrames` = 8 frames) preservando el Doble Salto. Sobrevive a morir y a R corto; se guarda en `save.ewsv` v4 (retrocompatible con v1–v3) y se limpia con R largo. Mapa de prueba: `assets/maps/src/wallclimb.yaml` (`wall_climb_secret`). |
| `break_wall` | Muro agrietado rompible de **1×3 tiles** (`kDoorWTiles` × `kDoorHTiles`, esquina sup. izq. en `(x, y)`). Sólido mientras `!active`; al embestirlo con **Dash**, se destruye (`active = true`), suelta escombros y libera el paso. Persiste tras morir, R corto y en `save.ewsv` v4 (retrocompatible con v1–v3); se restaura con R largo. |
| `mover` | Plataforma sólida de `w`×`h` **tiles** que recorre waypoints en ping-pong, a `speed` px/s (default 40) con `pause` s en cada extremo (default 0.5). `(x, y)` es la esquina **sup. izq.** (px) y el primer waypoint. El segundo sale de `to_x`/`to_y`, o una ruta más larga con `path: "x y x y ..."` (hasta 8 puntos en total, sin comas). `tile` es el id fuente del atlas (default 6, metal). Se dibuja con el atlas, con autotile 4 vecinos dentro del rect. El player parado encima viaja con ella (carry). No se atraviesa desde arriba, de costado ni desde abajo. **Aplastamiento:** la plataforma empuja; si después de empujar el player queda metido en un tile sólido o en otra plataforma, muere y reaparece en el checkpoint. `reset()` (R largo) y `respawn()` (R corto o muerte) la devuelven al primer waypoint. La cuerda colisiona contra ella; el agua no se toca. |
| `elevator` | Ascensor sólido que comparte `MoverRecord` en la sección `MOVER` del `.ewmap` (`w`×`h` en tiles, `speed`, `pause`, `tile` y dos waypoints vía `to_x`/`to_y` o `path`). *Link locking:* si aparece como destino en `links:` (`l.to_entity == m.ent`), arranca bloqueado (`active = false`, fijo en `wp[0]`) hasta activar el `switch` enlazado (`Scene::set_linked`); sin link arranca desbloqueado (`active = true`). Una vez desbloqueado, viaja entre `wp[0]` y `wp[1]` al pararse encima (`ridden_`, tras pausa `>= 0.65 s`) o al llamarlo desde arriba/abajo con E / X / ↓ vía `Sim::call_elevator(px, py)` (alcance `<= 28 px` en X, `<= 32 px` en Y). `respawn()` lo devuelve a `wp[0]` manteniendo el desbloqueo; `reset()` (R largo) restaura el bloqueo inicial. |

| `gravity_flip` | Orbe que **invierte la gravedad del player** al tocarlo (centro a ≤ `Sim::kFlipReach`, 10 px). Cada toque alterna normal ↔ invertida; el orbe se rearma cuando el player se aleja más de `Sim::kFlipRearm` (18 px), así que quedarse encima no lo repite. Con gravedad invertida el player cae hacia arriba, el techo funciona como piso (salto, `grounded`, carry de un `mover` por su cara de abajo, aplastamiento contra el piso) y se dibuja con los pies arriba. Ver “Gravedad invertida” abajo. |

| `boss` | Boss chiquito: caja de `Sim::kBossW`×`kBossH` (24×16 px); `(x, y)` es el **centro de los pies** (ponelo con `y` = superficie del piso). Patrón determinista en loop: camina `kBossWalkSec` (1.6 s) en ping-pong a ±`kBossRange` (64 px) de su posición, se queda quieto `kBossTellSec` (0.5 s, se ilumina: aviso) y salta (~33 px). Se vence con **3 pisotones** (`kBossHits`): caerle encima (moviéndose hacia abajo, centro por encima de su cabeza). Cada golpe lo hace más rápido y le da `kBossHurtSec` (1 s) de invulnerabilidad (parpadea; se lo puede atravesar). Tocarlo de cualquier otra forma **mata**. Con gravedad invertida el pisotón es desde abajo (en pleno salto). Al vencerlo se abren las puertas linkeadas (`links: - {from: boss, to: door}`) y, mientras haya un boss vivo, la meta **no cuenta**. Morir / R corto lo cura y lo devuelve a su lugar (si ya estaba vencido, queda vencido); R largo lo restablece entero. |
| `boss_dragon` | Jefe dragón multipatrón (el Wyrm de `level3`): caja de `Sim::kDragonW`×`kDragonH` (32×22 px) vencido con **4 pisotones** (`kDragonHits`). Alterna patrulla (`1.3 s`), aviso (`0.55 s`), **salto alado lanzando bolas de fuego (`Fireball`)** y **embestida rasa (`145 px/s` en ±`kDragonRange` 84 px)**. Las bolas de fuego matan al contacto salvo durante los i-frames de **Dash (`dash_ticks_ > 0`)**. Al caer derrotado abre sus puertas linkeadas y disipa todas sus llamas. |
| `imp` | Enemigo menor (diablillo de brasas, caja `10×10 px`): patrulla `±32 px` (`Sim::kImpRange`) a `34 px/s`. Cae de **1 golpe** tanto por **pisotón** como embistiendo con **Dash** (recarga salto aéreo y Dash); tocarlo de costado mata. Reaparece al morir o con R corto. |

Ejemplo:

```yaml
  - {name: plat, type: mover, x: 96, y: 128, w: 3, h: 1, to_x: 216, to_y: 128, speed: 80, pause: 0.25, tile: 6}
  - {name: lift, type: mover, x: 80, y: 144, w: 2, h: 1, path: "80 64 160 64", speed: 40, pause: 0.5}
```

`links: - {from: nombre_switch, to: nombre_door}` abre la puerta cuando el switch (o
`flood_switch`) está activo. Un mismo switch puede apuntar a varias puertas.

`dialogs: - {entity: npc_vigia, trigger: interact, speaker: "VIGIA", text: "Linea 1/Linea 2.|Pagina 2"}` asocia diálogos opcionales (`DIALOG = 10`) a cualquier entidad (`interact` con E/X/↓, `pickup` al recoger un huevo/reliquia o `proximity` al acercarse). Usá `/` para salto de línea (hasta 30 caracteres ASCII) y `|` para pasar de página.

`flood_switch` solo afecta agua cuyo rectángulo tiene `x ≥ 400` (la pileta del chunk 0
de `slice` no sube). Si tu pileta inundable está en el chunk 0, no va a subir.

### Protagonista: movimiento y Game Feel

- **Coyote time y Jump buffer:** el salto (`Sim::kJumpSpeed = 230` px/s, ~44 px de altura; caminata `Sim::kWalkSpeed = 90` px/s) admite **`Sim::kCoyoteFrames = 6`** frames de margen tras dejar un borde (sin gastar el Doble Salto) y **`Sim::kJumpBufferFrames = 6`** frames de buffer cuando se presiona salto justo antes de tocar el piso.
- **Squash & Stretch y polvo:** `Sim::squash_x()` / `Sim::squash_y()` (expuestos en `FrameData::player_squash_x` / `player_squash_y` y anclados a los pies del sprite) estiran al protagonista al saltar/doble saltar y lo comprimen al aterrizar o iniciar un Dash, decayendo suavemente a `(1, 1)`. Además emite partículas `Sim::DustParticle` (hasta `Sim::kMaxDust = 24`) al despegar, aterrizar, hacer doble salto, saltar desde pared al trepar (`wall-jump`) y durante la embestida (`Dash`, con estela visual de `Sim::kDashFlashFrames = 14` frames).
- **Hitstop y sacudida de cámara (Etapa 13):** `Sim::trigger_impact` congela la física del jugador y enemigos brevemente en impactos clave (`Sim::kHitstopEnemyFrames = 4` frames al pisar un boss, destruir `break_wall` o absorber daño en `take_hit`; `Sim::kHitstopDefeatFrames = 6` frames al derrotar un boss; `3` frames al eliminar un `imp`) mientras `victory_` y `wind_` siguen avanzando. Durante ese freeze (`hitstop_ticks_ > 0 && !finished_`), `Sim::step` conserva la intención de salto en `jump_buf_` (`kJumpBufferFrames`) si el jugador presiona salto justo en el impacto. En paralelo, `step_shake` / `eval_shake_offset` calculan un desplazamiento determinista con decaimiento lineal (`Sim::shake_x()`, `Sim::shake_y()` con amplitudes `Sim::kShakeHitAmp = 3.0f`, `Sim::kShakeBreakAmp = 2.5f`, `Sim::kShakeBossAmp = 4.5f` px y `2.2f` px en `imp`), sumado a `cam_x`/`cam_y` en `build_world_frame` sin desplazar `chunk_x_px`/`chunk_y_px`. Se activa o apaga desde Pausa con **`IMPACTO: SI/NO`** (`PauseMenu::kImpact` → `Sim::set_impact_fx_enabled`, que limpia de inmediato `hitstop_ticks_` y la sacudida al desactivarse y conserva la preferencia tras `reset()`). Probado sin GL en `game_feel` (`tests/game_feel_test.cpp`) y `pause_menu` (`tests/menu_test.cpp`).
- **Música por bioma y pasos por superficie (Etapa 14):** `Sim::surface_under_feet()` clasifica el suelo bajo los pies en `SurfaceKind` (`Stone`, `Moss`, `Metal`, `Water`) priorizando agua (`player_in_water()`), plataformas móviles (`ridden_ >= 0`), familia de tiles (`VIS`/`COL`) y `BackdropTheme` (`Engranajes` o tile familia `6` → `Metal`; familia `2`/`3` o `≤ 1` en `Pozo`/`Raices`/`Nido` → `Moss`; resto → `Stone`), emitiendo `kSfxStepStone`, `kSfxStepMoss`, `kSfxStepMetal` o `kSfxStepWater` (`SfxId::StepStone`, `StepMoss`, `StepMetal`, `StepWater`) cada **`14` frames** al caminar en el suelo (`!held_ && dash_ticks_ == 0`). En paralelo, `ew::game::music_zone_for(scene, sim)` (`engine/game/music.hpp`) mapea el `BackdropTheme` del chunk actual y la proximidad a bosses activos (`!defeated` dentro de `±320 × ±184 px`) a `MusicZone` (`Well`, `Catacombs`, `WyrmLair`, `Boss`, `Aqueduct`, `Tower`), reproducida por `Mixer::set_music_zone` / `device_set_music_zone` con crossfade suave de **`~1.5 s`** (`kMusicFadeSec = 1.5f`, `Mixer::music_fade()`, `set_music_enabled()`) y atenuación de volumen al `0.35×` al abrir el menú de pausa (`* 0.35f`). Cubierto sin GL por los tests `sfx` (`tests/sfx_test.cpp`) y `audio_feel` (`tests/audio_feel_test.cpp`).

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
- **Secuencia ordenada de victoria (`BossVictorySeq`, probada en `boss_victory_seq` y `mechanisms_crt`)**: al dar el
  golpe final en cualquiera de los 3 bosses (`slice`, `level2`, `level3`) se abren las
  puertas linkeadas y corre en orden estricto:
  1. **Animación de derrota (`DefeatAnim`, ~3.0 s / `Sim::kBossBurstFrames = 180` por defecto):**
     desintegración CRT en scanlines (`v.boss_count` alterna `0`/`1` según la paridad de `boss_burst()`),
     destello de pantalla (`flash_alpha`), 14 partículas/chispas radiales CRT,
     sacudida amortiguada de cámara (`shake_amp = 2.5` px) y sonido `SfxId::BossCrt` (`kSfxBossCrt`).
  2. **Pausa dramática (`AftermathPause`, 0.75 s por defecto):** sin CRT ni sacudida, con
     el jugador libre para moverse en cámara lenta (`slow_mo_scale = 0.55`).
  3. **Diálogo de salida (`ReadyForDialog` → `Done`):** se dispara una única vez al
     terminar la pausa (`SELLO DEL POZO` en `slice`, `SELLO DE LAS CATACUMBAS` en
     `level2` y `CORONA DEL WYRM` en `level3`, centralizados en `engine/game/story.hpp`
     y auditados por el test headless `story_tone`); reaparecer o cargar partida con el boss
     ya vencido deja la secuencia en `Done` y no repite el diálogo.

## Fondos (backdrops)

Cada pantalla (chunk) puede tener un **fondo procedural** detrás de los tiles. No hay imágenes:
lo dibuja el geometry pass (`assets/shaders/gbuffer_map.frag`) a partir de un tema y una seed, en
px enteros (pixel art), con una capa lejana con parallax 0.35 (se nota al deslizar la cámara
entre pantallas). El fondo recibe la luz del player a medias y tiene un ambiente frío propio, así
la silueta de la sala se lee pero queda detrás del primer plano. Algunos temas tienen luz propia
(gotas, esporas, luciérnagas, antorchas) que no depende del player.

```yaml
backdrops:
  - {cx: 0, cy: 0, theme: pozo}            # seed opcional (default cx*7 + cy*13)
  - {cx: 1, cy: 0, theme: raices, seed: 5}
```

| tema | motivo | dónde se usa |
|---|---|---|
| `cueva` | piedra gris azulada irregular. **Default** de todo chunk sin registro y de los mapas viejos | `hello`, `room` |
| `pozo` | pared curva de ladrillos, haz de luz desde la boca del pozo, gotas, musgo abajo | slice chunk 0 (sala 1, inicio y pileta) |
| `raices` | tierra, raíces colgantes, troncos lejanos, esporas | slice chunk 1 (cuerda) |
| `grieta` | roca rojiza partida con brasas tenues, estalactitas lejanas | slice chunk 2 (pozo de pinchos) |
| `cisterna` | bloques grandes, caños con bridas, marcas del nivel del agua, verdín | slice chunk 3 (sala 2, agua que sube) |
| `nido` | ladrillo cálido con nichos en arco y huevos con dibujo, luciérnagas | slice chunk 4 (sala 3, huevo) |
| `engranajes` | paneles remachados, engranajes lejanos que giran, cadenas | slice chunk 5 (sala 4), `mover` |
| `invertido` | arquitectura espejada techo/piso, franja de estrellas, polvo que cae hacia arriba | slice chunk 6 (sala 5), `gravity` |
| `arena` | arcada de coliseo en dos pisos, sello con el ojo de la bestia, antorchas | slice chunk 7 (sala 6, boss), `boss` |
| `vacio` | el fondo liso de antes (azul casi negro) | para comparar o salas "en blanco" |
| `cripta` | arcos góticos de pizarra, estandartes y braseros violetas | `level2` chunk `(0,0)` (entrada a Las Catacumbas) |
| `guarida` | estrato basáltico oscuro, grietas de magma tenue y costillar fósil de dragón | `level2` chunk `(3,0)` (guarida del boss dracónico) |
| `santuario` | pilastras de mármol oscuro, vitral ojival y runas arcanas tenues | `level2` chunk `(3,1)` (altar del huevo y santuario) |
| `abismo` | sillares de obsidiana azul-violácea, arcadas lejanas, cristales/esporas abisales con luz propia y ojos en la penumbra | `level2` chunk `(1,0)` (hub vertical profundo `c10`) |
| `aqueduct` | bloques húmedos con marcas del nivel de agua, caños con bridas, burbujas que suben y verdín (Archivo Sumergido) | biomas nuevos (mapa de prueba `biomes`) |
| `tower` | sillares altos, vitrales ojivales tenues, campanas colgantes y polvo que cae (Torre de los Ecos) | biomas nuevos (mapa de prueba `biomes`) |

- Sección binaria `BACKDROP` (id 9), **opcional**: un mapa sin `backdrops` compila byte a byte
  igual que antes, y un loader viejo ignora la sección (ver [ewmap.md](ewmap.md)).
- `ewmapc` rechaza un tema desconocido, un chunk fuera de `size_chunks`, dos fondos para el
  mismo chunk o una seed fuera de 0..65535. En runtime un id fuera de rango se dibuja como `cueva`.
- Elegí el tema por lo que pasa en la sala y pensá en la lectura: el fondo es oscuro y de poco
  contraste, pero evitá un tema rojizo (`grieta`, `arena`) justo detrás de pinchos rojos si
  la sala depende de verlos de lejos. Los tiles `VIS` sin `COL` (columnas de decorado) se siguen
  dibujando encima del fondo.
- Para mirar los fondos: `tools/room_shots.py build out --png` (con display o `xvfb-run`) captura
  y valida las 46 salas de campaña (`slice`: 8, `level2`: 8, `level3`: 6, `world`: 24) y compara cada
  una con su misma sala en `vacio`. CI lo corre en el job `engine` y sube las capturas reales
  (`fondos/sala*.ppm`, `fondos/level2_*.ppm`, `fondos/level3_*.ppm`, `fondos/world_*.ppm`, excluyendo
  `*_vacio.ppm`) como artifact `fondos-salas`.

## Iluminación por bioma, farol y luces locales (`lighting.hpp`)

El sistema multipunto sin GL (`engine/game/lighting.hpp`, invocado al final de `build_world_frame` en `engine/game/frame_view.hpp` y cubierto por el test `room_lighting` en `tests/lighting_test.cpp`) combina la luz principal del protagonista con hasta **`kMaxPointLights = 12`** luces puntuales por frame (`FrameData::point_light_count`, `point_light_pos`, `point_light_col`):

- **Oscuridad ambiental (`biome_darkness`):** toma el `BackdropTheme` del chunk activo y fija `FrameData::ambient_darkness`:
  - `0.85` en biomas profundos (`Abismo`, `Guarida`, `Cripta`).
  - `0.68` en biomas intermedios (`Pozo`, `Raices`, `Grieta`, `Cueva`, `Invertido`, `Arena`, `Aqueduct`).
  - `0.48` en biomas tenues/arquitectónicos (`Cisterna`, `Nido`, `Engranajes`, `Santuario`, `Tower`).
  - `0.0` en `Vacio`.
- **Farol principal del héroe (`collect_room_lights`):** posiciona `light_x` / `light_y` adelantado frente al jugador (`player.pos.x + player_facing * 4 px`, `player.pos.y - 8 * down px`) con radio `lantern_radius = 125 px` para iluminar paredes, pisos y relieves cercanos.
- **Luces puntuales (`PointLightKind`, descartadas a `> 48 px` fuera de cámara):**
  1. **Entidades dinámicas (prioridad):** las bolas de fuego del Wyrm (`sim.fireballs()`) emiten luz **`Lava`** (`r = 44 px`); los `checkpoint` y `switch`/`flood_switch` emiten luz **`Shrine`** (cian → verde al activar el altar; ámbar → verde al accionar la palanca); los `collectible` y reliquias (`ability_double_jump`, `ability_dash`, `ability_wall_climb`) sin recoger emiten luz **`Crystal`** (`r = 32 px`).
  2. **Luces ambientales deterministas por bioma (vecindario 3×3, siempre en aire):**
     - **`Torch`:** hasta 3 antorchas titilantes por chunk (`r = 58 px`, cálido) junto a muros o techos en `Pozo`, `Cripta`, `Cueva`, `Santuario`, `Abismo`, `Arena` y `Tower`.
     - **`Crystal`:** hasta 2 cristales bioluminiscentes por chunk (`r = 46 px`, alternando cian y violeta) anclados en techos o pisos en `Cripta`, `Santuario`, `Abismo`, `Cisterna`, `Invertido`, `Raices`, `Aqueduct` y `Tower`.
     - **`Lava`:** hasta 2 focos de lava/brasas por chunk (`r = 54 px`, rojo-anaranjado) sobre el suelo bajo en `Grieta`, `Guarida` y `Arena`.

## Vegetación, brisa e interacción ambiental

- **Pasto pixel-art con variantes, hongos y flores (`assets/shaders/gbuffer_map.frag`):** sobre pisos y repisas sólidas (`floor_1`, despejado por `near_mech` en `|dx| ≤ 11 px, |dy| ≤ 18 px` alrededor de altares `shape 2` y palancas `shape 3`) se dibujan matas cada 5 px con claros entre sí y 3 variantes deterministas de altura: cortas (`2–3 px`), medianas arqueadas (`4–5 px`) y juncos altos (`6–8 px`), usando paletas de 3 tonos según el tema del chunk (verde musgo en `pozo`/`raices`/`nido`/`cueva`/`engranajes`, verde-azulado espectral en `cripta`/`santuario`/`abismo`/`cisterna`, oliva-ocre con punta ámbar en `grieta`/`guarida`/`arena` e índigo-lavanda en `invertido`). En los temas de catacumbas/subterráneos (`cripta`, `santuario`, `abismo`, `cisterna`, `raices`, `grieta`) brotan **hongos** chicos (tallo `2–3 px` y sombrero cian espectral u ocre con brillo tenue), mientras que en el resto de los biomas aparecen **flores silvestres** mecidas por el viento.
- **Brisa global con ondas viajeras (`engine/game/wind.hpp` y `wind_sway` en `gbuffer_map.frag`):** `WindField` evalúa `wind_gust(time_s, seed)` (ráfaga lenta acotada a `[0.35, 1.85]`) multiplicada por `wind_wave(world_x, time_s, seed)` (onda viajera horizontal determinista según la `seed` de la sala).
- **Interacción física y ráfagas locales (`Sim` + `WindField`):** además de la brisa global (`u_wind_gust`), `WindField` mantiene hasta `kMaxImpulses = 4` impulsos empaquetados para el renderer (`u_wind_imp_count`, `u_wind_imp`):
  - **Caminar:** estela continua en el slot 0 (`set_wake`, radio `16 px`, centrada en los pies mientras el jugador está en el suelo o recién despega).
  - **Aterrizar de un salto:** ráfaga local de radio `24 px` (`0.38 s`) al tocar el piso.
  - **Embestida (Dash):** ráfaga direccional fuerte de radio `30 px` (`0.25 s`) durante `dash_ticks_ > 0`.
  - **Enemigos y bosses al caer/derrotarlos:** aterrizaje tras salto del coloso (`28 px`, `0.38 s`) y del Wyrm (`34 px`, `0.42 s`), golpe final al boss (`44 px`, `0.55 s`) y derrota de `imp` (`22 px`, `0.35 s`).
- **Reacción en telarañas, hojas, cuerdas y estandartes:** el campo de brisa y sus ráfagas locales inclinan pasto, hongos y flores, hacen vibrar **telarañas** en esquinas bajo techos, curvan **enredaderas y cordeles colgantes**, arrastran **hojas y polvo** en el aire según `u_wind_gust`, mecen los **estandartes** rasgados de `cripta` y, en `Sim::step`, empujan horizontalmente los nodos libres de las **cuerdas Verlet** (`wind_.sample_at(pt.pos.x, pt.pos.y, 1u) * 24.f`).
- **Menú de pausa (`BRISA SI` / `BRISA NO` y `HEROE`):** el ítem `PauseMenu::kBrisa` (entre `CONTROLES` y `HEROE`) arranca activado (`foliage() == true`) y alterna con `Enter`/`Espacio`/`←`/`→` sin cerrar la pausa. En `NO`, `Sim::set_wind_enabled(false)` limpia impulsos, desactiva la fuerza sobre cuerdas y pone `u_foliage = 0` (oculta el follaje/telarañas/hojas animados y detiene los estandartes). El ítem `PauseMenu::kHero` abre el selector visual **`ELIGE TU HEROE`** (`Sr. del Farol`, `La Zarigüeya`, `El Castor`). Probado en `wind_test` (`wind_breeze`) y `menu_test` (`pause_menu`).

## Flecha guía de navegación (`guide`)

El motor expone la lógica de navegación en `engine/game/guide.hpp` (`GuideArrow` y `compute_guide_arrow(const Scene&, const Sim&, float time_s = 0.f)`), leyendo la sección opcional `GUIDE` (`id = 11`, registros `GuideRecord` de 24 B). Por defecto en pantalla no se dibuja la flecha orbitando al personaje (`show_player_guide = false` en `build_world_frame`); solo los switches sin accionar conservan su indicador `↓` encima.

- **Reglas generales en runtime:**
  - Si `sim.finished()` es `true` o la distancia entre el jugador y el objetivo es `≤ 24 px`, `compute_guide_arrow` devuelve `valid = false`.
  - Si el mapa tiene sección `GUIDE`, elige el primer registro `Objective` vigente y, si existe un `Shaft` cuyo `split_y` separa al jugador del objetivo (`py < split_y && ty >= split_y` o viceversa), rutea hacia la boca del pozo más cercano en X (horizontal hasta `|px - sx| ≤ 42 px` y luego vertical hacia `y_bot` o `y_top`).
  - **Fallback automático sin `GUIDE`:** si `map().guides()` está vacío (mapas viejos como `hello.ewmap`), recorre `links:` y apunta al primer `switch` o `flood_switch` inactivo enlazado a una `door` todavía cerrada (`style = 0`).

### Claves YAML de `guide:`

- **Objetivos (`kind: objective`, valor por defecto si se omite `kind`):**
  - `target`: nombre de la entidad destino en `entities` (obligatorio). Si `target` es una `door`, el punto objetivo usa el centro vertical de la puerta (`y + 12 px`).
  - `gate`: nombre de la puerta/muro asociado (opcional; el objetivo se da por cumplido si `target` o `gate` está activo). Mutuamente excluyente con `until`.
  - `until`: nombre de la entidad que mantiene vigente un objetivo de cruce hasta abrirla/romperla; activa `cross` automáticamente. Mutuamente excluyente con `gate`.
  - `style`: `switch` (ámbar, `style = 0`, por defecto) o `ability` (cian, `style = 1`, flag `kGuideAbility`).
  - `cross`: `true`/`false` (`kGuideCrossWhenOpen`: activo cuando `target->active` es `true` y falta abrir/romper `until` o cumplir la restricción de posición).
  - `forward_x`: coordenada X en px (`kGuideForwardX`); el objetivo se considera superado al avanzar con `player.x > forward_x`.
  - `below_y`: coordenada Y en px (`kGuideBelowY`); en objetivos `cross`, solo aplica mientras `player.y >= below_y` (si también se definió `forward_x`, se descarta cuando `player.y < below_y && player.x > forward_x`).
- **Pozos verticales (`kind: shaft`):**
  - `x`: centro X para bajar por el pozo en px (obligatorio).
  - `y_top`, `y_bot`: cotas Y de las bocas superior e inferior (`y_top <= y_bot`, obligatorios).
  - `up_x`: centro X de retorno al subir desde planta baja en px (opcional; por defecto usa `x`).
  - `split_y`: altura Y en px que separa planta alta y planta baja (opcional; por defecto `184`, equivalente a `kChunkH * kTileSize`).

```yaml
guide:
  - {kind: shaft, x: 448, up_x: 472, split_y: 184, y_top: 148, y_bot: 280}
  - {target: rel_double_jump, style: ability}
  - {target: sw_sanctum, gate: door_hub_gallery}
  - {target: door_hub_gallery, until: wall_guardian, cross: true, below_y: 184, forward_x: 640}
```

Probado sin GL en `tests/guide_test.cpp` (`guide_arrows`).

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

En el árbol de build, CMake ya compila `world` (generado por `tools/gen_world.py`, verificado por `world_gen_check` y `world_metroidvania`), `hello`, `slice`, `level2`, `level3`, `room`, `mover`, `gravity`, `boss`, `ability`, `dash` y `wallclimb` hacia `build/maps/*.ewmap`.
`ctest` corre `ewmapc_*` + `ewmap_load_*` (y el job `build` de CI lo vuelve a
compilar y pasar por `ewmap_smoke`).

## Enganchar la sala al juego

El prototipo arranca en **`world`** (`EW_DEFAULT_MAP`: `build/maps/world.ewmap`, mundo unificado `8×3`). Para probar la tuya o los capítulos sueltos (como `slice`) no hace falta cambiar el default:

```sh
./build/echo_engine --map build/maps/room.ewmap
./build/echo_engine --map build/maps/slice.ewmap
```

Para que CMake la compile siempre, como `room.yaml`:

1. Dejá la fuente en `assets/maps/src/<nombre>.yaml`.
2. En `CMakeLists.txt`, copiá el bloque de `EW_ROOM_MAP` (custom command + `ew_maps` +
   `add_test` `ewmapc_*` / `ewmap_load_*` con `ewmap_smoke --name … --chunks … --has …`).
3. En `.github/workflows/ci.yml`, en el paso “Compile example maps”, agregá el par
   `ewmapc --crc` + `ewmap_smoke` con los mismos flags.
4. Si querés que viaje en el paquete portable, sumala a `install(FILES …)`.

`echo_engine --map` alcanza para jugarla; el default del binario se deja en `world`.

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
- **Sala final con boss (sala 6 de `slice`).** Arena plana de una pantalla: checkpoint a
  la entrada (fuera de la patrulla), boss en el medio y la meta detrás de una puerta
  linkeada al boss. Después de las salas de mecánicas, la pelea solo pide saltar y medir
  la distancia; lo nuevo es leer el patrón (el aviso antes del salto).
  `slice_test` la pelea con `solve_room6`.
- **El secreto es opcional.** Un `collectible` se esconde al costado o “para atrás”
  (agua baja, puertas en la pileta). El HUD n/total informa; no bloquees el `goal`.
- **Estructura metroidvania y backtracking (`level2.yaml`).** En mapas 2D de varias plantas
  (como `level2`, 4×2 chunks), mostrá desde el inicio zonas altas o puertas cerradas que el
  jugador todavía no puede alcanzar (un salto simple sube ~44 px; con **Doble Salto** llega a
  ~82 px, así que una repisa a 56–64 px sobre el piso sirve como candado natural de habilidad).
  Un pozo vertical con agua abajo permite bajar sin daño de caída y exige Doble Salto por
  escalones intermedios para volver a subir (backtracking vertical).
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
