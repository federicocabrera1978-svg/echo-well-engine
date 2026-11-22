#pragma once
// Datos de frame y construcción de la escena de mundo sin dependencias de GL/GLFW.
#include "game/guide.hpp"
#include "game/hero.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ew::game {

inline constexpr int kInternalW = 320;
inline constexpr int kInternalH = 180;

inline constexpr int kMaxMarkers = 48;
inline constexpr int kMaxSprites = 16;
inline constexpr int kMaxHazards = 32;
inline constexpr int kMaxBosses = 4;
inline constexpr int kMaxWaterCols = 64;
inline constexpr int kMaxWaterBodies = 4;
inline constexpr int kMaxMovers = 8;
inline constexpr int kTilesW = 120, kTilesH = 69;  // 3×3 chunks de 40×23

// Lo que el juego le pasa al renderer cada frame (coords de mundo en px, y hacia abajo).
struct FrameData {
  float cam_x = 0, cam_y = 0;
  float chunk_x_px = 0, chunk_y_px = 0;      // origen del vecindario 3×3 (chunk activo - 1)
  const uint8_t* tiles = nullptr;            // kTilesW×kTilesH RGB8 (R=COL, G=VIS, B=FG), 3×3 chunks
  uint32_t tiles_version = 0;                // re-sube la textura si cambia
  // Marcadores: (x, y, radio, forma 0=círculo 1=cuadrado) + color RGB.
  int marker_count = 0;
  std::array<float, kMaxMarkers * 4> markers{};
  std::array<float, kMaxMarkers * 3> marker_colors{};
  // Sprites pixel-art (16×16 px): (x, y, col_signed, row_signed) + tinte (r, g, b, emit_boost).
  // col_signed >= 0 → col 0..15 normal; col_signed < 0 → espejo horizontal de col = -col_signed - 1.
  // row_signed >= 0 → row 0..7 normal; row_signed < 0 → espejo vertical de row = -row_signed - 1.
  int sprite_count = 0;
  std::array<float, kMaxSprites * 4> sprites{};
  std::array<float, kMaxSprites * 4> sprite_tint{};
  int player_sprite_idx = -1;                // índice en `sprites` del héroe principal (-1 si no se dibujó)
  float player_squash_x = 1.f, player_squash_y = 1.f;
  // Agua: hasta kMaxWaterBodies cuerpos. Por cuerpo: rect (x, y_reposo, w, fondo),
  // n° de columnas y altura de superficie por columna (px mundo), stride kMaxWaterCols.
  int water_count = 0;
  std::array<int, kMaxWaterBodies> water_cols{};
  std::array<float, kMaxWaterBodies * 4> water_rect{};
  std::array<float, kMaxWaterBodies * kMaxWaterCols> water_surface{};
  float light_x = 160, light_y = 90;         // luz principal (px mundo)
  float fade = 0;                            // 0..1 pantalla de meta (light pass)
  int end_screen = 0;                        // 1 = pantalla de final interactiva (usa ctl_txt)
  int end_sel = 0;                           // opción seleccionada 0..3
  int end_has_next = 0;                      // 1 = hay siguiente nivel disponible
  int end_true = 0;                          // 1 = final verdadero (100% huevos al terminar campaña)
  int pause = 0;                             // 1 = menú de pausa (light pass)
  int pause_sel = 0;
  float pause_vol = 0.8f;
  int pause_mute = 0;
  int pause_has_save = 0;                    // CONTINUAR habilitado
  int dlg = 0;                               // 1 = caja de diálogo abierta (usa ctl_txt)
  int dlg_more = 0;                          // 1 = hay más páginas en el diálogo
  int ctl = 0;                               // página CONTROLES
  int ctl_sel = 0;                           // fila de u_txt resaltada
  int ctl_wait = 0;                          // esperando tecla/botón
  int hero_select = 0;                       // 1 = selector de héroe abierto
  int hero_cursor = 0;                       // opción seleccionada 0..2
  std::array<int32_t, 96> ctl_txt{};         // game::pack_text(controls_text(menu))
  // HUD de collectibles (light pass): ícono de huevo + "n/total" arriba a la izquierda.
  // total == 0 → no se dibuja. flash 0..1 = destello al agarrar uno.
  int items = 0, items_total = 0;
  float items_flash = 0;
  // HUD de habilidades (light pass): bitmask kAbility* + destello 0..1 al desbloquear.
  int abilities = 0;
  float ability_flash = 0;
  int hp = 1, max_hp = 1;                    // vida actual / máxima (2 con 2+ huevos)
  int dash_upgraded = 0;                     // 1 = Dash II desbloqueado (4+ huevos)
  // Mini-mapa exploratorio (light pass): grilla de chunks, chunk activo, bitmasks de
  // visitados, puertas/muros cerrados, huevos pendientes y porcentaje explorado.
  int map_w = 0, map_h = 0, cur_cx = 0, cur_cy = 0;
  uint32_t visited_mask = 0;
  uint32_t visited_mask_hi = 0;
  uint32_t blocked_doors_lo = 0, blocked_doors_hi = 0;
  uint32_t missing_eggs_lo = 0, missing_eggs_hi = 0;
  int explored_pct = 0;
  // Rayos catódicos al vencer un boss (light pass): origen en px de vista + progreso t (1=inicio -> 0=fin).
  float boss_burst_x = 0, boss_burst_y = 0, boss_burst_t = 0;
  float boss_flash = 0;
  // Plataformas móviles: rect (x, y, w, h) en px mundo + familia de atlas (1..15).
  int mover_count = 0;
  std::array<float, kMaxMovers * 4> mover_rect{};
  std::array<float, kMaxMovers> mover_fam{};
  // Pinchos pixel-art (META 4): tramos horizontales (x0, y0, w_px, dir: +1 piso, -1 techo).
  int hazard_count = 0;
  std::array<float, kMaxHazards * 4> hazards{};
  // Bosses pixel-art: rect (x_center, y_feet, w, h) + estado (kind, dir, phase, hits).
  int boss_count = 0;
  std::array<float, kMaxBosses * 4> boss_rect{};
  std::array<float, kMaxBosses * 4> boss_state{};
  // Fondos (backdrops) del vecindario 3×3, [oy*3+ox] desde chunk_x_px/chunk_y_px:
  // tema (ew::map::BackdropTheme) y seed. Default: cueva.
  std::array<int, 9> bg_theme{};
  std::array<int, 9> bg_seed{};
  // Brisa global y ráfagas locales sobre vegetación.
  int foliage_enabled = 1;
  float wind_gust = 1.f;
  int wind_impulse_count = 0;
  std::array<float, 16> wind_impulses{};
};

inline void push_marker(FrameData& v, float x, float y, float r, int shape, float cr, float cg, float cb) {
  if (v.marker_count >= kMaxMarkers) return;
  // Fuera de la vista no gasta slots (hay kMaxMarkers; con 2 cuerdas no alcanzan para todo).
  constexpr float m = 16.f;
  if (x + r < v.cam_x - m || x - r > v.cam_x + float(kInternalW) + m ||
      y + r < v.cam_y - m || y - r > v.cam_y + float(kInternalH) + m)
    return;
  const int i = v.marker_count++;
  v.markers[std::size_t(i * 4 + 0)] = x;
  v.markers[std::size_t(i * 4 + 1)] = y;
  v.markers[std::size_t(i * 4 + 2)] = r;
  v.markers[std::size_t(i * 4 + 3)] = float(shape);
  v.marker_colors[std::size_t(i * 3 + 0)] = cr;
  v.marker_colors[std::size_t(i * 3 + 1)] = cg;
  v.marker_colors[std::size_t(i * 3 + 2)] = cb;
}

inline void push_sprite(FrameData& v, float x, float y, int col, int row, bool flip_x = false, bool flip_y = false,
                        float tr = 1.f, float tg = 1.f, float tb = 1.f, float emit_boost = 0.f) {
  if (v.sprite_count >= kMaxSprites) return;
  constexpr float m = 16.f;
  if (x + 8.f < v.cam_x - m || x - 8.f > v.cam_x + float(kInternalW) + m ||
      y + 8.f < v.cam_y - m || y - 8.f > v.cam_y + float(kInternalH) + m)
    return;
  const int i = v.sprite_count++;
  const int c = std::clamp(col, 0, 15);
  const int r = std::clamp(row, 0, 7);
  v.sprites[std::size_t(i * 4 + 0)] = x;
  v.sprites[std::size_t(i * 4 + 1)] = y;
  v.sprites[std::size_t(i * 4 + 2)] = flip_x ? float(-c - 1) : float(c);
  v.sprites[std::size_t(i * 4 + 3)] = flip_y ? float(-r - 1) : float(r);
  v.sprite_tint[std::size_t(i * 4 + 0)] = tr;
  v.sprite_tint[std::size_t(i * 4 + 1)] = tg;
  v.sprite_tint[std::size_t(i * 4 + 2)] = tb;
  v.sprite_tint[std::size_t(i * 4 + 3)] = emit_boost;
}

inline void push_movers(FrameData& v, const Sim& sim, const Scene* scene = nullptr) {
  constexpr float m = 16.f;
  const float x0 = v.cam_x - m, x1 = v.cam_x + float(kInternalW) + m;
  const float y0 = v.cam_y - m, y1 = v.cam_y + float(kInternalH) + m;
  for (int i = 0; i < sim.mover_count() && v.mover_count < kMaxMovers; ++i) {
    const auto& mv = sim.mover(i);
    if (mv.is_elevator) {
      const bool unlocked = scene && mv.ent < scene->entities().size() && scene->entities()[mv.ent].active;
      const float cr = unlocked ? 0.22f : 0.95f;
      const float cg = unlocked ? 0.92f : 0.58f;
      const float cb = unlocked ? 0.72f : 0.14f;
      push_marker(v, mv.wp0_x - 2.f, mv.wp0_y + 2.f, 1.5f, 1, cr, cg, cb);
      push_marker(v, mv.wp0_x + mv.w + 2.f, mv.wp0_y + 2.f, 1.5f, 1, cr, cg, cb);
      push_marker(v, mv.wp1_x - 2.f, mv.wp1_y + 2.f, 1.5f, 1, cr, cg, cb);
      push_marker(v, mv.wp1_x + mv.w + 2.f, mv.wp1_y + 2.f, 1.5f, 1, cr, cg, cb);
    }
    if (mv.x + mv.w < x0 || mv.x > x1 || mv.y + mv.h < y0 || mv.y > y1) continue;
    const int k = v.mover_count++;
    v.mover_rect[std::size_t(k * 4 + 0)] = mv.x;
    v.mover_rect[std::size_t(k * 4 + 1)] = mv.y;
    v.mover_rect[std::size_t(k * 4 + 2)] = mv.w;
    v.mover_rect[std::size_t(k * 4 + 3)] = mv.h;
    v.mover_fam[std::size_t(k)] = float((mv.tile % 15) + 1);
  }
}

inline void push_entities(FrameData& v, const Scene& s, float time_s) {
  for (const auto& e : s.entities()) {
    if (e.type == "switch" || e.type == "flood_switch") {  // abre-puertas pixel-art elevado (shape 3)
      push_marker(v, std::round(e.x), std::round(e.y), 10.f, 3,
                  e.active ? 0.25f : 1.0f, e.active ? 1.0f : 0.55f, e.active ? 0.50f : 0.12f);
      if (!e.active) {  // flecha pulsante sobre el abre-puertas aún sin accionar
        const float bob = std::round(std::sin(time_s * 6.5f + e.x * 0.1f) * 2.f);
        push_marker(v, std::round(e.x), std::round(e.y - 17.f + bob), 6.f, 4, 0.f, 1.f, 0.f);
      }
    } else if (e.type == "door") {  // se desliza hacia arriba al abrir (anim 0..1)
      const int h = kDoorHTiles;
      const float rise = e.anim * float(h) * 8.f;
      if (e.anim < 0.99f) {
        const float vis = float(h) * (1.f - e.anim);
        const int n = std::max(1, int(std::ceil(vis / 8.f)));
        const float fade = 1.f - e.anim * 0.35f;
        for (int k = 0; k < n; ++k)
          push_marker(v, e.x + 4, e.y + 4 + 8.f * float(k) - rise, 4, 1, 0.6f * fade, 0.3f * fade, 0.85f * fade);
      }
      if (e.anim > 0.05f) push_marker(v, e.x + 4, e.y + 1, 1, 1, 0.3f, 0.2f, 0.4f);
    } else if (e.type == "one_way_door") {  // compuerta de atajo unidireccional (1×3 tiles, desliza al abrir)
      const float rise = e.anim * float(kDoorHTiles * ew::map::kTileSize);
      if (e.anim < 0.99f) {
        const float fade = 1.f - e.anim * 0.40f;
        for (int k = 0; k < kDoorHTiles; ++k) {
          const float sy = e.y + 4.f + 8.f * float(k) - rise;
          if (sy + 4.f <= e.y) continue;
          push_marker(v, e.x + 4.f, sy, 3.6f, 1, 0.34f * fade, 0.42f * fade, 0.54f * fade);
          push_marker(v, e.x + 4.f, sy, 1.4f, 1, 0.62f * fade, 0.74f * fade, 0.88f * fade);
        }
        const float side = one_way_unlocks_from_left(e) ? -1.f : 1.f;
        const float ly = e.y + 12.f - rise;
        if (ly + 2.f > e.y)
          push_marker(v, e.x + 4.f + side * 3.f, ly, 1.8f, 0, 0.25f * fade, 0.95f * fade, 0.72f * fade);
      }
      if (e.anim > 0.05f) push_marker(v, e.x + 4.f, e.y + 1.f, 1.2f, 1, 0.25f, 0.75f, 0.55f);
    } else if (e.type == "player_spawn") {
      push_marker(v, e.x, e.y + 7, 1, 1, 0.3f, 0.9f, 0.4f);
    } else if (e.type == "goal") {  // pedestal + orbe dorado (más brillante al completar)
      push_marker(v, e.x, e.y + 6, 4, 1, 0.45f, 0.35f, 0.15f);
      push_marker(v, e.x, e.y, 5, 0, 1.0f, 0.85f, 0.25f);
    } else if (e.type == "collectible") {  // huevo de dragón con escamas; agarrado = no se dibuja
      if (!e.active)
        push_sprite(v, std::round(e.x), std::round(e.y - 1.f), int(time_s * 5.f) & 3, 6, false, false, 1.f, 1.f, 1.f,
                    0.08f);
    } else if (e.type == "ability_double_jump") {  // reliquia alada cian de doble salto; al recogerla desaparece
      if (!e.active) {
        const float bob = std::round(std::sin(time_s * 3.5f) * 1.5f);
        push_sprite(v, std::round(e.x), std::round(e.y + bob), 4 + (int(time_s * 6.f) & 3), 6, false, false, 1.f, 1.f,
                    1.f, 0.15f);
      }
    } else if (e.type == "ability_dash") {  // reliquia ígnea dorado-carmesí de Embestida (Dash)
      if (!e.active) {
        const float bob = std::round(std::sin(time_s * 4.2f) * 1.5f);
        push_sprite(v, std::round(e.x), std::round(e.y + bob), 4 + (int(time_s * 7.f) & 3), 6, false, false,
                    1.45f, 0.58f, 0.25f, 0.24f);
      }
    } else if (e.type == "ability_wall_climb") {  // reliquia esmeralda de Trepar y Salto en Pared
      if (!e.active) {
        const float bob = std::round(std::sin(time_s * 3.8f) * 1.5f);
        push_sprite(v, std::round(e.x), std::round(e.y + bob), 4 + (int(time_s * 6.5f) & 3), 6, false, false,
                    0.35f, 1.45f, 0.65f, 0.24f);
      }
    } else if (e.type == "break_wall") {  // muro agrietado rompible con Dash (1×3 tiles)
      if (!e.active) {
        const float pulse = 0.78f + 0.22f * std::sin(time_s * 4.0f);
        for (int k = 0; k < kDoorHTiles; ++k) {
          const float by = e.y + 4.f + 8.f * float(k);
          push_marker(v, e.x + 4.f, by, 4, 1, 0.30f, 0.26f, 0.34f);
          push_marker(v, e.x + 4.f + (k == 1 ? 1.f : -1.f), by, 1.4f, 0, 0.96f * pulse, 0.45f * pulse, 0.15f * pulse);
        }
      }
    } else if (e.type == "gravity_flip") {  // orbe violeta; el punto marca hacia dónde cae el player
      const float d = e.active ? -1.f : 1.f;
      push_marker(v, e.x, e.y, 5, 0, 0.6f, 0.35f, 1.0f);
      push_marker(v, e.x, e.y + 3.f * d, 2, 0, 0.95f, 0.9f, 1.0f);
    } else if (e.type == "npc") {  // Vigía / Cronista del Farol (fila 7, cols 0..3)
      push_sprite(v, std::round(e.x), std::round(e.y - 2.f), int(time_s * 4.f) & 3, 7, false, false, 1.f, 1.f, 1.f,
                  0.06f);
    } else if (e.type == "lore") {  // Estela dracónica con runas incandescentes (fila 7, cols 4..7)
      push_sprite(v, std::round(e.x), std::round(e.y - 2.f), 4 + (int(time_s * 4.f) & 3), 7, false, false, 1.f, 1.f,
                  1.f, 0.12f);
    } else if (e.type == "checkpoint") {  // santuario de checkpoint pixel-art (shape 2)
      push_marker(v, std::round(e.x), std::round(e.y), 10.f, 2,
                  e.active ? 0.30f : 0.25f, e.active ? 1.0f : 0.55f, e.active ? 0.55f : 0.70f);
    }
  }
}

namespace detail {
inline void push_hazard_span(FrameData& v, float x0, float y0, float w, float dir) {
  if (v.hazard_count >= kMaxHazards || w <= 0.f) return;
  const int i = v.hazard_count++;
  v.hazards[std::size_t(i * 4 + 0)] = x0;
  v.hazards[std::size_t(i * 4 + 1)] = y0;
  v.hazards[std::size_t(i * 4 + 2)] = w;
  v.hazards[std::size_t(i * 4 + 3)] = dir;
}
}  // namespace detail

// Pinchos (META 4) visibles alrededor de la cámara como tramos horizontales pixel-art.
inline void push_hazards(FrameData& v, Scene& s) {
  const float ts = float(ew::map::kTileSize);
  const int x0 = int(std::floor(s.cam_x() / ts)), y0 = int(std::floor(s.cam_y() / ts));
  const int x1 = x0 + kViewW / ew::map::kTileSize + 2;
  const int y1 = y0 + kViewH / ew::map::kTileSize + 2;
  auto is_haz = [&](int tx, int ty) { return s.tile(tx, ty, ew::map::Layer::META) == Sim::kMetaHazard; };
  for (int ty = y0; ty <= y1; ++ty) {
    int run_dir = 0, run_start = 0;
    auto flush = [&](int tx_end) {
      if (run_dir != 0)
        detail::push_hazard_span(v, float(run_start) * ts, float(ty) * ts, float(tx_end - run_start) * ts,
                                 float(run_dir));
      run_dir = 0;
    };
    for (int tx = x0; tx <= x1; ++tx) {
      int dir = 0;
      if (is_haz(tx, ty)) {
        const bool up_haz = is_haz(tx, ty - 1);
        const bool dn_haz = is_haz(tx, ty + 1);
        const bool dn_sol = s.solid_tile(tx, ty + 1);
        const bool up_sol = s.solid_tile(tx, ty - 1);
        if (!dn_haz && !dn_sol && (up_sol || up_haz)) dir = -1;
        else if (!up_haz && !up_sol) dir = 1;
      }
      if (dir != run_dir) {
        flush(tx);
        if (dir != 0) { run_dir = dir; run_start = tx; }
      }
    }
    flush(x1 + 1);
  }
}

// Empaqueta cámara, vecindario 3×3, entidades, física, bosses, protagonista, agua, luz y viento.
inline void build_world_frame(FrameData& v, Scene& scene, const Sim& sim, float time_s, bool demo,
                              bool overlay_open, bool show_player_guide = false, int preview_hero_sel = -1) {
  v.marker_count = 0;
  v.sprite_count = 0;
  v.player_sprite_idx = -1;
  v.player_squash_x = 1.f;
  v.player_squash_y = 1.f;
  v.water_count = 0;
  v.mover_count = 0;
  v.hazard_count = 0;
  v.boss_count = 0;
  v.boss_burst_x = 0;
  v.boss_burst_y = 0;
  v.boss_burst_t = 0;
  v.boss_flash = 0;

  float shake_x = 0.f, shake_y = 0.f;
  sim.victory_seq().camera_shake(shake_x, shake_y);
  v.cam_x = std::round(scene.cam_x() + shake_x);
  v.cam_y = std::round(scene.cam_y() + shake_y);  // pixel-perfect
  v.chunk_x_px = scene.nb_origin_x();             // vecindario 3×3 alrededor del chunk activo
  v.chunk_y_px = scene.nb_origin_y();
  v.tiles = scene.active_tiles().data();
  v.tiles_version = scene.chunk_version();
  for (std::size_t i = 0; i < 9; ++i) {           // fondos del vecindario 3×3 (sección BACKDROP)
    v.bg_theme[i] = scene.active_backdrops()[i].theme;
    v.bg_seed[i] = scene.active_backdrops()[i].seed;
  }

  push_entities(v, scene, time_s);
  if (show_player_guide && !demo) {
    if (const auto ga = compute_guide_arrow(scene, sim, time_s); ga.valid)
      push_marker(v, ga.x, ga.y, 6.f, 4, ga.ux, ga.uy, float(ga.style));
  }
  if (const int nd = sim.nearby_interact_dialog();
      nd >= 0 && !overlay_open && std::size_t(nd) < scene.dialogs().size()) {
    const auto& de = scene.entities()[scene.dialogs()[std::size_t(nd)].entity];
    push_marker(v, de.x, de.y - 12.f + std::round(std::sin(time_s * 5.f)), 1.5f, 0, 1.0f, 0.88f, 0.35f);
  }
  push_movers(v, sim, &scene);

  for (int bi = 0; bi < sim.boss_count() && v.boss_count < kMaxBosses; ++bi) {
    const auto& b = sim.boss(bi);
    if (b.defeated && sim.boss_burst() <= 0) continue;
    if (b.defeated && (sim.boss_burst() % 2 == 0)) continue;  // se desintegra en scanlines CRT
    if (!b.defeated && b.hurt > 0.f && int(b.hurt * 20.f) % 2 == 0) continue;  // parpadea herido
    const float crt_jitter = b.defeated ? std::round(std::sin(float(sim.boss_burst()) * 2.1f) * 3.f) : 0.f;
    const int k = v.boss_count++;
    v.boss_rect[std::size_t(k * 4 + 0)] = std::round(b.x) + crt_jitter;
    v.boss_rect[std::size_t(k * 4 + 1)] = std::round(b.y);
    v.boss_rect[std::size_t(k * 4 + 2)] = b.width();
    v.boss_rect[std::size_t(k * 4 + 3)] = b.height();
    v.boss_state[std::size_t(k * 4 + 0)] = b.dragon ? 2.f : (b.guardian2 ? 1.f : 0.f);
    v.boss_state[std::size_t(k * 4 + 1)] = float(b.dir);
    v.boss_state[std::size_t(k * 4 + 2)] = b.defeated ? 1.f : float(b.phase);
    v.boss_state[std::size_t(k * 4 + 3)] = float(b.hits);
  }
  const auto& vic = sim.victory_seq();
  if (vic.in_defeat_anim()) {
    v.boss_burst_x = vic.origin_x() - v.cam_x;
    v.boss_burst_y = vic.origin_y() - v.cam_y;
    v.boss_burst_t = vic.crt_progress();
  }
  v.boss_flash = vic.flash_alpha();
  for (const auto& pt : vic.particles()) {
    if (v.marker_count >= kMaxMarkers) break;
    if (pt.life <= 0.f) continue;
    const float f = std::clamp(pt.life / std::max(pt.max_life, 1e-4f), 0.f, 1.f);
    push_marker(v, pt.x, pt.y, pt.radius * (0.4f + 0.6f * f), 1, pt.r * f, pt.g * f, pt.b * f);
  }
  for (const auto& fb : sim.fireballs()) {
    push_marker(v, fb.x, fb.y, Sim::kFireballRadius + 1.f, 0, 0.96f, 0.34f, 0.08f);
    push_marker(v, fb.x, fb.y, 1.8f, 0, 1.0f, 0.92f, 0.45f);
  }
  for (int ii = 0; ii < sim.imp_count(); ++ii) {  // diablillos menores (caen con pisotón o Dash)
    const auto& im = sim.imp(ii);
    if (im.defeated) continue;
    const float bob = std::sin(time_s * 8.f + float(ii)) * 0.6f;
    push_marker(v, im.x, im.y - 5.f + bob, 4.5f, 1, 0.32f, 0.14f, 0.24f);
    push_marker(v, im.x - 2.f, im.y - 9.5f + bob, 1.2f, 1, 0.55f, 0.28f, 0.32f);
    push_marker(v, im.x + 2.f, im.y - 9.5f + bob, 1.2f, 1, 0.55f, 0.28f, 0.32f);
    push_marker(v, im.x + float(im.dir) * 2.f, im.y - 5.5f + bob, 1.2f, 0, 1.0f, 0.42f, 0.15f);
  }
  push_hazards(v, scene);
  if (sim.finished())
    if (const auto* g = scene.find_type("goal"))
      push_marker(v, g->x, g->y, 8, 0, 1.0f, 1.0f, 0.55f);

  const auto& pts = sim.world().points();
  for (int r = 0; r < sim.rope_count(); ++r)  // cuerdas Verlet (sección ROPE)
    for (int i = 0; i < sim.rope_points(r); ++i) {
      const auto& rp = pts[sim.rope_first(r) + uint32_t(i)];
      const bool held = sim.on_rope() && sim.held_rope() == r && i == sim.rope_node();
      const float rad = i == 0 ? 2.f : (held ? 2.5f : rp.radius);
      const float cr = held ? 1.0f : 0.75f, cg = held ? 0.85f : 0.6f, cb = held ? 0.4f : 0.4f;
      push_marker(v, rp.pos.x, rp.pos.y, rad, i == 0 ? 1 : 0, cr, cg, cb);
    }
  if (sim.grab_flash() > 0 && sim.on_rope()) {  // anillo que se expande al agarrar
    const auto& node = pts[sim.rope_first(sim.held_rope()) + uint32_t(sim.rope_node())];
    const float f = float(sim.grab_flash()) / float(Sim::kGrabFlashFrames);
    push_marker(v, node.pos.x, node.pos.y, 3.f + (1.f - f) * 10.f, 0, 1.0f, 0.9f * f, 0.35f * f);
  }
  if (sim.double_jump_flash() > 0) {  // aro cian que se expande al activar doble salto en el aire
    const float f = float(sim.double_jump_flash()) / float(Sim::kDoubleJumpFlashFrames);
    push_marker(v, sim.double_jump_x(), sim.double_jump_y(), 2.5f + (1.f - f) * 7.f, 0,
                0.45f * f, 0.92f * f, 1.0f * f);
  }
  if (sim.wall_break_flash() > 0) {  // fragmentos que saltan al destruir un break_wall con Dash
    const float f = float(sim.wall_break_flash()) / float(Sim::kWallBreakFlashFrames);
    const float spread = (1.f - f) * 14.f;
    for (int k = 0; k < 4; ++k) {
      const float sx = (k & 1) ? 1.f : -1.f, sy = (k & 2) ? 1.f : -1.f;
      push_marker(v, sim.wall_break_x() + sx * spread, sim.wall_break_y() + sy * (spread * 0.7f - 2.f),
                  2.f * f, 1, 0.95f * f, 0.55f * f, 0.25f * f);
    }
  }

  const auto& pl = sim.player();
  const float px = pl.pos.x, py = pl.pos.y;
  if (sim.wall_sliding() && sim.wall_dir() != 0) {  // chispa esmeralda contra la pared al deslizar
    const float wx = px + float(sim.wall_dir()) * (Sim::kPlayerRadius + 0.5f);
    const float sy = py - sim.down() * 2.f + std::sin(time_s * 24.f) * 1.5f;
    push_marker(v, wx, sy, 1.6f, 0, 0.25f, 1.0f, 0.55f);
    push_marker(v, wx, sy + sim.down() * 2.5f, 1.1f, 1, 0.65f, 1.0f, 0.80f);
  }
  for (const auto& pt : sim.dust()) {  // partículas de polvo/estela (despegue, aterrizaje, salto, dash)
    if (v.marker_count >= kMaxMarkers) break;
    if (pt.life <= 0.f) continue;
    const float f = std::clamp(pt.life / std::max(pt.max_life, 1e-4f), 0.f, 1.f);
    push_marker(v, pt.x, pt.y, pt.radius * (0.4f + 0.6f * f), 0, pt.r * f, pt.g * f, pt.b * f);
  }
  {  // protagonista pixel-art 16×16 (según héroe): espejado según dirección y gravedad
    const float d = sim.down();
    const bool inv = sim.gravity_flipped();
    const bool dashing = sim.dash_ticks() > 0;
    const int h_col = hero_sprite_col_offset(sim.hero());
    if (dashing) {  // estela ígnea detrás del protagonista al embestir
      const float dir = float(sim.dash_dir());
      push_sprite(v, std::round(px - dir * 8.f), std::round(py - 2.f * d), h_col + 1, 2,
                  sim.player_facing() < 0, inv, 0.70f, 0.34f, 0.16f, 0.08f);
      push_sprite(v, std::round(px - dir * 4.f), std::round(py - 2.f * d), h_col + 2, 2,
                  sim.player_facing() < 0, inv, 1.05f, 0.52f, 0.22f, 0.18f);
    } else if (sim.dash_flash() > 0) {  // estela residual al salir del Dash
      const float dir = float(sim.dash_dir());
      const float f = float(sim.dash_flash()) / float(Sim::kDashFlashFrames);
      const bool up = sim.dash_upgraded();
      push_sprite(v, std::round(px - dir * 5.f), std::round(py - 2.f * d), h_col + 2, 2,
                  sim.player_facing() < 0, inv,
                  (up ? 0.50f : 0.90f) * f, (up ? 0.92f : 0.48f) * f, (up ? 1.0f : 0.22f) * f, 0.12f * f);
    }
    const bool blink = sim.invuln() > 0 && (sim.invuln() / 2) % 2 == 1;
    const float tr = blink ? 0.55f : (dashing ? 1.25f : (inv ? 0.90f : 1.0f));
    const float tg = blink ? 0.55f : (dashing ? 0.90f : (inv ? 0.85f : 1.0f));
    const float tb = blink ? 0.70f : (dashing ? 0.55f : (inv ? 1.08f : 1.0f));
    const float boost = dashing ? 0.38f : (sim.double_jump_flash() > 0 ? 0.25f : 0.05f);
    v.player_sprite_idx = v.sprite_count;
    push_sprite(v, std::round(px), std::round(py - 2.f * d), h_col + sim.player_anim_frame(), int(sim.player_anim()),
                sim.player_facing() < 0, inv, tr, tg, tb, boost);
    if (v.sprite_count == v.player_sprite_idx) {
      v.player_sprite_idx = -1;
      v.player_squash_x = 1.f;
      v.player_squash_y = 1.f;
    } else {
      v.player_squash_x = sim.squash_x();
      v.player_squash_y = sim.squash_y();
    }
  }
  if (preview_hero_sel >= 0) {
    const int frame = int(time_s * 4.f) & 3;
    for (int h = 0; h < kHeroCount; ++h) {
      const bool sel = (h == preview_hero_sel);
      const float tr = sel ? 1.08f : 0.72f;
      const float tg = sel ? 1.04f : 0.72f;
      const float tb = sel ? 0.96f : 0.78f;
      const float boost = sel ? 0.25f : 0.06f;
      push_sprite(v, std::round(v.cam_x + 70.f), std::round(v.cam_y + 62.f + float(h) * 26.f),
                  h * 4 + frame, int(PlayerAnim::Idle), false, false, tr, tg, tb, boost);
    }
  }
  for (const auto& w : sim.water()) {  // hasta kMaxWaterBodies; solo los que tocan la vista
    if (v.water_count >= kMaxWaterBodies) break;
    if (w.x() + w.width() < v.cam_x || w.x() > v.cam_x + kInternalW ||
        w.bottom() < v.cam_y || w.rest_y() - 32 > v.cam_y + kInternalH)
      continue;
    const int b = v.water_count++;
    const int n = std::min(w.cols(), kMaxWaterCols);
    v.water_cols[std::size_t(b)] = n;
    for (int k = 0; k < 4; ++k)
      v.water_rect[std::size_t(b * 4 + k)] = std::array<float, 4>{w.x(), w.rest_y(), w.width(), w.bottom()}[std::size_t(k)];
    for (int i = 0; i < n; ++i)
      v.water_surface[std::size_t(b * kMaxWaterCols + i)] = w.surface_col(i);
  }
  v.light_x = px;
  v.light_y = py;
  v.fade = sim.finish_t();
  v.foliage_enabled = sim.wind_enabled() ? 1 : 0;
  v.wind_gust = sim.wind().current_gust(uint32_t(v.bg_seed[4]));
  v.wind_impulse_count = sim.wind().pack_impulses(v.wind_impulses);
}

}  // namespace ew::game
