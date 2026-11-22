// echo_engine: ventana + loop + Deferred 320x180 sobre un mapa .ewmap.
//   echo_engine [--scale N] [--frames N] [--shaders DIR] [--map FILE] [--next-map FILE] [--atlas FILE.png] [--sprites FILE.png] [--dump FILE.ppm] [--demo] [--start X Y] [--hero farol|zarigueya|castor] [--nosound] [--nogamepad] [--nosave] [--version]
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "audio/device.hpp"
#include "audio/sfx.hpp"
#include "core/paths.hpp"
#include "game/frame_view.hpp"
#include "game/gamepad.hpp"
#include "game/hud.hpp"
#include "game/level_flow.hpp"
#include "game/music.hpp"
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
#ifndef EW_VERSION
#define EW_VERSION "0.1.0"
#endif

namespace {

int g_last_key = -1;  // última tecla apretada (callback GLFW), para remapear

void play_sfx(uint32_t bits) {
  using namespace ew::audio;
  if (!bits) return;
  static constexpr std::pair<uint32_t, SfxId> kMap[] = {
      {kSfxJump, SfxId::Jump},           {kSfxDie, SfxId::Die},             {kSfxCheckpoint, SfxId::Checkpoint},
      {kSfxGoal, SfxId::Goal},           {kSfxSwitch, SfxId::Switch},       {kSfxSplash, SfxId::Splash},
      {kSfxPickup, SfxId::Pickup},       {kSfxAbility, SfxId::Ability},     {kSfxDash, SfxId::Dash},
      {kSfxBreak, SfxId::Break},         {kSfxHit, SfxId::Hit},             {kSfxBossCrt, SfxId::BossCrt},
      {kSfxStepStone, SfxId::StepStone}, {kSfxStepMoss, SfxId::StepMoss},   {kSfxStepMetal, SfxId::StepMetal},
      {kSfxStepWater, SfxId::StepWater},
  };
  for (const auto& [bit, id] : kMap)
    if (bits & bit) device_play(id);
}

void poll_gamepad(bool demo, bool nogamepad, bool hanging, double now_in, const ew::game::Bindings& bind,
                  ew::game::Input& in, uint32_t& pad_buttons, bool& pad_start, bool& pad_a, bool& pad_b,
                  bool& pad_du, bool& pad_dd, bool& pad_dl, bool& pad_dr) {
  if (demo || nogamepad) return;
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
    pad_a = raw.a;
    pad_b = raw.b;
    pad_start = st.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
    raw.x = st.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
    raw.y = st.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
    raw.lb = st.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
    raw.rb = st.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    raw.du = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS;
    raw.dd = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS;
    raw.dl = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS;
    raw.dr = st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS;
    pad_du = raw.du;
    pad_dd = raw.dd;
    pad_dl = raw.dl;
    pad_dr = raw.dr;
    for (int bt = 0; bt <= GLFW_GAMEPAD_BUTTON_LAST; ++bt)
      if (st.buttons[bt] == GLFW_PRESS) raw.buttons |= 1u << unsigned(bt);
    pad_buttons = raw.buttons;
    found = true;
    break;
  }
  static ew::game::PadEdges pad_edges;
  static bool pad_announced = false;
  if (found) {
    if (!pad_announced) {
      std::printf("echo_engine: gamepad\n");
      pad_announced = true;
    }
    ew::game::apply_pad(raw, in, hanging, pad_edges, now_in, ew::game::Sim::kRestartFullHold, &bind);
  }
}

}  // namespace

int main(int argc, char** argv) {
  int scale = 4, max_frames = 0;
  bool demo = false, has_start = false, has_hero = false, nosound = false, nogamepad = false, nosave = false,
       explicit_map = false;
  ew::game::HeroKind cli_hero = ew::game::HeroKind::Lantern;
  float start_x = 0, start_y = 0;
  std::string shader_dir = EW_SHADER_DIR, map_path = EW_DEFAULT_MAP, dump_path, atlas_path, sprites_path,
              next_map_override;
  if (const char* e = std::getenv("EW_ATLAS")) atlas_path = e;
  if (const char* e = std::getenv("EW_SPRITES")) sprites_path = e;
  if (const char* e = std::getenv("EW_NOSOUND"); e && e[0] && e[0] != '0') nosound = true;
  if (const char* e = std::getenv("EW_NOGAMEPAD"); e && e[0] && e[0] != '0') nogamepad = true;
  if (const char* e = std::getenv("EW_NOSAVE"); e && e[0] && e[0] != '0') nosave = true;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--scale" && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (a == "--frames" && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (a == "--shaders" && i + 1 < argc) shader_dir = argv[++i];
    else if (a == "--map" && i + 1 < argc) { map_path = argv[++i]; explicit_map = true; }
    else if (a == "--next-map" && i + 1 < argc) next_map_override = argv[++i];
    else if (a == "--dump" && i + 1 < argc) dump_path = argv[++i];
    else if (a == "--atlas" && i + 1 < argc) atlas_path = argv[++i];
    else if (a == "--sprites" && i + 1 < argc) sprites_path = argv[++i];
    else if (a == "--demo") demo = true;
    else if (a == "--hero" && i + 1 < argc) {
      if (!ew::game::hero_from_arg(argv[++i], cli_hero)) {
        std::fprintf(stderr, "echo_engine: héroe desconocido '%s' (usa farol|zarigueya|castor)\n", argv[i]);
        return 1;
      }
      has_hero = true;
    }
    else if (a == "--nosound") nosound = true;
    else if (a == "--nogamepad") nogamepad = true;
    else if (a == "--nosave") nosave = true;
    else if (a == "--version") { std::printf("echo_engine v%s\n", EW_VERSION); return 0; }
    else if (a == "--start" && i + 2 < argc) {
      start_x = float(std::atof(argv[++i]));
      start_y = float(std::atof(argv[++i]));
      has_start = true;
    }
  }
  if (scale < 1) scale = 1;

  const std::string root = ew::core::exe_dir(argv[0]);
  const std::string build_maps_dir = ew::core::parent_path(EW_DEFAULT_MAP);
  if (shader_dir == EW_SHADER_DIR) {
    const auto vert = ew::core::pick_asset(root, "assets/shaders/fullscreen.vert",
                                           std::string(EW_SHADER_DIR) + "/fullscreen.vert");
    shader_dir = ew::core::parent_path(vert);
  }
  if (map_path == EW_DEFAULT_MAP)
    map_path = ew::core::pick_asset(root, "assets/maps/bin/world.ewmap", EW_DEFAULT_MAP);
  if (atlas_path.empty()) atlas_path = ew::core::pick_asset(root, "assets/atlas/tiles.png", EW_DEFAULT_ATLAS);
  if (sprites_path.empty()) sprites_path = ew::core::pick_asset(root, "assets/atlas/sprites.png", EW_DEFAULT_SPRITES);

  ew::game::LevelFlow flow;
  std::string err;
  ew::game::LevelFlowConfig cfg{map_path, next_map_override, explicit_map, root, build_maps_dir,
                                has_start, start_x, start_y};
  if (!flow.init(cfg, err)) {
    std::fprintf(stderr, "echo_engine: %s\n", err.c_str());
    return 1;
  }
  std::printf("echo_engine v%s\n", EW_VERSION);
  std::printf("echo_engine: mapa %s (%dx%d px, %zu entidades)\n", flow.map_path().c_str(), flow.scene().width_px(),
              flow.scene().height_px(), flow.scene().entities().size());
  if (flow.scene().find_type("goal")) std::printf("echo_engine: hay una meta — llegá para completar el camino\n");
  if (flow.scene().find_type("checkpoint"))
    std::printf("echo_engine: R corto vuelve al checkpoint, R largo (~0.8 s) al inicio\n");

  glfwSetErrorCallback([](int c, const char* d) { std::fprintf(stderr, "glfw %d: %s\n", c, d); });
  if (!glfwInit()) return 1;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  GLFWwindow* win = glfwCreateWindow(ew::game::kInternalW * scale, ew::game::kInternalH * scale,
                                     "Echo Well v" EW_VERSION, nullptr, nullptr);
  if (!win) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  if (const char* miss = ew::gl::load([](const char* n) { return reinterpret_cast<void*>(glfwGetProcAddress(n)); })) {
    std::fprintf(stderr, "echo_engine: falta %s (se requiere GL 3.3 core)\n", miss);
    return 1;
  }
  std::printf("echo_engine: GL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

  ew::render::DeferredRenderer renderer;
  if (!renderer.init(shader_dir, err, atlas_path, sprites_path)) {
    std::fprintf(stderr, "echo_engine: %s\n", err.c_str());
    return 1;
  }
  ew::audio::Mixer mixer;
  if (!nosound) {
    if (ew::audio::device_start(mixer)) std::printf("echo_engine: SFX on\n");
  } else {
    std::printf("echo_engine: SFX off (--nosound)\n");
  }

  ew::game::PauseMenu menu;
  ew::game::EndScreen end_screen;
  ew::game::DialogueBox dialogue;
  ew::game::HudTracker hud;
  const bool explicit_dir = std::getenv("EW_SAVE_DIR") != nullptr;  // NOLINT(concurrency-mt-unsafe)
  {
    std::vector<std::string> logs;
    flow.init_save(nosave, demo, has_start, max_frames, explicit_dir, menu, &logs);
    for (const auto& line : logs) std::printf("%s\n", line.c_str());
  }
  if (has_hero) {
    menu.set_hero(cli_hero);
    flow.sim().set_hero(cli_hero);
  } else if (!demo && !max_frames && !has_start && !menu.has_save()) {
    menu.open_hero_select(true);
  }

  int frames = 0, sim_steps = 0;
  bool jump_was_down = false, jump_up_was_down = false, act_was_down = false, grab_was_down = false;
  bool dash_was_down = false, r_was_down = false, r_full_sent = false;
  bool esc_was = false, start_was = false, m_up_was = false, m_dn_was = false, m_lt_was = false, m_rt_was = false,
       m_ok_was = false, m_b_was = false;
  double acc = 0, last = glfwGetTime(), r_down_since = -1;
  uint32_t pad_prev = 0;
  ew::game::Input latched_in;
  glfwSetKeyCallback(win, [](GLFWwindow*, int k, int, int action, int) { if (action == GLFW_PRESS) g_last_key = k; });

  const double t_loop = glfwGetTime();
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    auto key = [&](int k) { return glfwGetKey(win, k) == GLFW_PRESS; };
    const auto& bind = menu.bindings();
    auto act = [&](int a) {
      return bind.key_down(a, [](int k, void* w) { return glfwGetKey(static_cast<GLFWwindow*>(w), k) == GLFW_PRESS; },
                           win);
    };

    ew::game::Input in;
    in.move_x = float(int(act(ew::game::kBindRight)) - int(act(ew::game::kBindLeft)));
    const bool hanging = flow.sim().on_rope();
    if (hanging) in.climb = float(int(act(ew::game::kBindDown)) - int(act(ew::game::kBindUp)));
    const bool j_key = act(ew::game::kBindJump);
    const bool u_key = !hanging && act(ew::game::kBindUp);
    in.jump = (j_key && !jump_was_down) || (u_key && !jump_up_was_down);
    jump_was_down = j_key;
    jump_up_was_down = u_key;
    const bool act_key = act(ew::game::kBindAct);
    const bool dash_key = key(GLFW_KEY_K) || key(GLFW_KEY_RIGHT_SHIFT);
    const bool act_down = act_key || (!hanging && act(ew::game::kBindDown));
    in.interact = act_down && !act_was_down;
    act_was_down = act_down;
    const bool dash_down = act_key || dash_key;
    in.dash = dash_down && !dash_was_down;
    dash_was_down = dash_down;
    const bool grab_down = act(ew::game::kBindGrab);
    in.grab = grab_down && !grab_was_down;
    grab_was_down = grab_down;
    const bool r_down = act(ew::game::kBindRestart);
    const double now_in = glfwGetTime();
    if (r_down) {
      if (r_down_since < 0) r_down_since = now_in;
      if (!r_full_sent && (now_in - r_down_since) >= ew::game::Sim::kRestartFullHold) {
        in.restart_full = true;
        r_full_sent = true;
      }
    } else {
      if (r_was_down && !r_full_sent) in.restart = true;
      r_down_since = -1;
      r_full_sent = false;
    }
    r_was_down = r_down;

    uint32_t pad_buttons = 0;
    bool pad_start = false, pad_a = false, pad_b = false, pad_du = false, pad_dd = false, pad_dl = false, pad_dr = false;
    poll_gamepad(demo, nogamepad, hanging, now_in, bind, in, pad_buttons, pad_start, pad_a, pad_b, pad_du, pad_dd,
                 pad_dl, pad_dr);
    if (demo) {  // camina ida y vuelta y salta cada ~1.5 s
      in.move_x = (sim_steps / 150) % 2 ? -1.f : 1.f;
      in.jump = sim_steps % 90 == 45;
      in.interact = sim_steps % 300 == 43;
    }

    ew::game::MenuInput mi, ei;
    if (!demo) {
      const bool esc_down = key(GLFW_KEY_ESCAPE);
      bool nav_toggle = (esc_down && !esc_was) || (pad_start && !start_was);
      esc_was = esc_down;
      start_was = pad_start;
      if (menu.open() && !menu.waiting() && pad_b && !m_b_was) nav_toggle = true;
      m_b_was = pad_b;
      const bool up = key(GLFW_KEY_UP) || key(GLFW_KEY_W) || pad_du;
      const bool dn = key(GLFW_KEY_DOWN) || key(GLFW_KEY_S) || pad_dd;
      const bool lt = key(GLFW_KEY_LEFT) || key(GLFW_KEY_A) || pad_dl;
      const bool rt = key(GLFW_KEY_RIGHT) || key(GLFW_KEY_D) || pad_dr;
      const bool ok = key(GLFW_KEY_ENTER) || key(GLFW_KEY_SPACE) || key(GLFW_KEY_Z) || pad_a;
      const bool nav_up = up && !m_up_was, nav_dn = dn && !m_dn_was;
      const bool nav_lt = lt && !m_lt_was, nav_rt = rt && !m_rt_was, nav_ok = ok && !m_ok_was;
      m_up_was = up; m_dn_was = dn; m_lt_was = lt; m_rt_was = rt; m_ok_was = ok;
      if (menu.open()) {
        mi.toggle = nav_toggle;
        if (menu.waiting()) {
          if (g_last_key != GLFW_KEY_ESCAPE) mi.key = g_last_key;
          const uint32_t fresh = pad_buttons & ~pad_prev;
          for (int bt = 0; bt <= GLFW_GAMEPAD_BUTTON_LAST && mi.key < 0; ++bt)
            if ((fresh >> unsigned(bt)) & 1u) { mi.pad_button = bt; break; }
        }
        mi.up = nav_up; mi.down = nav_dn; mi.left = nav_lt; mi.right = nav_rt; mi.confirm = nav_ok;
      } else if (dialogue.open()) {
        if (nav_toggle) dialogue.close();
        else if (nav_ok || in.interact) dialogue.advance();
      } else if (end_screen.open()) {
        if (nav_toggle) ei.toggle = true;
        else if (flow.sim().finish_t() >= 0.35f) { ei.up = nav_up; ei.down = nav_dn; ei.confirm = nav_ok; }
      } else {
        mi.toggle = nav_toggle;
      }
    }
    g_last_key = -1;
    pad_prev = pad_buttons;

    const auto mact = menu.handle(mi);
    if (mact == ew::game::MenuAction::Quit) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (mact == ew::game::MenuAction::Continue && !flow.save_path().empty()) {
      std::string lerr;
      if (flow.continue_save(menu, end_screen, dialogue, hud, lerr)) {
        if (flow.consume_tiles_dirty()) renderer.invalidate_tiles();
        glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
        std::printf("echo_engine: partida cargada (checkpoint %d, huevos %d/%d)\n", flow.sim().checkpoint_index(),
                    flow.sim().collected(), flow.sim().collectible_total());
      } else {
        std::fprintf(stderr, "echo_engine: no se pudo cargar la partida (%s)\n", lerr.c_str());
      }
    }
    if (mact == ew::game::MenuAction::Restart) {
      ew::game::Input rst;
      rst.restart = true;
      flow.sim().step(rst);
      ++sim_steps;
    }
    const auto eact = end_screen.handle(ei);
    if (eact == ew::game::EndAction::Quit) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (eact == ew::game::EndAction::OpenMenu) menu.open_at(ew::game::PauseMenu::kSeguir);
    if (eact == ew::game::EndAction::RestartLevel) {
      flow.restart_level(end_screen, dialogue);
      glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
      std::printf("echo_engine: reinicio\n");
    }
    if (eact == ew::game::EndAction::NextLevel) {
      std::string lerr;
      if (flow.next_level(menu, end_screen, dialogue, hud, lerr)) {
        if (!flow.save_path().empty()) {
          if (flow.last_save_ok()) menu.set_has_save(true);
          else std::fprintf(stderr, "echo_engine: no se pudo guardar en %s\n", flow.save_path().c_str());
        }
        if (flow.consume_tiles_dirty()) renderer.invalidate_tiles();
        glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
        std::printf("echo_engine: mapa %s (%dx%d px, %zu entidades)\n", flow.map_path().c_str(),
                    flow.scene().width_px(), flow.scene().height_px(), flow.scene().entities().size());
      } else if (!lerr.empty() && lerr != "no hay siguiente nivel") {
        std::fprintf(stderr, "echo_engine: %s\n", lerr.c_str());
      }
    }

    mixer.set_gain(menu.volume_setting() * (menu.open() ? 0.35f : 1.0f));
    mixer.set_mute(menu.muted());
    flow.sim().set_wind_enabled(menu.foliage());
    flow.sim().set_impact_fx_enabled(menu.impact_fx());
    flow.sim().set_hero(menu.hero());

    const double now = glfwGetTime();
    if (menu.open() || dialogue.open()) {
      acc = 0;
      latched_in = {};
    } else if (end_screen.open()) {
      acc += max_frames ? ew::game::Sim::kDt : now - last;
      in = {};
      latched_in = {};
    } else {
      acc += max_frames ? ew::game::Sim::kDt : (now - last) * double(flow.sim().victory_seq().time_scale());
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
      flow.sim().step(in);
      if (!demo) {
        if (const int dlg_id = flow.sim().take_dialog();
            dlg_id >= 0 && std::size_t(dlg_id) < flow.scene().dialogs().size()) {
          const auto& d = flow.scene().dialogs()[std::size_t(dlg_id)];
          dialogue.open(d.speaker, d.text);
        }
      }
      if (!demo && !max_frames && !flow.sim().finished()) {
        ew::game::BossVictoryDialog vdlg;
        if (flow.sim().take_victory_dialog(vdlg)) dialogue.open(vdlg.speaker, vdlg.text);
      }
      in.jump = in.interact = in.grab = in.dash = in.restart = in.restart_full = false;
      latched_in = {};
      ++sim_steps;
    }
    if (!nosound) {
      play_sfx(flow.sim().take_sfx());
      ew::audio::device_set_music_zone(demo ? ew::audio::MusicZone::Silence
                                            : ew::game::music_zone_for(flow.scene(), flow.sim()));
    }
    if (acc > ew::game::Sim::kDt) acc = 0;

    const auto post = flow.post_step(menu, end_screen, dialogue, did_restart);
    if (post.opened_end) {
      std::printf("echo_engine: ¡meta! camino completado  (R corto: checkpoint, R largo: inicio)\n");
      glfwSetWindowTitle(win, "Echo Well — ¡meta!  [R] checkpoint, hold inicio");
    }
    if (post.cleared_end) {
      glfwSetWindowTitle(win, "Echo Well v" EW_VERSION);
      std::printf("echo_engine: reinicio\n");
    }
    if (post.save_failed)
      std::fprintf(stderr, "echo_engine: no se pudo guardar en %s\n", flow.save_path().c_str());

    ew::game::FrameData v;
    const bool overlay_open = dialogue.open() || menu.open() || end_screen.open();
    const int preview_hero = menu.in_hero_select() ? menu.hero_cursor() : -1;
    ew::game::build_world_frame(v, flow.scene(), flow.sim(), float(now), demo, overlay_open, false, preview_hero);
    const auto hev = ew::game::populate_hud_and_overlays(v, flow.scene(), flow.sim(), menu, end_screen, dialogue, hud,
                                                         EW_VERSION);
    if (hev.new_item) std::printf("echo_engine: ¡huevo! %d/%d\n", v.items, v.items_total);
    if (hev.new_double_jump) std::printf("echo_engine: ¡habilidad desbloqueada! doble salto\n");
    if (hev.new_dash) std::printf("echo_engine: ¡habilidad desbloqueada! embestida (dash)\n");
    if (hev.new_wall_climb) std::printf("echo_engine: ¡habilidad desbloqueada! trepar paredes\n");

    int fw = 0, fh = 0;
    glfwGetFramebufferSize(win, &fw, &fh);
    renderer.render(static_cast<float>(glfwGetTime()), fw, fh, v);
    glfwSwapBuffers(win);
    if (max_frames && ++frames >= max_frames) break;
  }

  if (!dump_path.empty() && !renderer.dump_ppm(dump_path))
    std::fprintf(stderr, "echo_engine: no se pudo escribir %s\n", dump_path.c_str());
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
