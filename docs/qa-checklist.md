# Checklist de QA — Echo Well v0.1.0

Versión de prueba para QA de la demo del nivel 1: **6 salas** en el mapa `slice`, de la
salida a la meta. No es un release público. Los paquetes salen como *artifacts* del CI de
GitHub Actions en `main`.

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
`assets/maps/bin/{slice,level2,hello,room,mover,gravity,boss,ability}.ewmap`, `README.md`, `DIST.txt` y
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
| `--map assets/maps/bin/level2.ewmap` | carga directo el Nivel 2 "Las Catacumbas" (también `boss`, `ability`, `gravity`, `mover`, `room`) |
| `--next-map assets/maps/bin/ability.ewmap` | define el mapa que carga **SIGUIENTE NIVEL** al completar el actual |

La consola (terminal en Linux; en Windows abrí `cmd` y corré `echo_engine.exe` desde ahí)
muestra mensajes útiles para los reportes: mapa cargado, huevo agarrado, partida guardada
o cargada, meta.

## 2. Controles (por defecto)

| Acción | Teclado | Gamepad |
|---|---|---|
| Caminar | ← → o A D | stick izquierdo o d-pad |
| Saltar / nadar hacia arriba | Espacio, Z (también ↑ si no estás en la cuerda) | A, RB |
| Activar switch | E, X (también ↓ si no estás en la cuerda) | X, B |
| Agarrar / soltar cuerda | C, Shift izquierdo | LB, Y |
| Trepar (colgado) | ↑ ↓ o W S | stick o d-pad arriba/abajo |
| Volver al último checkpoint | R corto (al soltar) | — (menú → REINICIO) |
| Reiniciar todo al inicio | R mantenido ~0,8 s | — |
| Menú de pausa | Esc | Start (B vuelve atrás) |

El menú siempre se navega con flechas/WASD + Enter/Espacio/Z, o d-pad + A/B.
Menú: SEGUIR, CONTINUAR (si hay partida guardada), REINICIO (vuelve al checkpoint),
VOL (←/→ cambia, Enter silencia), CONTROLES, SALIR.

## 3. Recorrido sala por sala

Para cada sala: llegá, morite a propósito en cada peligro y verificá que reaparezcas en el
checkpoint correcto, con la cámara bien ubicada y sin quedar trabado. El checkpoint se marca
en verde al tomarlo.

- [ ] **Protagonista pixel-art (Explorador del Farol) y objetos.** Verificá animaciones de reposo, caminata (orientada a ←/→), salto/caída, despliegue de alas cian al usar Doble Salto, trepada en cuerda, nado y parpadeo post-respawn; con gravedad invertida el sprite queda invertido verticalmente apoyado en el techo. Los coleccionables se muestran como huevos de dragón escamados y la reliquia como un emblema alado cian.
- [ ] **Diálogos dracónicos y lore de los huevos (`npc`, `lore`, `collectible`).** En la entrada de `slice` (Vigía del Farol) y en `level2` (Cronista de las Catacumbas y Estela del Santuario), al acercarte aparece un indicador dorado y apretar **E**/**X**/**↓** abre la caja de diálogo pixel-art (con `Enter`/`Espacio`/`E` avanza página y `Esc` cierra). Al recoger cada huevo de dragón o la reliquia alada se dispara automáticamente el eco dracónico que revela la historia.
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
  Tocarlo de costado te mata y morir lo cura. Al vencerlo se abre la puerta de la derecha.
- [ ] **Fondos y arte dark fantasy.** Cada pantalla tiene su fondo (`slice` conserva pozo, raíces, roca con
  brasas, cisterna, nichos, engranajes, invertido y arena; `level2` suma `cripta`, `abismo`, `guarida` y `santuario`).
  Los tiles muestran mampostería, ladrillos, madera remachada y musgo renovados, y los pinchos `META 4` se ven como
  púas pixel-art de acero y punta carmesí con alto contraste sobre el fondo.
  Anotá si algún fondo hace difícil ver un pincho, una plataforma o un switch.
- [ ] **Meta y pantalla de final.** Detrás de esa puerta: se abre la pantalla de final pixel-art
  (**NIVEL COMPLETADO**) con el resumen de huevos, muertes y habilidades, y las opciones
  **SIGUIENTE NIVEL** (habilitada en `slice` para pasar a `level2`, o cuando se pasa `--next-map`),
  **REINICIAR NIVEL**, **MENU DE PAUSA** y **SALIR**. Con el boss vivo la meta no cuenta.
  Después de la meta, R corto vuelve al checkpoint y R largo al inicio con el HUD en `0/1`.
- [ ] **Nivel 2 — Las Catacumbas (`level2`, 4×2 salas metroidvania).** Al elegir **SIGUIENTE NIVEL**
  tras completar `slice` (o con `--map assets/maps/bin/level2.ewmap`):
  1. En la entrada `(0,0)`, verificá que el huevo de la repisa alta se ve pero no se alcanza con un salto simple (`0/2` en el HUD).
  2. Avanzá al Hub `(1,0)`, bajá por el pozo vertical hasta caer al agua de `(1,1)` y caminá a la izquierda hasta `(0,1)` para tomar la reliquia cian de **Doble Salto** (aparece el ícono en el HUD).
  3. Volvé a `(1,1)`, subí con Doble Salto al Escalón 1 de la derecha, cruzá `(2,1)` sobre la plataforma móvil `plat_crypt` y en `(3,1)` activá `sw_sanctum` (abre la puerta alta del Hub); luego subí al altar con Doble Salto para agarrar `egg_catacomb_2` (`1/2`) y la reliquia ígnea **`rel_dash`** (`ability_dash`, aparece la flecha ardiente en el HUD junto al doble salto).
  4. Hacé backtracking a `(1,1)`, subí con Doble Salto por los Escalones 1 y 2 de vuelta al Hub `(1,0)` y regresá a `(0,0)` para subir a la repisa inicial y agarrar `egg_catacomb_1` (`2/2` en dorado).
  5. Desde el Hub `(1,0)`, subí con Doble Salto a la Galería Alta, cruzá `door_hub_gallery` hacia `(2,0)` (gravedad invertida por el techo), rompé el muro agrietado **`wall_guardian`** embistiendo con **Dash** (`E`/`X`/`K`/`Shift der.`) al entrar a `(3,0)` y vencé a `boss_catacomb` (3 pisotones) para abrir `door_boss2` y llegar a la meta (`HABILIDADES: DOBLE SALTO + DASH`).

## 4. Guardar partida

El guardado es automático. Se guarda al tomar un checkpoint, agarrar el huevo, vencer al
boss o cambiar el volumen o los controles. Archivo `save.ewsv` en:
Linux `~/.local/share/echo-well/` (o `$XDG_DATA_HOME/echo-well/`), Windows
`%APPDATA%\EchoWell\`.

- [ ] Primera vez (sin archivo): el juego arranca jugando y CONTINUAR aparece apagado en el menú.
- [ ] Tomá un checkpoint de una sala avanzada, salí con SALIR y volvé a abrir: el menú abre
  en **CONTINUAR**; al elegirlo aparecés en ese checkpoint.
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
