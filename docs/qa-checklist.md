# Checklist de QA — Echo Well v0.1.0

Versión de prueba para QA del mundo Metroidvania unificado (**`world.ewmap`**, grilla `8×5`: **El Pozo → Las Catacumbas → La Guarida del Wyrm → Archivo Sumergido → Torre de los Ecos** con atajos de un solo sentido y ascensores; el ejecutable arranca ahí por defecto, mientras los capítulos sueltos `slice`, `level2` y `level3` siguen probándose con `--map`), habilidades desbloqueables (**Doble Salto**, **Dash** y **Trepar / Salto en Pared**), diálogos de huevos de dragón, mini-mapa exploratorio y pelea final contra el Wyrm. No es un release público. Los paquetes salen como *artifacts* del CI de GitHub Actions en `main`.

Antes de empezar, anotá la versión: `echo_engine --version` (tiene que decir `v0.1.0`).
También aparece en el título de la ventana y abajo a la derecha del menú de pausa.

## 1. Instalar y correr

Los paquetes están en la página del run de CI de `main` (sección **Artifacts**). Hace falta
iniciar sesión en GitHub para descargarlos.

| Artifact | Contenido |
|---|---|
| `echo-well-linux-x86_64` | zip con `echo-well/` |
| `echo-well-windows-x86_64` | zip con `echo-well/` |

Adentro de `echo-well/` tiene que haber: `echo_engine` (o `echo_engine.exe`), `ewmapc`,
`ewmap_smoke`, `assets/shaders/`, `assets/atlas/tiles.png`, `assets/atlas/sprites.png`,
`assets/maps/bin/{world,slice,level2,level3,hello,room,mover,gravity,boss,ability,dash,wallclimb}.ewmap`, `README.md`, `DIST.txt` y
`docs/qa-checklist.md` (este archivo).

### Linux (x86_64)
1. Descomprimí el artifact. GitHub lo baja como un zip que adentro tiene otro zip: descomprimí los dos.
2. Requisitos: glibc reciente, OpenGL 3.3 y X11 (o XWayland). En Debian/Ubuntu:
   `sudo apt install libgl1 libx11-6`.
3. `cd echo-well && chmod +x echo_engine && ./echo_engine`
4. Sin GPU o en una VM: `LIBGL_ALWAYS_SOFTWARE=1 ./echo_engine`.

### Windows 10/11 (x64)
1. Descomprimí el artifact (zip dentro de zip) en una carpeta con permisos de escritura.
2. Doble clic en `echo-well\echo_engine.exe`. El binario no está firmado: si aparece
   SmartScreen, elegí "Más información → Ejecutar de todos modos".
3. Hace falta una placa con OpenGL 3.3 o superior (cualquier GPU de los últimos ~10 años).

### Opciones útiles
| Opción | Qué hace |
|---|---|
| `--version` | muestra la versión y sale |
| `--scale N` | tamaño de ventana (320×180 × N; por defecto 4) |
| `--nosound` / `EW_NOSOUND=1` | sin sonido |
| `--nogamepad` / `EW_NOGAMEPAD=1` | ignora el gamepad |
| `--nosave` / `EW_NOSAVE=1` | no lee ni escribe la partida guardada |
| `EW_SAVE_DIR=carpeta` | usa otra carpeta para el guardado (útil para probar desde cero) |
| `--hero farol\|zarigueya\|castor` | elige el personaje inicial directo sin abrir el selector |
| `--map assets/maps/bin/slice.ewmap` | carga directo el capítulo suelto (`slice`, `level2`, `level3`, `wallclimb`, `dash`, `boss`, `ability`, `gravity`, `mover`, `room`); por defecto arranca en `world.ewmap` |
| `--next-map assets/maps/bin/ability.ewmap` | define el mapa que carga **SIGUIENTE NIVEL** al completar el actual |

La consola (terminal en Linux; en Windows abrí `cmd` y corré `echo_engine.exe` desde ahí)
muestra mensajes útiles para los reportes: mapa cargado, huevo agarrado, partida guardada
o cargada, meta.

## 2. Controles (por defecto)

| Acción | Teclado | Gamepad |
|---|---|---|
| Caminar | ← → o A D | stick izquierdo o d-pad |
| Saltar / nadar / Doble Salto aéreo | Espacio, Z (también ↑ o W si no estás en la cuerda, flancos separados) | A, RB |
| Activar switch / diálogo | E, X (también ↓ o S si no estás en la cuerda) | X, B |
| Embestida (Dash, al desbloquear reliquia) | E, X, K, Shift derecho (si no hay switch/diálogo cerca) | X, B |
| Agarrar / soltar cuerda | C, Shift izquierdo | LB, Y |
| Trepar (colgado) | ↑ ↓ o W S | stick o d-pad arriba/abajo |
| Volver al último checkpoint | R corto (al soltar) | — (menú → REINICIO) |
| Reiniciar todo al inicio | R mantenido ~0,8 s | — |
| Menú de pausa | Esc | Start (B vuelve atrás) |

El menú siempre se navega con flechas/WASD + Enter/Espacio/Z, o d-pad + A/B.
Menú: SEGUIR, CONTINUAR (si hay partida guardada), REINICIO (vuelve al checkpoint),
VOL (←/→ cambia, Enter silencia), CONTROLES, BRISA SI/NO (Enter/Espacio/←/→ alterna brisa y vegetación sin cerrar), IMPACTO SI/NO (Enter/Espacio/←/→ alterna hitstop y sacudida de cámara sin cerrar), HEROE (abre el selector de personaje), SALIR.

## 3. Recorrido sala por sala

Para cada sala: llegá, morite a propósito en cada peligro y verificá que reaparezcas en el
checkpoint correcto, con la cámara bien ubicada y sin quedar trabado. Los **altares de
checkpoint** (`shape 2`) pasan de cristal cian a llama verde al tomarlos, y las **palancas
abre-puertas elevadas** (`shape 3`) muestran una flecha verde encima mientras están en ámbar y
pasan a verde al accionarlas; ambos quedan **despejados de pasto** alrededor (`near_mech`).

- [ ] **Selector de 3 personajes (`Sr. del Farol`, `La Zarigüeya`, `El Castor`) y objetos.** Verificá que al arrancar sin partida guardada se abre **`ELIGE TU HEROE`** con retratos animados en vivo (↑/↓ o W/S elige, Enter/Espacio confirma, Esc cierra), y que desde Pausa → **`HEROE`** podés cambiar de personaje en cualquier momento (o iniciando con `--hero farol|zarigueya|castor`). Probá animaciones de reposo, caminata (orientada a ←/→), salto/caída, despliegue de alas cian al usar Doble Salto (`245 px/s`), trepada en cuerda, nado y parpadeo post-respawn en los 3 héroes; con gravedad invertida el sprite queda invertido verticalmente apoyado en el techo. Los coleccionables se muestran como huevos de dragón escamados, la reliquia de Doble Salto como emblema alado cian y la de Dash como reliquia ígnea dorado-carmesí.
- [ ] **Sensación de movimiento (Coyote time, Jump buffer, Squash & Stretch, polvo y estela de Dash).**
  - Saltar justo al salir de un borde (~6 frames de margen / coyote time, sin gastar Doble Salto) y presionar salto justo antes de tocar el piso (jump buffer, ~6 frames).
  - Verificar compresión/estiramiento del personaje al saltar y aterrizar manteniendo los pies apoyados en el piso, motas de polvo al despegar/aterrizar/trepar y estela residual al usar Dash.
- [ ] **Hitstop, sacudida de cámara y opción `IMPACTO SI/NO`.**
  - Verificar la micro-pausa de impacto y sacudida de cámara al pisar/vencer bosses, eliminar diablillos `imp`, romper `break_wall` o absorber un golpe.
  - Abrir Pausa (`Esc`/`Start`), bajar a **`IMPACTO: SI`** y cambiar a **`NO`** sin cerrar el menú: comprobar que desaparecen hitstop y sacudida manteniendo intacto el menú en 9 filas alineadas con la barra de volumen.
- [ ] **Música ambiental por zona y pasos según suelo.**
  - Caminar sobre musgo/tierra en *El Pozo*, piedra/ladrillo en *Las Catacumbas*, plataformas metálicas (`mover`/`elevator`) y agua poco profunda: distinguir los 4 timbres de pasos.
  - Escuchar el cambio suave (~1,5 s) entre la atmósfera de *El Pozo*, *Las Catacumbas*, *La Guarida del Wyrm* y la música de combate al entrar en la arena de un boss (y la atenuación al pausar o silencio total con `--nosound` / `MUTE`).
- [ ] **Lectura visual limpia sin flecha junto al protagonista.** Verificá que no aparece ninguna flecha orbitando al lado del personaje (los abre-puertas sin accionar conservan únicamente su indicador `↓` arriba). La lógica de cálculo basada en la sección `GUIDE` (`engine/game/guide.hpp`) sigue cubierta por `guide_arrows`.
- [ ] **Diálogos dracónicos y lore de los huevos (`npc`, `lore`, `collectible`).** En la entrada de `slice` (Vigía del Farol) y en `level2` (Cronista de las Catacumbas y Estela del Santuario), al acercarte aparece un indicador dorado y apretar **E**/**X**/**↓** abre la caja de diálogo pixel-art (con `Enter`/`Espacio`/`E` avanza página y `Esc` cierra). Al recoger cada huevo de dragón o la reliquia alada se dispara automáticamente el eco dracónico que revela la historia.
- [ ] **Mini-mapa exploratorio Metroidvania (esquina superior derecha en `world`, `slice`, `level2`, `level3`).**
  - Las salas no visitadas permanecen **ocultas** (sin revelarse en la grilla hasta entrar a ellas).
  - Al explorar nuevos chunks, cada sala visitada se revela en el mini-mapa (resaltando la sala actual) y se actualiza el contador de **porcentaje explorado (`%`)** hasta `100%` al recorrer todo el mapa.
  - En las salas ya visitadas con puertas cerradas (`door` / `one_way_door`) o muros agrietados intactos (`break_wall`) aparece un **marcador carmesí**, que desaparece al abrir la puerta o romper el muro.
  - En las salas ya visitadas con huevos sin recoger (`collectible`) aparece un **marcador dorado**, que desaparece al agarrar el huevo.
- [ ] **Sala 1 (inicio, pinchos y checkpoints).** Aparecés abajo a la izquierda; el HUD
  arriba a la izquierda dice `0/1`. El switch abre la puerta y hay que nadar la pileta (el
  agua frena la caída y se nada hacia arriba). Una cuerda sube una pared que no se salta (la
  cuerda no atraviesa paredes). En la cornisa está el checkpoint `ck_ledge`, después un hueco
  con pinchos: morir ahí te devuelve a la cornisa. Al final está el `flood_switch`.
- [ ] **Sala 2 (agua que sube).** Activar el `flood_switch` sube el nivel de la pileta
  siguiente para nadar hasta la cornisa de salida. Checkpoint `ck_exit` a la salida.
- [ ] **Sala 3 (huevo, opcional).** Con el agua **baja** (sin activar el flood), el switch
  dentro de la pileta abre dos puertas. Se nada al pasillo, se trepa la segunda cuerda y se
  agarra el huevo: el HUD pasa a `1/1` en dorado, con destello y sonido. También se llega por
  arriba, desde la cornisa de salida. La meta **no** exige el huevo.
- [ ] **Sala 4 (plataformas móviles).** Checkpoint `ck_mover`. El piso es todo pinchos. Una
  plataforma horizontal te lleva; un ascensor vertical baja al piso seguro. Probá que la
  plataforma te arrastre, que no la puedas atravesar y que te mate si te aplasta contra una pared o un techo.
- [ ] **Sala 5 (gravedad invertida).** Checkpoint `ck_grav`. El orbe violeta del piso invierte
  la gravedad (el personaje queda con los pies arriba). Se cruza por el techo, colgado debajo de una plataforma
  móvil, y el orbe del techo devuelve la gravedad. Morir te devuelve con la gravedad que
  tenías en el checkpoint.
- [ ] **Sala 6 (boss).** Checkpoint `ck_boss` fuera de su alcance. El boss camina, se ilumina
  (aviso) y salta. Se le pega cayéndole encima: 3 pisotones. Cada golpe lo acelera y lo hace parpadear.
  Tocarlo de costado te mata y morir lo cura. Al vencerlo se abre la puerta de la derecha y
  corre la secuencia ordenada de victoria: animación CRT + destello + partículas + sacudida (~3,0 s) →
  pausa breve (~0,75 s) con jugador libre en cámara lenta → diálogo de salida (`SELLO DEL POZO`,
  que no se repite al reiniciar/cargar con el boss ya vencido).
- [ ] **Secuencia CRT post-boss en los 3 bosses (`slice`, `level2`, `level3`).** Al derrotar al
  Vigilante de `slice` (3 pisotones), a `boss_catacomb` en `level2` (3 pisotones) o al
  `wyrm` en `level3` (4 pisotones), verificá el orden estricto: primero animación CRT
  (~3,0 s / 180 frames de desintegración en scanlines + destello + 14 chispas radiales +
  sacudida de cámara + sonido CRT `BossCrt`), luego pausa breve en cámara lenta donde
  seguís pudiendo moverte libremente (~0,75 s) y recién al terminar aparece una sola vez el
  diálogo de salida (`SELLO DEL POZO`, `SELLO DE LAS CATACUMBAS` o `CORONA DEL WYRM`).
- [ ] **Historia y tono (*Calabozos y Dragones*) + salto de diálogos.** Verificá que todos los carteles, NPCs, estelas, ecos de huevos y diálogos de boss mantienen el lore de dragones/nidadas (cualquier diálogo se avanza con `Enter`/`Espacio`/`E` y se puede saltear/cerrar de inmediato con `Esc`, validado también por el test headless `story_tone`).
- [ ] **Fondos y arte dark fantasy.** Cada pantalla tiene su fondo (`slice` conserva pozo, raíces, roca con
  brasas, cisterna, nichos, engranajes, invertido y arena; `level2` suma `cripta`, `abismo`, `guarida` y `santuario`).
  Los tiles muestran mampostería, ladrillos, madera remachada y musgo renovados, y los pinchos `META 4` se ven como
  púas pixel-art de acero y punta carmesí con alto contraste sobre el fondo.
  Verificá en particular que el fondo del Hub `c10` (`abismo`, `(1,0)` de `level2`) tenga lectura clara y contraste suficiente frente al primer plano y los bordes del pozo.
  Anotá si algún fondo hace difícil ver un pincho, una plataforma o un switch.
- [ ] **Iluminación dinámica (Farol, Antorchas, Cristales y Lava).**
  - Verificá que en zonas oscuras (`Cripta`, `Abismo`, `Guarida`, `Pozo`) la luz ambiental baja según el bioma y el farol del protagonista ilumina dinámicamente paredes, relieves y tiles cercanos al moverse, girar o invertir gravedad.
  - Verificá antorchas titilando junto a muros en `Pozo`/`Cripta`/`Santuario`, brillos cian/violeta de cristales en *Las Catacumbas* y resplandor cálido de lava/brasas en *La Guarida del Wyrm*, además de la luz dinámica de las bolas de fuego del dragón y el cambio de color en altares (`checkpoint`), palancas y reliquias sin recoger.
- [ ] **Pasto, brisa e interacción ambiental (`BRISA SI/NO`).**
  - Verificá sobre pisos y repisas el **pasto pixel-art con variantes** (despejado alrededor de palancas `shape 3` y altares `shape 2` vía `near_mech` para no taparlos) y, en Las Catacumbas (`level2`), los **hongos y flores** bioluminiscentes.
  - Verificá la **brisa global con ondas viajeras** recorriendo la sala y la reacción local al **caminar** (estela en los pies), **aterrizar** de un salto, **embestir con Dash** y al **caer/aterrizar enemigos y bosses** (coloso, `imp`, Wyrm).
  - Comprobá que reaccionan tanto el pasto/hongos/flores como las **telarañas**, **hojas/enredaderas colgantes**, **estandartes** (p. ej. `cripta`) y el balanceo de las **cuerdas**.
  - Abrí el menú de pausa (`Esc`/`Start`), bajá hasta **`BRISA SI`** y pulsá `Enter`/`Espacio`/`←`/`→`: debe cambiar a **`BRISA NO`** sin cerrar el menú, deteniendo la animación de vegetación y el empuje del viento en las cuerdas; al volver a **`BRISA SI`** todo se reanuda.
- [ ] **Meta y pantalla de final.** Detrás de esa puerta: se abre la pantalla de final pixel-art
  (**NIVEL COMPLETADO**) con el resumen de huevos, muertes, habilidades, tiempo y rango **S/A/B**, y las opciones
  **SIGUIENTE NIVEL** (habilitada en `slice` para pasar a `level2`, o cuando se pasa `--next-map`),
  **REINICIAR NIVEL**, **MENU DE PAUSA** y **SALIR**. Con el boss vivo la meta no cuenta.
  Después de la meta, R corto vuelve al checkpoint y R largo al inicio con el HUD en `0/1`.
- [ ] **Nivel 2 — Las Catacumbas (`level2`, 4×2 salas metroidvania).** Al elegir **SIGUIENTE NIVEL**
  tras completar `slice` (o con `--map assets/maps/bin/level2.ewmap`):
  1. En la entrada `(0,0)`, verificá que el huevo de la repisa alta se ve pero no se alcanza con un salto simple (`0/2` en el HUD).
  2. Avanzá al Hub `(1,0)` (`c10`), verificá que su fondo (`abismo`) tenga lectura clara y contraste suficiente, bajá por el pozo vertical hasta caer al agua de `(1,1)` y caminá a la izquierda hasta `(0,1)` para tomar la reliquia cian de **Doble Salto** (aparece el ícono en el HUD).
  3. Volvé a `(1,1)`, subí con Doble Salto al Escalón 1 de la derecha, cruzá `(2,1)` sobre la plataforma móvil `plat_crypt` y en `(3,1)` activá `sw_sanctum` (abre la puerta alta del Hub); luego subí al altar con Doble Salto para agarrar `egg_catacomb_2` (`1/2`) y la reliquia ígnea **`rel_dash`** (`ability_dash`, aparece la flecha ardiente en el HUD junto al doble salto).
  4. Hacé backtracking a `(1,1)`, subí con Doble Salto por los Escalones 1 y 2 de vuelta al Hub `(1,0)` y regresá a `(0,0)` para subir a la repisa inicial y agarrar `egg_catacomb_1` (`2/2` en dorado).
  5. Desde el Hub `(1,0)`, subí con Doble Salto a la Galería Alta, cruzá `door_hub_gallery` hacia `(2,0)` (gravedad invertida por el techo), rompé el muro agrietado **`wall_guardian`** embistiendo con **Dash** (`E`/`X`/`K`/`Shift der.`) al entrar a `(3,0)` y vencé a `boss_catacomb` (3 pisotones) para abrir `door_boss2` y llegar a la meta (`HABILIDADES: DOBLE SALTO + DASH`), habilitando **SIGUIENTE NIVEL** hacia `level3`.
- [ ] **Nivel 3 — La Guarida del Wyrm (`level3`, 3×2 salas finales):**
  1. En `(0,0)`, recogé o conservá Doble Salto + Dash, rompé `wall_entry` embistiendo y saltá sobre el foso hacia `(1,0)`.
  2. En `(1,0)` recogé `egg_wyrm_1` sobre la repisa alta (`1/2`), bajá por el pozo vertical al agua de `(1,1)`, cruzá el corredor invertido con `flip_up3` / `flip_down3`, rompé `wall_secret3` hacia `(0,1)`, derrotá al diablillo `imp_santuario` embistiendo con Dash o con pisotón para activar `sw_seal3` (abre `door_wyrm_seal`) y tomar `egg_wyrm_2` (`2/2`).
  3. Subí de vuelta a `(1,0)`, observando en el **mini-mapa** arriba a la derecha cómo se ilumina cada sala visitada; entrá por `door_wyrm_seal` a `(2,0)` y vencé al jefe dragón **`wyrm`** (`boss_dragon`, 4 pisotones sobre su corona dorada, saltando su embestida o atravesando sus bolas de fuego con Dash) para abrir `door_wyrm_exit` y ver la pantalla de victoria final (**`VICTORIA: EL WYRM CAYO`** con tiempo y rango **S/A/B**).
- [ ] **Atajos, puertas de un solo sentido (`one_way_door`) y ascensores (`elevator`) en `world` (`8×5`):**
  - **`owd_wyrm_cat_l` (entre `(3,2)` y `(4,2)`, Las Catacumbas ↔ La Guarida del Wyrm):** desde la derecha (Zona 2, Las Catacumbas) **no se abre** ni al empujar ni al pulsar E/X/↓ (suena clink metálico); al llegar por la izquierda desde Zona 3 (La Guarida del Wyrm) **se abre de forma permanente** dejando libre el atajo.
  - **`owd_wyrm_pozo` (en `(1,0)`, El Pozo):** desde la izquierda (lado inicial de El Pozo) **no se abre**; al subir desde Zona 3 por la derecha **se abre de forma permanente** y conserva su estado abierto tras morir, R corto y al recargar `save.ewsv`.
  - **`sw_elev_cat` → `elev_cat_pozo` (entre `(6,1)` y `(6,0)`):** verificá que `elev_cat_pozo` arranca bloqueado (sellando el piso en `(6,0)`), que al activar `sw_elev_cat` en `(6,1)` se desbloquea, se puede llamar con E/X/↓ desde arriba o abajo y te **sube directo a `(6,0)` en El Pozo** sin aplastarte ni invertir la gravedad.
  - **`elev_wyrm_pozo` (conecta `(1,1) ↔ (1,0)` cerca del inicio):** verificá que arranca desbloqueado, sube desde `(1,1)` hacia la alcoba de `(1,0)` al pisarlo (y responde al llamado con E/X/↓ desde cualquiera de los dos extremos) para abrir `owd_wyrm_pozo` hacia el inicio.
- [ ] **`ability_wall_climb` y el secreto del techo en Zona 1 `(0,0)`:**
  - En Zona 3 `(0,2)` de `world.ewmap`, recogé la reliquia **`rel_wall_climb`** (`ability_wall_climb`): aparece el ícono **esmeralda** en el HUD arriba a la izquierda (junto a Doble Salto y Dash) y en la pantalla de final figura **`ESCALADA`**.
  - Al mantener dirección hacia una pared sólida en el aire, verificá que el personaje **desliza lentamente** sin caer rápido (`36 px/s` máx.) y puede alternar saltos de pared (`wall-jump`) conservando el **Doble Salto** aéreo disponible.
  - Hacé backtracking a **Zona 1 `(0,0)`** (p. ej. vía `elev_wyrm_pozo` → `owd_wyrm_pozo`), escalá la chimenea vertical del techo (`x=128..152`, inaccesible sin trepar) y verificá que arriba podés recoger **`egg_pozo_secreto`** (completando **`6/6`** huevos en `world`) y leer la estela **`lore_techo`**.
  - Probá también la sala aislada `--map assets/maps/bin/wallclimb.ewmap` (`wall_climb_secret`): la chimenea de `136 px` no se sube con Doble Salto solo, pero sí alternando saltos en pared hasta la meta.
- [ ] **Recompensas por huevos y Final alternativo (`world`):**
  - Verificá `1` corazón inicial en el HUD arriba a la izquierda (con `0–1` huevos, cualquier golpe de pinchos o enemigo mata en 1 toque).
  - Al juntar `2` huevos, verificá que aparece el **`2.º` corazón** en el HUD (`+1HP`), que podés absorber 1 golpe de pinchos o enemigo (`imp` / boss) quedando con `1` corazón e invulnerabilidad temporal (`60` frames), y que tocar un checkpoint o hacer respawn recarga ambos corazones.
  - Al juntar `4` huevos (con Dash desbloqueado), verificá el ícono dorado-cian de **Dash II** en el HUD y que podés encadenar **`2` embestidas seguidas en el aire** (`14` frames de duración cada una) antes de volver a apoyar.
  - Al completar `world` con **`6/6` huevos**, verificá la pantalla **`FINAL VERDADERO`** (`NIDADA A SALVO: TODOS LOS HUEVOS`, marco dorado-cian y etiquetas `DASH II` / `+1HP`), distinta de la victoria estándar (`VICTORIA: EL WYRM CAYO`) al terminar con `< 6` huevos.
- [ ] **Zonas nuevas — Archivo Sumergido (`cy=3`) y Torre de los Ecos (`cy=4`):**
  - Desde el **Santuario del Wyrm** `(1,2)` (tras activar el sello y derrotar al diablillo), bajá por el pozo `D1` —una cuerda cuelga del borde— hasta el **Archivo Sumergido** (`aqueduct`, 8 salas con piletas, estelas `aq_lore_archivo` / `aq_lore_presa` y checkpoints `aq_ck_entrada` / `aq_ck_presa`).
  - Recorré el corredor hacia la derecha, nadá las piletas (W1 en `(1,3)` y W2 en `(5,3)`) y bajá por el pozo `D2` en `(7,3)` (otra cuerda) hasta la **Torre de los Ecos** (`tower`, estela `tw_lore_campana` y checkpoint `tw_ck`).
  - Verificá que podés volver a subir trepando las cuerdas de `D2` y `D1` (o con salto en pared) y que el mini-mapa sigue contando el `%` explorado sobre **40 salas**.
  - Verificá el fondo `aqueduct` (bloques húmedos, caños, burbujas) y `tower` (vitrales, campanas, polvo), la oscuridad propia de cada bioma y el cambio de música (acuática en el Archivo, campanario en la Torre).

## 4. Guardar partida

El guardado es automático. Se guarda al tomar un checkpoint, agarrar el huevo, vencer al
boss o cambiar el volumen o los controles. Archivo `save.ewsv` en:
Linux `~/.local/share/echo-well/` (o `$XDG_DATA_HOME/echo-well/`), Windows
`%APPDATA%\EchoWell\`.

- [ ] Primera vez (sin archivo): el juego abre primero **`ELIGE TU HEROE`** (3 personajes), luego arranca jugando y CONTINUAR aparece apagado en el menú.
- [ ] Tomá un checkpoint de una sala avanzada, elegí a `La Zarigüeya` o `El Castor`, salí con SALIR y volvé a abrir: el menú abre
  en **CONTINUAR**; al elegirlo aparecés en ese checkpoint con el mismo personaje guardado en `save.ewsv` v4 (saves v1–v3 siguen cargando).
- [ ] Se restauran: huevo (HUD), puertas abiertas, agua inundada (sin animación), boss
  vencido, gravedad del checkpoint, volumen/mute y controles.
- [ ] Elegí **SEGUIR** con un save existente: arrancás de cero; el save viejo se reemplaza
  recién al tomar un checkpoint o agarrar el huevo (salí antes y CONTINUAR sigue ahí).
- [ ] R largo después de continuar: inicio limpio (`0/1`).
- [ ] Corrupción: editá o truncá `save.ewsv` (o reemplazalo por un archivo de texto) → el
  juego arranca normal sin crashear y avisa "save ignorado" en la consola.
- [ ] Carpeta sin permisos de escritura → el juego anda igual (guarda junto al ejecutable o
  no guarda); no debe crashear.
- [ ] `--nosave`: no lee ni escribe nada.

## 5. Controles configurables

- [ ] Menú → CONTROLES: se listan las 8 acciones con sus teclas y, para salto, acción y cuerda, sus botones de pad.
- [ ] Enter en SALTO → "TECLA O BOTON..." parpadea → apretá `K`: SALTO queda en `K` y salta con K.
- [ ] Asigná a una acción una tecla que ya usa otra (p. ej. ACCION = `K`): se intercambian y
  ninguna queda sin tecla.
- [ ] Esc mientras espera: cancela sin cambiar nada. Esc/Enter no se pueden asignar.
- [ ] Pad: SALTO → apretá `Y`: salta con Y, y la acción que tenía Y recibe el botón anterior.
  Start, Back y el d-pad no se asignan; mover no se remapea en el pad.
- [ ] POR DEFECTO restaura todo. VOLVER (o Esc/B) vuelve al menú principal.
- [ ] Los controles persisten al cerrar y abrir el juego.

## 6. Casos límite

- [ ] Pausar en el aire, en el agua, colgado de la cuerda o arriba de una plataforma móvil, y
  seguir: nada salta ni se teletransporta.
- [ ] Apretar Esc/Start rápido varias veces; abrir el menú justo al morir o al tocar la meta.
- [ ] Mantener R mientras el menú está abierto.
- [ ] Conectar y desconectar el gamepad durante el juego.
- [ ] Minimizar o mover la ventana y volver: el juego no se acelera ni atraviesa paredes.
- [ ] Morir en los bordes entre pantallas (cada 320 px): la cámara se acomoda.
- [ ] Salto en el borde exacto de las plataformas, del agua y de las puertas a medio abrir.
- [ ] Sin dispositivo de audio: el juego anda en silencio.
- [ ] Ventana con `--scale 1` y `--scale 6`.

## 7. Cómo reportar bugs

Abrí un issue en GitHub con la plantilla **Bug** (`.github/ISSUE_TEMPLATE/bug.md`). Incluí:
versión (`--version`), sistema operativo y GPU, sala y checkpoint, pasos exactos para
reproducirlo, qué esperabas y qué pasó, y frecuencia (siempre / a veces / una vez). Sumá una
captura o un video y la salida de la consola. Si el problema es de guardado, adjuntá el
`save.ewsv`. Un bug por issue.
