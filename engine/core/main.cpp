// echo_engine: ventana + loop + Deferred 320x180 sobre un mapa .ewmap.
//   echo_engine [--scale N] [--frames N] [--shaders DIR] [--map FILE] [--atlas FILE.png] [--sprites FILE.png] [--dump FILE.ppm] [--demo] [--start X Y] [--nosound] [--nogamepad] [--nosave] [--version]
// Controles: ←/→ (o A/D) caminar, Espacio/↑/Z saltar, E/X/↓ activar switch, C/Shift agarrar/soltar cuerda (colgado: ←/→ balancear, ↑/↓ o W/S trepar, Espacio/Z saltar), R corto = checkpoint, R largo = inicio, ESC/Start = menú de pausa (seguir, continuar partida, reiniciar, volumen, controles, salir). Las teclas y botones de juego se remapean en CONTROLES (game/bindings.hpp) y se guardan con la partida. Autoguardado en el directorio de datos del usuario (ver game/save.hpp). --demo = input automático.
// Mapa por defecto: slice.ewmap (camino start→goal). hello.ewmap sigue disponible con --map.
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "game/campaign.hpp"
#include "game/dialogue.hpp"
#include "game/end_screen.hpp"
#include "game/sim.hpp"
#include "core/paths.hpp"
#include "game/gamepad.hpp"
#include "game/menu.hpp"
#include "game/menu_text.hpp"
#include "game/save.hpp"
#include "audio/device.hpp"
#include "audio/sfx.hpp"
#include "render/deferred.hpp"

#ifndef EW_SHADER_DIR
#define EW_SHADER_DIR "assets/shaders"
#endif
#ifndef EW_DEFAULT_MAP
#define EW_DEFAULT_MAP "assets/maps/bin/hello.ewmap"
#endif
#ifndef EW_DEFAULT_ATLAS
#define EW_DEFAULT_ATLAS "assets/atlas/tiles.png"
#endif
#ifndef EW_DEFAULT_SPRITES
#define EW_DEFAULT_SPRITES "assets/atlas/sprites.png"
#endif

namespace {
using ew::render::FrameView;

void push_marker(FrameView& v, float x, float y, float r, int shape, float cr, float cg, float cb) {
  if (v.marker_count >= ew::render::kMaxMarkers) return;
  // Fuera de la vista no gasta slots (hay kMaxMarkers; con 2 cuerdas no alcanzan para todo).
  constexpr float m = 16.f;
  if (x + r < v.cam_x - m || x - r > v.cam_x + float(ew::render::kInternalW) + m ||
      y + r < v.cam_y - m || y - r > v.cam_y + float(ew::render::kInternalH) + m) return;
  const int i = v.marker_count++;
  v.markers[i * 4 + 0] = x; v.markers[i * 4 + 1] = y; v.markers[i * 4 + 2] = r; v.markers[i * 4 + 3] = float(shape);
  v.marker_colors[i * 3 + 0] = cr; v.marker_colors[i * 3 + 1] = cg; v.marker_colors[i * 3 + 2] = cb;
}

void push_sprite(FrameView& v, float x, float y, int col, int row, bool flip_x = false, bool flip_y = false,
                 float tr = 1.f, float tg = 1.f, float tb = 1.f, float emit_boost = 0.f) {
  if (v.sprite_count >= ew::render::kMaxSprites) return;
  constexpr float m = 16.f;
  if (x + 8.f < v.cam_x - m || x - 8.f > v.cam_x + float(ew::render::kInternalW) + m ||
      y + 8.f < v.cam_y - m || y - 8.f > v.cam_y + float(ew::render::kInternalH) + m)
    return;
  const int i = v.sprite_count++;
  const int c = std::clamp(col, 0, 7);
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

void push_movers(FrameView& v, const ew::game::Sim& sim) {
  constexpr float m = 16.f;
  const float x0 = v.cam_x - m, x1 = v.cam_x + float(ew::render::kInternalW) + m;
  const float y0 = v.cam_y - m, y1 = v.cam_y + float(ew::render::kInternalH) + m;
  for (int i = 0; i < sim.mover_count() && v.mover_count < ew::render::kMaxMovers; ++i) {
    const auto& mv = sim.mover(i);
    if (mv.x + mv.w < x0 || mv.x > x1 || mv.y + mv.h < y0 || mv.y > y1) continue;
    const int k = v.mover_count++;
    v.mover_rect[k * 4 + 0] = mv.x;
    v.mover_rect[k * 4 + 1] = mv.y;
    v.mover_rect[k * 4 + 2] = mv.w;
    v.mover_rect[k * 4 + 3] = mv.h;
    v.mover_fam[k] = float((mv.tile % 15) + 1);
  }
}

void push_entities(FrameView& v, const ew::game::Scene& s, float time_s) {
  for (const auto& e : s.entities()) {
    if (e.type == "switch" || e.type == "flood_switch") {  // abre-puertas pixel-art elevado (shape 3)
      push_marker(v, std::round(e.x), std::round(e.y), 10.f, 3,
                  e.active ? 0.25f : 1.0f, e.active ? 1.0f : 0.55f, e.active ? 0.50f : 0.12f);
      if (!e.active) {  // flecha pulsante sobre el abre-puertas aún sin accionar
        const float bob = std::round(std::sin(time_s * 6.5f + e.x * 0.1f) * 2.f);
        push_marker(v, std::round(e.x), std::round(e.y - 17.f + bob), 6.f, 4, 0.f, 1.f, 0.f);
      }
    } else if (e.type == "door") {  // se desliza hacia arriba al abrir (anim 0..1)
      const int h = ew::game::kDoorHTiles;
      const float rise = e.anim * float(h) * 8.f;
      if (e.anim < 0.99f) {
        const float vis = float(h) * (1.f - e.anim);
        const int n = std::max(1, int(std::ceil(vis / 8.f)));
        const float fade = 1.f - e.anim * 0.35f;
        for (int k = 0; k < n; ++k)
          push_marker(v, e.x + 4, e.y + 4 + 8.f * float(k) - rise, 4, 1, 0.6f * fade, 0.3f * fade, 0.85f * fade);
      }
      if (e.anim > 0.05f) push_marker(v, e.x + 4, e.y + 1, 1, 1, 0.3f, 0.2f, 0.4f);
    }
    else if (e.type == "player_spawn") push_marker(v, e.x, e.y + 7, 1, 1, 0.3f, 0.9f, 0.4f);
    else if (e.type == "goal") {  // pedestal + orbe dorado (más brillante al completar)
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
    } else if (e.type == "break_wall") {  // muro agrietado rompible con Dash (1×3 tiles)
      if (!e.active) {
        const float pulse = 0.78f + 0.22f * std::sin(time_s * 4.0f);
        for (int k = 0; k < ew::game::kDoorHTiles; ++k) {
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

void push_guide_arrows(FrameView& v, const ew::game::Scene& s, const ew::game::Sim& sim, float time_s) {
  if (sim.finished()) return;
  const auto& ents = s.entities();
  const auto mname = s.map().string_at(1);
  const float px = sim.player().pos.x, py = sim.player().pos.y;
  auto find_ent = [&](std::string_view name) -> const ew::game::Entity* {
    for (const auto& e : ents)
      if (e.name == name) return &e;
    return nullptr;
  };
  bool has_target = false;
  float tx = 0.f, ty = 0.f, style = 0.f;
  if (mname == "level2") {
    const auto* rel_dj = find_ent("rel_double_jump");
    const auto* door_hub = find_ent("door_hub_gallery");
    const auto* sw_sanc = find_ent("sw_sanctum");
    const auto* rel_da = find_ent("rel_dash");
    const auto* wall_g = find_ent("wall_guardian");
    if (!sim.has_ability(ew::game::kAbilityDoubleJump) && rel_dj && !rel_dj->active) {
      has_target = true; tx = rel_dj->x; ty = rel_dj->y; style = 1.f;
    } else if (door_hub && !door_hub->active && sw_sanc && !sw_sanc->active) {
      has_target = true; tx = sw_sanc->x; ty = sw_sanc->y; style = 0.f;
    } else if (!sim.has_ability(ew::game::kAbilityDash) && rel_da && !rel_da->active) {
      has_target = true; tx = rel_da->x; ty = rel_da->y; style = 1.f;
    } else if (door_hub && door_hub->active && wall_g && !wall_g->active && (py >= 184.f || px < 640.f)) {
      has_target = true; tx = door_hub->x; ty = door_hub->y + 12.f; style = 0.f;
    }
  } else if (mname == "level3") {
    const auto* door_seal = find_ent("door_wyrm_seal");
    const auto* sw_seal = find_ent("sw_seal3");
    if (door_seal && !door_seal->active && sw_seal && !sw_seal->active) {
      has_target = true; tx = sw_seal->x; ty = sw_seal->y; style = 0.f;
    } else if (door_seal && door_seal->active && py >= 184.f) {
      has_target = true; tx = door_seal->x; ty = door_seal->y + 12.f; style = 0.f;
    }
  } else {
    for (const auto& l : s.map().links()) {
      if (l.from_entity >= ents.size() || l.to_entity >= ents.size()) continue;
      const auto& from = ents[l.from_entity];
      const auto& to = ents[l.to_entity];
      if (to.type != "door" || to.active) continue;
      if ((from.type == "switch" || from.type == "flood_switch") && !from.active) {
        if (mname == "slice" && (from.name == "switch_well" || px > to.x + 16.f)) continue;
        has_target = true; tx = from.x; ty = from.y; style = 0.f;
        break;
      }
    }
    if (!has_target && mname == "slice" && px >= 400.f && px < 1130.f) {
      if (const auto* fl = find_ent("flood"); fl && !fl->active) {
        has_target = true; tx = fl->x; ty = fl->y; style = 0.f;
      }
    }
  }
  if (!has_target) return;
  const float dist_full = std::hypot(tx - px, ty - py);
  if (dist_full <= 24.f) return;
  float wx = tx, wy = ty;
  // Ruteo por el pozo vertical cuando el objetivo está en otra planta (level2 / level3).
  if (mname == "level2") {
    if (py < 184.f && ty >= 184.f) { wx = 448.f; wy = (px < 405.f) ? 148.f : 182.f; }
    else if (py >= 184.f && ty < 184.f) { wx = 472.f; wy = (px > 510.f || px < 420.f) ? 280.f : 188.f; }
  } else if (mname == "level3") {
    if (py < 184.f && ty >= 184.f) { wx = 568.f; wy = (px < 540.f) ? 148.f : 182.f; }
    else if (py >= 184.f && ty < 184.f) { wx = 568.f; wy = (px < 520.f) ? 320.f : 188.f; }
  }
  const float dx = wx - px, dy = wy - py;
  const float len = std::hypot(dx, dy);
  if (len < 1e-3f) return;
  const float ux = dx / len, uy = dy / len;
  const float pulse = std::sin(time_s * 6.0f) * 2.0f;
  const float ax = std::round(px + ux * (17.f + pulse));
  const float ay = std::round(py - 4.f + uy * (17.f + pulse));
  push_marker(v, ax, ay, 6.f, 4, ux, uy, style);
}

void push_hazard_span(FrameView& v, float x0, float y0, float w, float dir) {
  if (v.hazard_count >= ew::render::kMaxHazards || w <= 0.f) return;
  const int i = v.hazard_count++;
  v.hazards[std::size_t(i * 4 + 0)] = x0;
  v.hazards[std::size_t(i * 4 + 1)] = y0;
  v.hazards[std::size_t(i * 4 + 2)] = w;
  v.hazards[std::size_t(i * 4 + 3)] = dir;
}

// Pinchos (META 4) visibles alrededor de la cámara como tramos horizontales pixel-art.
void push_hazards(FrameView& v, ew::game::Scene& s) {
  const float ts = float(ew::map::kTileSize);
  const int x0 = int(std::floor(s.cam_x() / ts)), y0 = int(std::floor(s.cam_y() / ts));
  const int x1 = x0 + ew::game::kViewW / ew::map::kTileSize + 2;
  const int y1 = y0 + ew::game::kViewH / ew::map::kTileSize + 2;
  auto is_haz = [&](int tx, int ty) { return s.tile(tx, ty, ew::map::Layer::META) == ew::game::Sim::kMetaHazard; };
  for (int ty = y0; ty <= y1; ++ty) {
    int run_dir = 0, run_start = 0;
    auto flush = [&](int tx_end) {
      if (run_dir != 0)
        push_hazard_span(v, float(run_start) * ts, float(ty) * ts, float(tx_end - run_start) * ts, float(run_dir));
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
}  // namespace

static int g_last_key = -1;  // última tecla apretada (callback GLFW), para remapear

int main(int argc, char** argv) {
  int scale = 4, max_frames = 0;
  bool demo = false, has_start = false, nosound = false, nogamepad = false, nosave = false, explicit_map = false;
  float start_x = 0, start_y = 0;
  std::string shader_dir = EW_SHADER_DIR, map_path = EW_DEFAULT_MAP, dump_path, atlas_path, sprites_path,
              next_map_override;
  if (const char* e = std::getenv("EW_ATLAS")) atlas_path = e;
  if (const char* e = std::getenv("EW_SPRITES")) sprites_path = e;
  if (const char* e = std::getenv("EW_NOSOUND"); e && e[0] && e[0] != '0') nosound = true;
  if (const char* e = std::getenv("EW_NOGAMEPAD"); e && e[0] && e[0] != '0') nogamepad = true;
  if (const char* e = std::getenv("EW_NOSAVE"); e && e[0] && e[0] != '0') nosave = true;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--scale" && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (a == "--frames" && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (a == "--shaders" && i + 1 < argc) shader_dir = argv[++i];
    else if (a == "--map" && i + 1 < argc) { map_path = argv[++i]; explicit_map = true; }
    else if (a == "--next-map" && i + 1 < argc) next_map_override = argv[++i];
    else if (a == "--dump" && i + 1 < argc) dump_path = argv[++i];
    else if (a == "--atlas" && i + 1 < argc) atlas_path = argv[++i];
    else if (a == "--sprites" && i + 1 < argc) sprites_path = argv[++i];
    else if (a == "--demo") demo = true;
    else if (a == "--nosound") nosound = true;
    else if (a == "--nogamepad") nogamepad = true;
    else if (a == "--nosave") nosave = true;
    else if (a == "--version") { std::printf("echo_engine v%s\n", EW_VERSION); return 0; }
    else if (a == "--start" && i + 2 < argc) { start_x = float(std::atof(argv[++i])); start_y = float(std::atof(argv[++i])); has_start = true; }
  }
  if (scale < 1) scale = 1;
  // Assets portables: junto al ejecutable, cwd, o rutas de compile-time (dev).
  const std::string root = ew::core::exe_dir(argv[0]);
  const std::string build_maps_dir = ew::core::parent_path(EW_DEFAULT_MAP);
  if (shader_dir == EW_SHADER_DIR) {
    const auto vert = ew::core::pick_asset(root, "assets/shaders/fullscreen.vert",
                                           std::string(EW_SHADER_DIR) + "/fullscreen.vert");
    shader_dir = ew::core::parent_path(vert);
  }
  if (map_path == EW_DEFAULT_MAP)
    map_path = ew::core::pick_asset(root, "assets/maps/bin/slice.ewmap", EW_DEFAULT_MAP);
  if (atlas_path.empty())
    atlas_path = ew::core::pick_asset(root, "assets/atlas/tiles.png", EW_DEFAULT_ATLAS);
  if (sprites_path.empty())
    sprites_path = ew::core::pick_asset(root, "assets/atlas/sprites.png", EW_DEFAULT_SPRITES);
  const std::string initial_map_path = map_path;

  auto scene = std::make_unique<ew::game::Scene>();
  std::string err;
  if (!scene->load(map_path, err)) { std::fprintf(stderr, "echo_engine: %s\n", err.c_str()); return 1; }
  auto sim = std::make_unique<ew::game::Sim>(*scene);
  if (has_start) {  // --start X Y: aparece en otro punto (p. ej. probar el seam/otro chunk)
    auto& p = sim->world().points()[sim->player_index()];
    p.pos = p.prev = {start_x, start_y};
    scene->follow(start_x, start_y);
  }
  std::printf("echo_engine v%s\n", EW_VERSION);
  std::printf("echo_engine: mapa %s (%dx%d px, %zu entidades)\n", map_path.c_str(), scene->width_px(),
              scene->height_px(), scene->entities().size());
  if (scene->find_type("goal")) std::printf("echo_engine: hay una meta — llegá para completar el camino\n");
  if (scene->find_type("checkpoint"))
    std::printf("echo_engine: R corto vuelve al checkpoint, R largo (~0.8 s) al inicio\n");

  glfwSetErrorCallback([](int c, const char* d) { std::fprintf(stderr, "glfw %d: %s\n", c, d); });
  if (!glfwInit()) return 1;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  GLFWwindow* win = glfwCreateWindow(ew::render::kInternalW * scale, ew::render::kInternalH * scale,
                                     "Echo Well v" EW_VERSION, nullptr, nullptr);
  if (!win) { glfwTerminate(); return 1; }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  // ESC abre/cierra el menú de pausa (se maneja en el loop, no cierra la ventana).

  if (const char* miss = ew::gl::load([](const char* n) { return reinterpret_cast<void*>(glfwGetProcAddress(n)); })) {
    std::fprintf(stderr, "echo_engine: falta %s (se requiere GL 3.3 core)\n", miss);
    return 1;
  }
  std::printf("echo_engine: GL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

  ew::render::DeferredRenderer renderer;
  if (!renderer.init(shader_dir, err, atlas_path, sprites_path)) { std::fprintf(stderr, "echo_engine: %s\n", err.c_str()); return 1; }
  ew::audio::Mixer mixer;
  if (!nosound) {
    if (ew::audio::device_start(mixer))
      std::printf("echo_engine: SFX on\n");
  } else {
    std::printf("echo_engine: SFX off (--nosound)\n");
  }

  int frames = 0, sim_steps = 0;
  bool jump_was_down = false, jump_up_was_down = false, act_was_down = false, grab_was_down = false, r_was_down = false, r_full_sent = false, announced_goal = false;
  bool esc_was = false, start_was = false, m_up_was = false, m_dn_was = false, m_lt_was = false, m_rt_was = false, m_ok_was = false, m_b_was = false;
  double acc = 0, last = glfwGetTime(), r_down_since = -1;
  ew::game::PauseMenu menu;
  ew::game::EndScreen end_screen;
  ew::game::DialogueBox dialogue;
  int last_items = 0;
  uint32_t last_abilities = 0;
  uint32_t pad_prev = 0;
  glfwSetKeyCallback(win, [](GLFWwindow*, int k, int, int action, int) { if (action == GLFW_PRESS) g_last_key = k; });

  auto resolve_next_map = [&]() -> std::string {
    if (!next_map_override.empty() && map_path == initial_map_path && ew::core::file_exists(next_map_override))
      return next_map_override;
    const auto next_name = ew::game::next_campaign_map(scene->map().string_at(1));
    if (next_name.empty()) return {};
    return ew::game::resolve_map_file(next_name, map_path, root, build_maps_dir);
  };
  auto load_level = [&](const std::string& path, uint32_t carry_abilities) -> bool {
    auto next_scene = std::make_unique<ew::game::Scene>();
    std::string lerr;
    if (!next_scene->load(path, lerr)) {
      std::fprintf(stderr, "echo_engine: %s\n", lerr.c_str());
      return false;
    }
    auto next_sim = std::make_unique<ew::game::Sim>(*next_scene);
    if (carry_abilities) next_sim->set_abilities(carry_abilities);
    scene = std::move(next_scene);
    sim = std::move(next_sim);
    map_path = path;
    renderer.invalidate_tiles();
    announced_goal = false;
    end_screen.close();
    dialogue.close();
    last_items = sim->collected();
    last_abilities = sim->abilities();
    glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
    std::printf("echo_engine: mapa %s (%dx%d px, %zu entidades)\n", map_path.c_str(), scene->width_px(),
                scene->height_px(), scene->entities().size());
    return true;
  };

  // Partida guardada (un slot). Sin guardado con --nosave, --demo o --start, ni con --frames
  // salvo que EW_SAVE_DIR esté definido (corridas deterministas / de prueba). Se autoguarda
  // al progresar (checkpoint, huevo, boss, cambio de nivel) y al cambiar el volumen; si al
  // arrancar hay save (del mismo mapa o de otro mapa de la campaña), el menú abre en CONTINUAR.
  std::string save_path;
  const bool explicit_dir = std::getenv("EW_SAVE_DIR") != nullptr;  // NOLINT(concurrency-mt-unsafe)
  if (!nosave && !demo && !has_start && (!max_frames || explicit_dir)) {
    const std::string dir = ew::game::save_dir(root);
    if (!dir.empty()) save_path = ew::core::join(dir, ew::game::kSaveFileName);
  }
  auto save_matches = [&](const ew::game::SaveData& d) {
    return d.map == scene->map().string_at(1) && d.active.size() == scene->entities().size();
  };
  auto can_continue_save = [&](const ew::game::SaveData& d) {
    if (save_matches(d)) return true;
    if (explicit_map) return false;
    return !ew::game::resolve_map_file(d.map, map_path, root, build_maps_dir).empty();
  };
  ew::game::SaveData loaded;
  bool save_armed = true;  // false = hay un save sin continuar todavía: no pisarlo con un spawn limpio
  if (!save_path.empty()) {
    const auto e = ew::game::read_save_file(save_path, loaded);
    if (e == ew::game::SaveError::None && can_continue_save(loaded)) {
      menu.set_has_save(true);
      menu.set_audio(loaded.volume, loaded.muted);
      menu.set_bindings(loaded.bind);
      menu.open_at(ew::game::PauseMenu::kContinuar);
      save_armed = false;
      std::printf("echo_engine: partida guardada en %s — CONTINUAR en el menú\n", save_path.c_str());
    } else if (e == ew::game::SaveError::None) {  // save de otro mapa no disponible: solo audio + controles
      menu.set_audio(loaded.volume, loaded.muted);
      menu.set_bindings(loaded.bind);
      std::printf("echo_engine: save de otro mapa (se usan volumen y controles)\n");
    } else if (e != ew::game::SaveError::Io) {
      std::printf("echo_engine: save ignorado (%s)\n", e == ew::game::SaveError::None ? "otro mapa" : ew::game::save_error_str(e));
    } else {
      std::printf("echo_engine: guardado en %s\n", save_path.c_str());
    }
  }
  struct SaveKey {
    std::string map;
    int ck = -1, items = 0;
    uint32_t abilities = 0;
    bool boss = false, muted = false;
    float vol = 0;
    bool operator==(const SaveKey&) const = default;
  };
  auto save_key = [&] {
    return SaveKey{std::string(scene->map().string_at(1)), sim->checkpoint_index(), sim->collected(),
                   sim->abilities(), sim->boss_count() > 0 && sim->bosses_defeated(), menu.muted(),
                   menu.volume_setting()};
  };
  SaveKey last_key = save_key();
  auto write_save = [&](ew::game::SaveData d) {
    d.volume = menu.volume_setting();
    d.muted = menu.muted();
    d.bind = menu.bindings();
    if (ew::game::write_save_file(save_path, d) == ew::game::SaveError::None) {
      menu.set_has_save(true);
    } else {
      std::fprintf(stderr, "echo_engine: no se pudo guardar en %s\n", save_path.c_str());
    }
  };
  bool dash_was_down = false;
  ew::game::Input latched_in;
  const double t_loop = glfwGetTime();
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    auto key = [&](int k) { return glfwGetKey(win, k) == GLFW_PRESS; };
    ew::game::Input in;
    // Teclas de juego según los controles (remapeables en el menú).
    const auto& bind = menu.bindings();
    auto act = [&](int a) {
      return bind.key_down(a, [](int k, void* w) { return glfwGetKey(static_cast<GLFWwindow*>(w), k) == GLFW_PRESS; }, win);
    };
    using namespace ew::game;
    in.move_x = float(int(act(kBindRight)) - int(act(kBindLeft)));
    const bool hanging = sim->on_rope();
    // Colgado, arriba/abajo trepan (no saltan ni activan switch).
    if (hanging) in.climb = float(int(act(kBindDown)) - int(act(kBindUp)));
    const bool j_key = act(kBindJump);
    const bool u_key = !hanging && act(kBindUp);
    in.jump = (j_key && !jump_was_down) || (u_key && !jump_up_was_down);
    jump_was_down = j_key;
    jump_up_was_down = u_key;
    const bool act_key = act(kBindAct);
    const bool dash_key = key(GLFW_KEY_K) || key(GLFW_KEY_RIGHT_SHIFT);
    const bool act_down = act_key || (!hanging && act(kBindDown));
    in.interact = act_down && !act_was_down;
    act_was_down = act_down;
    const bool dash_down = act_key || dash_key;
    in.dash = dash_down && !dash_was_down;
    dash_was_down = dash_down;
    const bool grab_down = act(kBindGrab);
    in.grab = grab_down && !grab_was_down;
    grab_was_down = grab_down;
    const bool r_down = act(kBindRestart);
    const double now_in = glfwGetTime();
    if (r_down) {
      if (r_down_since < 0) r_down_since = now_in;
      if (!r_full_sent && (now_in - r_down_since) >= ew::game::Sim::kRestartFullHold) {
        in.restart_full = true;
        r_full_sent = true;
      }
    } else {
      if (r_was_down && !r_full_sent) in.restart = true;  // R corto: al soltar
      r_down_since = -1;
      r_full_sent = false;
    }
    r_was_down = r_down;
    uint32_t pad_buttons = 0;
    bool pad_start = false, pad_a = false, pad_b = false, pad_du = false, pad_dd = false, pad_dl = false, pad_dr = false;
    // Gamepad (primer mando reconocido). Se combina con el teclado; --demo lo ignora.
    if (!demo && !nogamepad) {
      ew::game::PadRaw raw;
      bool found = false;
      for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
        if (!glfwJoystickIsGamepad(j)) continue;
        GLFWgamepadstate st;
        if (!glfwGetGamepadState(j, &st)) continue;
        raw.lx = st.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        raw.ly = st.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        raw.a = st.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
        raw.b = st.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
        pad_a = raw.a; pad_b = raw.b;
        pad_start = st.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
        raw.x = st.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
        raw.y = st.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
        raw.lb = st.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
        raw.rb = st.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
        raw.start = false;  // Start abre el menú, no es R
        raw.du = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS;
        raw.dd = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS;
        raw.dl = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS;
        raw.dr = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS;
        pad_du = raw.du; pad_dd = raw.dd; pad_dl = raw.dl; pad_dr = raw.dr;
        for (int bt = 0; bt <= GLFW_GAMEPAD_BUTTON_LAST; ++bt)
          if (st.buttons[bt] == GLFW_PRESS) raw.buttons |= 1u << unsigned(bt);
        pad_buttons = raw.buttons;
        found = true;
        break;
      }
      static ew::game::PadEdges pad_edges;
      static bool pad_announced = false;
      if (found) {
        if (!pad_announced) { std::printf("echo_engine: gamepad\n"); pad_announced = true; }
        ew::game::apply_pad(raw, in, hanging, pad_edges, now_in, ew::game::Sim::kRestartFullHold, &bind);
      }
    }
    if (demo) {  // camina ida y vuelta y salta cada ~1.5 s
      in.move_x = (sim_steps / 150) % 2 ? -1.f : 1.f;
      in.jump = sim_steps % 90 == 45;
      in.interact = sim_steps % 300 == 43;  // ~x=96: pasa por el switch y lo alterna
    }
    // Menú de pausa (Esc / Start) y pantalla de final. En --demo no se abren (determinista).
    ew::game::MenuInput mi, ei;
    if (!demo) {
      const bool esc_down = key(GLFW_KEY_ESCAPE);
      bool nav_toggle = (esc_down && !esc_was) || (pad_start && !start_was);
      esc_was = esc_down;
      start_was = pad_start;
      if (menu.open() && !menu.waiting() && pad_b && !m_b_was) nav_toggle = true;  // B cierra / vuelve
      m_b_was = pad_b;
      const bool up = key(GLFW_KEY_UP) || key(GLFW_KEY_W) || pad_du;
      const bool dn = key(GLFW_KEY_DOWN) || key(GLFW_KEY_S) || pad_dd;
      const bool lt = key(GLFW_KEY_LEFT) || key(GLFW_KEY_A) || pad_dl;
      const bool rt = key(GLFW_KEY_RIGHT) || key(GLFW_KEY_D) || pad_dr;
      const bool ok = key(GLFW_KEY_ENTER) || key(GLFW_KEY_SPACE) || key(GLFW_KEY_Z) || pad_a;
      const bool nav_up = up && !m_up_was;
      const bool nav_dn = dn && !m_dn_was;
      const bool nav_lt = lt && !m_lt_was;
      const bool nav_rt = rt && !m_rt_was;
      const bool nav_ok = ok && !m_ok_was;
      m_up_was = up; m_dn_was = dn; m_lt_was = lt; m_rt_was = rt; m_ok_was = ok;
      if (menu.open()) {
        mi.toggle = nav_toggle;
        if (menu.waiting()) {  // remapeo: la tecla / el botón recién apretado
          if (g_last_key != GLFW_KEY_ESCAPE) mi.key = g_last_key;
          const uint32_t fresh = pad_buttons & ~pad_prev;
          for (int bt = 0; bt <= GLFW_GAMEPAD_BUTTON_LAST && mi.key < 0; ++bt)
            if ((fresh >> unsigned(bt)) & 1u) { mi.pad_button = bt; break; }
        }
        mi.up = nav_up;
        mi.down = nav_dn;
        mi.left = nav_lt;
        mi.right = nav_rt;
        mi.confirm = nav_ok;
      } else if (dialogue.open()) {
        if (nav_toggle) {
          dialogue.close();
        } else if (nav_ok || in.interact) {
          dialogue.advance();
        }
      } else if (end_screen.open()) {
        if (nav_toggle) {
          ei.toggle = true;
        } else if (sim->finish_t() >= 0.35f) {
          ei.up = nav_up;
          ei.down = nav_dn;
          ei.confirm = nav_ok;
        }
      } else {
        mi.toggle = nav_toggle;
      }
    }
    g_last_key = -1;
    pad_prev = pad_buttons;
    const auto mact = menu.handle(mi);
    if (mact == ew::game::MenuAction::Quit) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (mact == ew::game::MenuAction::Continue && !save_path.empty()) {
      ew::game::SaveData d;
      auto e = ew::game::read_save_file(save_path, d);
      if (e == ew::game::SaveError::None) {
        if (d.map != scene->map().string_at(1)) {
          const std::string target = ew::game::resolve_map_file(d.map, map_path, root, build_maps_dir);
          if (target.empty()) {
            e = ew::game::SaveError::Mismatch;
          } else {
            auto next_scene = std::make_unique<ew::game::Scene>();
            std::string lerr;
            if (!next_scene->load(target, lerr)) {
              e = ew::game::SaveError::Io;
            } else {
              auto next_sim = std::make_unique<ew::game::Sim>(*next_scene);
              e = ew::game::apply_save(*next_sim, d);
              if (e == ew::game::SaveError::None) {
                scene = std::move(next_scene);
                sim = std::move(next_sim);
                map_path = target;
                renderer.invalidate_tiles();
                announced_goal = false;
                end_screen.close();
                glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
              }
            }
          }
        } else {
          e = ew::game::apply_save(*sim, d);
        }
      }
      if (e == ew::game::SaveError::None) {
        menu.set_audio(d.volume, d.muted);
        menu.set_bindings(d.bind);
        save_armed = true;
        last_key = save_key();
        last_items = sim->collected();  // el huevo cargado no se anuncia como recién agarrado
        last_abilities = sim->abilities();
        end_screen.close();
        dialogue.close();
        std::printf("echo_engine: partida cargada (checkpoint %d, huevos %d/%d)\n", sim->checkpoint_index(),
                    sim->collected(), sim->collectible_total());
      } else {
        std::fprintf(stderr, "echo_engine: no se pudo cargar la partida (%s)\n", ew::game::save_error_str(e));
      }
    }
    if (mact == ew::game::MenuAction::Restart) {
      ew::game::Input rst; rst.restart = true;
      sim->step(rst);
      ++sim_steps;
    }
    const auto eact = end_screen.handle(ei);
    if (eact == ew::game::EndAction::Quit) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (eact == ew::game::EndAction::OpenMenu) menu.open_at(ew::game::PauseMenu::kSeguir);
    if (eact == ew::game::EndAction::RestartLevel) {
      sim->reset();
      end_screen.close();
      dialogue.close();
      announced_goal = false;
      glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
      std::printf("echo_engine: reinicio\n");
    }
    if (eact == ew::game::EndAction::NextLevel) {
      const std::string next_p = resolve_next_map();
      const uint32_t carry = sim->abilities();
      if (!next_p.empty() && load_level(next_p, carry)) {
        if (!save_path.empty()) {
          save_armed = true;
          write_save(ew::game::capture_save(*sim));
        }
        last_key = save_key();
      }
    }
    mixer.set_gain(menu.volume_setting());
    mixer.set_mute(menu.muted());
    sim->set_wind_enabled(menu.foliage());

    // Paso fijo 60 Hz (con --frames: 1 paso por frame, determinista).
    // Nota: cuando end_screen está abierta, Sim::step debe seguir corriendo para avanzar finish_t_ (u_fade 0->1).
    const double now = glfwGetTime();
    if (menu.open() || dialogue.open()) {
      acc = 0;
      last = now;
      latched_in = {};
    } else if (end_screen.open()) {
      acc += max_frames ? ew::game::Sim::kDt : now - last;
      in = {};
      latched_in = {};
    } else {
      acc += max_frames ? ew::game::Sim::kDt : (now - last) * double(sim->victory_seq().time_scale());
      in.jump = in.jump || latched_in.jump;
      in.interact = in.interact || latched_in.interact;
      in.grab = in.grab || latched_in.grab;
      in.dash = in.dash || latched_in.dash;
      in.restart = in.restart || latched_in.restart;
      in.restart_full = in.restart_full || latched_in.restart_full;
      latched_in = in;
    }
    last = now;
    const bool did_restart = in.restart || in.restart_full || mact == ew::game::MenuAction::Restart ||
                             mact == ew::game::MenuAction::Continue || eact == ew::game::EndAction::RestartLevel;
    for (int n = 0; !menu.open() && !dialogue.open() && acc >= ew::game::Sim::kDt && n < 5;
         ++n, acc -= ew::game::Sim::kDt) {
      sim->step(in);
      if (!demo) {
        if (const int dlg_id = sim->take_dialog(); dlg_id >= 0 && std::size_t(dlg_id) < scene->dialogs().size()) {
          const auto& d = scene->dialogs()[std::size_t(dlg_id)];
          dialogue.open(d.speaker, d.text);
        }
      }
      if (!demo && !max_frames && !sim->finished()) {
        ew::game::BossVictoryDialog vdlg;
        if (sim->take_victory_dialog(vdlg)) dialogue.open(vdlg.speaker, vdlg.text);
      }
      in.jump = false;
      in.interact = false;
      in.grab = false;
      in.dash = false;
      in.restart = false;
      in.restart_full = false;
      latched_in = {};
      ++sim_steps;
    }
    if (!nosound) {
      const uint32_t bits = sim->take_sfx();
      using namespace ew::audio;
      if (bits & kSfxJump) device_play(SfxId::Jump);
      if (bits & kSfxDie) device_play(SfxId::Die);
      if (bits & kSfxCheckpoint) device_play(SfxId::Checkpoint);
      if (bits & kSfxGoal) device_play(SfxId::Goal);
      if (bits & kSfxSwitch) device_play(SfxId::Switch);
      if (bits & kSfxSplash) device_play(SfxId::Splash);
      if (bits & kSfxPickup) device_play(SfxId::Pickup);
      if (bits & kSfxAbility) device_play(SfxId::Ability);
      if (bits & kSfxDash) device_play(SfxId::Dash);
      if (bits & kSfxBreak) device_play(SfxId::Break);
      if (bits & kSfxHit) device_play(SfxId::Hit);
      if (bits & kSfxBossCrt) device_play(SfxId::BossCrt);
    }
    if (acc > ew::game::Sim::kDt) acc = 0;
    if (sim->finished() && !announced_goal && !dialogue.open()) {
      ew::game::EndStats st;
      st.title = ew::game::level_title(scene->map().string_at(1));
      st.items = sim->collected();
      st.items_total = sim->collectible_total();
      st.deaths = sim->deaths();
      st.abilities = sim->abilities();
      st.has_next = !resolve_next_map().empty();
      const auto mname = scene->map().string_at(1);
      st.elapsed_sec = sim->elapsed_sec();
      st.campaign_done = !st.has_next && ew::game::find_campaign_level(mname) != nullptr;
      end_screen.open(st);
      std::printf("echo_engine: ¡meta! camino completado  (R corto: checkpoint, R largo: inicio)\n");
      announced_goal = true;
      glfwSetWindowTitle(win, "Echo Well — ¡meta!  [R] checkpoint, hold inicio");
    }
    if (!sim->finished() && end_screen.open()) end_screen.close();
    if (did_restart && announced_goal && !sim->finished()) {
      announced_goal = false;
      glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
      std::printf("echo_engine: reinicio\n");
    }
    if (!save_path.empty()) {  // autoguardado
      const SaveKey k = save_key();
      const bool bind_changed = menu.take_bindings_changed();
      if (!(k == last_key) || bind_changed) {
        const bool progress = k.map != last_key.map || k.ck != last_key.ck || k.items != last_key.items ||
                              k.abilities != last_key.abilities || k.boss != last_key.boss;
        if (progress && (k.ck >= 0 || k.items > 0 || k.abilities != 0 || k.boss)) save_armed = true;  // jugando de verdad
        if (save_armed) write_save(ew::game::capture_save(*sim));
        else if (menu.has_save()) write_save(loaded);  // solo volumen/controles: no pisar la partida
        last_key = k;
      }
    }
    const auto& pl = sim->player();
    const float px = pl.pos.x, py = pl.pos.y;

    FrameView v;
    float shake_x = 0.f, shake_y = 0.f;
    sim->victory_seq().camera_shake(shake_x, shake_y);
    v.cam_x = std::round(scene->cam_x() + shake_x);
    v.cam_y = std::round(scene->cam_y() + shake_y);  // pixel-perfect
    v.chunk_x_px = scene->nb_origin_x();  // vecindario 3×3 alrededor del chunk activo
    v.chunk_y_px = scene->nb_origin_y();
    v.tiles = scene->active_tiles().data();
    v.tiles_version = scene->chunk_version();
    for (std::size_t i = 0; i < 9; ++i) {  // fondos del vecindario 3×3 (sección BACKDROP)
      v.bg_theme[i] = scene->active_backdrops()[i].theme;
      v.bg_seed[i] = scene->active_backdrops()[i].seed;
    }
    push_entities(v, *scene, float(now));
    if (!demo) push_guide_arrows(v, *scene, *sim, float(now));
    if (const int nd = sim->nearby_interact_dialog();
        nd >= 0 && !dialogue.open() && !menu.open() && !end_screen.open() && std::size_t(nd) < scene->dialogs().size()) {
      const auto& de = scene->entities()[scene->dialogs()[std::size_t(nd)].entity];
      push_marker(v, de.x, de.y - 12.f + std::round(std::sin(float(now) * 5.f)), 1.5f, 0, 1.0f, 0.88f, 0.35f);
    }
    push_movers(v, *sim);
    for (int bi = 0; bi < sim->boss_count() && v.boss_count < ew::render::kMaxBosses; ++bi) {
      const auto& b = sim->boss(bi);
      if (b.defeated && sim->boss_burst() <= 0) continue;
      if (b.defeated && (sim->boss_burst() % 2 == 0)) continue;  // se desintegra en scanlines CRT
      if (!b.defeated && b.hurt > 0.f && int(b.hurt * 20.f) % 2 == 0) continue;  // parpadea herido
      const float crt_jitter = b.defeated ? std::round(std::sin(float(sim->boss_burst()) * 2.1f) * 3.f) : 0.f;
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
    const auto& vic = sim->victory_seq();
    if (vic.in_defeat_anim()) {
      v.boss_burst_x = vic.origin_x() - v.cam_x;
      v.boss_burst_y = vic.origin_y() - v.cam_y;
      v.boss_burst_t = vic.crt_progress();
    }
    v.boss_flash = vic.flash_alpha();
    for (const auto& pt : vic.particles()) {
      if (v.marker_count >= ew::render::kMaxMarkers) break;
      if (pt.life <= 0.f) continue;
      const float f = std::clamp(pt.life / std::max(pt.max_life, 1e-4f), 0.f, 1.f);
      push_marker(v, pt.x, pt.y, pt.radius * (0.4f + 0.6f * f), 1, pt.r * f, pt.g * f, pt.b * f);
    }
    for (const auto& fb : sim->fireballs()) {
      push_marker(v, fb.x, fb.y, ew::game::Sim::kFireballRadius + 1.f, 0, 0.96f, 0.34f, 0.08f);
      push_marker(v, fb.x, fb.y, 1.8f, 0, 1.0f, 0.92f, 0.45f);
    }
    for (int ii = 0; ii < sim->imp_count(); ++ii) {  // diablillos menores (caen con pisotón o Dash)
      const auto& im = sim->imp(ii);
      if (im.defeated) continue;
      const float bob = std::sin(float(now) * 8.f + float(ii)) * 0.6f;
      push_marker(v, im.x, im.y - 5.f + bob, 4.5f, 1, 0.32f, 0.14f, 0.24f);
      push_marker(v, im.x - 2.f, im.y - 9.5f + bob, 1.2f, 1, 0.55f, 0.28f, 0.32f);
      push_marker(v, im.x + 2.f, im.y - 9.5f + bob, 1.2f, 1, 0.55f, 0.28f, 0.32f);
      push_marker(v, im.x + float(im.dir) * 2.f, im.y - 5.5f + bob, 1.2f, 0, 1.0f, 0.42f, 0.15f);
    }
    push_hazards(v, *scene);
    if (sim->finished())
      if (const auto* g = scene->find_type("goal"))
        push_marker(v, g->x, g->y, 8, 0, 1.0f, 1.0f, 0.55f);
    const auto& pts = sim->world().points();
    for (int r = 0; r < sim->rope_count(); ++r)  // cuerdas Verlet (sección ROPE)
      for (int i = 0; i < sim->rope_points(r); ++i) {
        const auto& rp = pts[sim->rope_first(r) + uint32_t(i)];
        const bool held = sim->on_rope() && sim->held_rope() == r && i == sim->rope_node();
        const float rad = i == 0 ? 2.f : (held ? 2.5f : rp.radius);
        const float cr = held ? 1.0f : 0.75f, cg = held ? 0.85f : 0.6f, cb = held ? 0.4f : 0.4f;
        push_marker(v, rp.pos.x, rp.pos.y, rad, i == 0 ? 1 : 0, cr, cg, cb);
      }
    if (sim->grab_flash() > 0 && sim->on_rope()) {  // anillo que se expande al agarrar
      const auto& node = pts[sim->rope_first(sim->held_rope()) + uint32_t(sim->rope_node())];
      const float f = float(sim->grab_flash()) / float(ew::game::Sim::kGrabFlashFrames);
      push_marker(v, node.pos.x, node.pos.y, 3.f + (1.f - f) * 10.f, 0, 1.0f, 0.9f * f, 0.35f * f);
    }
    if (sim->double_jump_flash() > 0) {  // aro cian que se expande al activar doble salto en el aire
      const float f = float(sim->double_jump_flash()) / float(ew::game::Sim::kDoubleJumpFlashFrames);
      push_marker(v, sim->double_jump_x(), sim->double_jump_y(), 2.5f + (1.f - f) * 7.f, 0,
                  0.45f * f, 0.92f * f, 1.0f * f);
    }
    if (sim->wall_break_flash() > 0) {  // fragmentos que saltan al destruir un break_wall con Dash
      const float f = float(sim->wall_break_flash()) / float(ew::game::Sim::kWallBreakFlashFrames);
      const float spread = (1.f - f) * 14.f;
      for (int k = 0; k < 4; ++k) {
        const float sx = (k & 1) ? 1.f : -1.f, sy = (k & 2) ? 1.f : -1.f;
        push_marker(v, sim->wall_break_x() + sx * spread, sim->wall_break_y() + sy * (spread * 0.7f - 2.f),
                    2.f * f, 1, 0.95f * f, 0.55f * f, 0.25f * f);
      }
    }
    {  // protagonista pixel-art 16×16 (Explorador del Farol): espejado según dirección y gravedad
      const float d = sim->down();
      const bool inv = sim->gravity_flipped();
      const bool dashing = sim->dash_ticks() > 0;
      if (dashing) {  // estela ígnea detrás del Explorador al embestir
        const float dir = float(sim->dash_dir());
        push_sprite(v, std::round(px - dir * 8.f), std::round(py - 2.f * d), 1, 2,
                    sim->player_facing() < 0, inv, 0.70f, 0.34f, 0.16f, 0.08f);
        push_sprite(v, std::round(px - dir * 4.f), std::round(py - 2.f * d), 2, 2,
                    sim->player_facing() < 0, inv, 1.05f, 0.52f, 0.22f, 0.18f);
      }
      const bool blink = sim->invuln() > 0 && (sim->invuln() / 2) % 2 == 1;
      const float tr = blink ? 0.55f : (dashing ? 1.25f : (inv ? 0.90f : 1.0f));
      const float tg = blink ? 0.55f : (dashing ? 0.90f : (inv ? 0.85f : 1.0f));
      const float tb = blink ? 0.70f : (dashing ? 0.55f : (inv ? 1.08f : 1.0f));
      const float boost = dashing ? 0.38f : (sim->double_jump_flash() > 0 ? 0.25f : 0.05f);
      push_sprite(v, std::round(px), std::round(py - 2.f * d), sim->player_anim_frame(), int(sim->player_anim()),
                  sim->player_facing() < 0, inv, tr, tg, tb, boost);
    }
    for (const auto& w : sim->water()) {  // hasta kMaxWaterBodies; solo los que tocan la vista
      if (v.water_count >= ew::render::kMaxWaterBodies) break;
      if (w.x() + w.width() < v.cam_x || w.x() > v.cam_x + ew::render::kInternalW ||
          w.bottom() < v.cam_y || w.rest_y() - 32 > v.cam_y + ew::render::kInternalH) continue;
      const int b = v.water_count++;
      const int n = std::min(w.cols(), ew::render::kMaxWaterCols);
      v.water_cols[std::size_t(b)] = n;
      for (int k = 0; k < 4; ++k)
        v.water_rect[std::size_t(b * 4 + k)] = std::array<float, 4>{w.x(), w.rest_y(), w.width(), w.bottom()}[std::size_t(k)];
      for (int i = 0; i < n; ++i) v.water_surface[std::size_t(b * ew::render::kMaxWaterCols + i)] = w.surface_col(i);
    }
    v.light_x = px; v.light_y = py;
    v.fade = sim->finish_t();
    v.pause = menu.open() ? 1 : 0;
    v.pause_sel = menu.cursor();
    v.pause_vol = menu.volume_setting();
    v.pause_mute = menu.muted() ? 1 : 0;
    v.pause_has_save = menu.has_save() ? 1 : 0;
    if (menu.open() && !menu.in_controls()) {  // versión abajo a la derecha del menú
      std::array<std::string, ew::game::kTxtRows> t{};
      t[ew::game::kTxtRows - 1] = "V" EW_VERSION;
      v.ctl_txt = ew::game::pack_text(t);
    }
    if (menu.in_controls()) {
      v.ctl = 1;
      v.ctl_sel = menu.ctl_cursor() + 1;
      v.ctl_wait = menu.waiting() ? 1 : 0;
      v.ctl_txt = ew::game::pack_text(ew::game::controls_text(menu));
    }
    if (!menu.open() && end_screen.open()) {
      v.end_screen = 1;
      v.end_sel = end_screen.cursor();
      v.end_has_next = end_screen.stats().has_next ? 1 : 0;
      v.ctl_txt = ew::game::pack_text(ew::game::end_screen_text(end_screen));
    } else if (!menu.open() && dialogue.open()) {
      v.dlg = 1;
      v.dlg_more = dialogue.has_more() ? 1 : 0;
      v.ctl_txt = ew::game::pack_text(ew::game::dialogue_text(dialogue));
    }
    v.items = sim->collected();
    v.items_total = sim->collectible_total();
    v.items_flash = float(sim->pickup_flash()) / float(ew::game::Sim::kPickupFlashFrames);
    if (v.items != last_items) {
      if (v.items > last_items) std::printf("echo_engine: ¡huevo! %d/%d\n", v.items, v.items_total);
      last_items = v.items;
    }
    const uint32_t ab = sim->abilities();
    if (ab != last_abilities) {
      if ((ab & ew::game::kAbilityDoubleJump) && !(last_abilities & ew::game::kAbilityDoubleJump))
        std::printf("echo_engine: ¡habilidad desbloqueada! doble salto\n");
      if ((ab & ew::game::kAbilityDash) && !(last_abilities & ew::game::kAbilityDash))
        std::printf("echo_engine: ¡habilidad desbloqueada! embestida (dash)\n");
      last_abilities = ab;
    }
    v.abilities = int(ab);
    v.ability_flash = float(sim->ability_flash()) / float(ew::game::Sim::kAbilityFlashFrames);
    v.map_w = scene->map().header().map_w_chunks;
    v.map_h = scene->map().header().map_h_chunks;
    v.cur_cx = scene->chunk_x();
    v.cur_cy = scene->chunk_y();
    v.visited_mask = uint32_t(scene->visited_chunks() & 0xFFFFFFFFu);
    v.foliage_enabled = sim->wind_enabled() ? 1 : 0;
    v.wind_gust = sim->wind().current_gust(uint32_t(v.bg_seed[4]));
    v.wind_impulse_count = sim->wind().pack_impulses(v.wind_impulses);

    int fw, fh; glfwGetFramebufferSize(win, &fw, &fh);
    renderer.render(static_cast<float>(glfwGetTime()), fw, fh, v);
    glfwSwapBuffers(win);
    if (max_frames && ++frames >= max_frames) break;
  }
  if (!dump_path.empty() && !renderer.dump_ppm(dump_path)) std::fprintf(stderr, "echo_engine: no se pudo escribir %s\n", dump_path.c_str());
  const double elapsed = glfwGetTime() - t_loop;
  const double ms_frame = frames > 0 ? elapsed * 1000.0 / double(frames) : 0;
  std::printf("echo_engine: %d frames, %d sim steps, %.2f ms total, %.2f ms/frame (%.1f fps), salida limpia\n",
              frames, sim_steps, elapsed * 1000.0, ms_frame, elapsed > 0 ? double(frames) / elapsed : 0);
  ew::audio::device_stop();
  renderer.shutdown();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
