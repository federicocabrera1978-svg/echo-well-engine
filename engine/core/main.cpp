// echo_engine: ventana + loop + Deferred 320x180 sobre un mapa .ewmap.
//   echo_engine [--scale N] [--frames N] [--shaders DIR] [--map FILE] [--dump FILE.ppm] [--demo] [--start X Y]
// Controles: ←/→ (o A/D) caminar, Espacio/↑/Z saltar, E/X/↓ activar switch, C/Shift agarrar/soltar cuerda (colgado: ←/→ balancear, ↑/↓ o W/S trepar, Espacio/Z saltar), ESC salir. --demo = input automático.
// Mapa por defecto: slice.ewmap (camino start→goal). hello.ewmap sigue disponible con --map.
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "game/sim.hpp"
#include "render/deferred.hpp"

#ifndef EW_SHADER_DIR
#define EW_SHADER_DIR "assets/shaders"
#endif
#ifndef EW_DEFAULT_MAP
#define EW_DEFAULT_MAP "assets/maps/bin/hello.ewmap"
#endif

namespace {
using ew::render::FrameView;

void push_marker(FrameView& v, float x, float y, float r, int shape, float cr, float cg, float cb) {
  if (v.marker_count >= ew::render::kMaxMarkers) return;
  const int i = v.marker_count++;
  v.markers[i * 4 + 0] = x; v.markers[i * 4 + 1] = y; v.markers[i * 4 + 2] = r; v.markers[i * 4 + 3] = float(shape);
  v.marker_colors[i * 3 + 0] = cr; v.marker_colors[i * 3 + 1] = cg; v.marker_colors[i * 3 + 2] = cb;
}

void push_entities(FrameView& v, const ew::game::Scene& s) {
  for (const auto& e : s.entities()) {
    if (e.type == "switch") {  // base gris + luz: ámbar = apagado, verde = encendido
      push_marker(v, e.x, e.y + 5, 3, 1, 0.35f, 0.35f, 0.4f);
      if (e.active) push_marker(v, e.x, e.y + 2, 2, 0, 0.3f, 1.0f, 0.45f);
      else push_marker(v, e.x, e.y + 2, 2, 0, 1.0f, 0.6f, 0.15f);
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
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  int scale = 4, max_frames = 0;
  bool demo = false, has_start = false;
  float start_x = 0, start_y = 0;
  std::string shader_dir = EW_SHADER_DIR, map_path = EW_DEFAULT_MAP, dump_path;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--scale" && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (a == "--frames" && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (a == "--shaders" && i + 1 < argc) shader_dir = argv[++i];
    else if (a == "--map" && i + 1 < argc) map_path = argv[++i];
    else if (a == "--dump" && i + 1 < argc) dump_path = argv[++i];
    else if (a == "--demo") demo = true;
    else if (a == "--start" && i + 2 < argc) { start_x = float(std::atof(argv[++i])); start_y = float(std::atof(argv[++i])); has_start = true; }
  }
  if (scale < 1) scale = 1;

  ew::game::Scene scene;
  std::string err;
  if (!scene.load(map_path, err)) { std::fprintf(stderr, "echo_engine: %s\n", err.c_str()); return 1; }
  ew::game::Sim sim(scene);
  if (has_start) {  // --start X Y: aparece en otro punto (p. ej. probar el seam/otro chunk)
    auto& p = sim.world().points()[sim.player_index()];
    p.pos = p.prev = {start_x, start_y};
    scene.follow(start_x, start_y);
  }
  std::printf("echo_engine: mapa %s (%dx%d px, %zu entidades)\n", map_path.c_str(), scene.width_px(),
              scene.height_px(), scene.entities().size());
  if (scene.find_type("goal")) std::printf("echo_engine: hay una meta — llegá para completar el camino\n");

  glfwSetErrorCallback([](int c, const char* d) { std::fprintf(stderr, "glfw %d: %s\n", c, d); });
  if (!glfwInit()) return 1;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  GLFWwindow* win = glfwCreateWindow(ew::render::kInternalW * scale, ew::render::kInternalH * scale,
                                     "Echo Well", nullptr, nullptr);
  if (!win) { glfwTerminate(); return 1; }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  glfwSetKeyCallback(win, [](GLFWwindow* w, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(w, GLFW_TRUE);
  });

  if (const char* miss = ew::gl::load([](const char* n) { return reinterpret_cast<void*>(glfwGetProcAddress(n)); })) {
    std::fprintf(stderr, "echo_engine: falta %s (se requiere GL 3.3 core)\n", miss);
    return 1;
  }
  std::printf("echo_engine: GL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

  ew::render::DeferredRenderer renderer;
  if (!renderer.init(shader_dir, err)) { std::fprintf(stderr, "echo_engine: %s\n", err.c_str()); return 1; }

  int frames = 0, sim_steps = 0;
  bool jump_was_down = false, act_was_down = false, grab_was_down = false, announced_goal = false;
  double acc = 0, last = glfwGetTime();
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    auto key = [&](int k) { return glfwGetKey(win, k) == GLFW_PRESS; };
    ew::game::Input in;
    in.move_x = float((key(GLFW_KEY_RIGHT) || key(GLFW_KEY_D)) - (key(GLFW_KEY_LEFT) || key(GLFW_KEY_A)));
    const bool hanging = sim.on_rope();
    // Colgado, ↑/↓ trepan (no saltan ni activan switch).
    if (hanging) in.climb = float((key(GLFW_KEY_DOWN) || key(GLFW_KEY_S)) - (key(GLFW_KEY_UP) || key(GLFW_KEY_W)));
    const bool jump_down = key(GLFW_KEY_SPACE) || key(GLFW_KEY_Z) || (!hanging && key(GLFW_KEY_UP));
    in.jump = jump_down && !jump_was_down;
    jump_was_down = jump_down;
    const bool act_down = key(GLFW_KEY_E) || key(GLFW_KEY_X) || (!hanging && key(GLFW_KEY_DOWN));
    in.interact = act_down && !act_was_down;
    act_was_down = act_down;
    const bool grab_down = key(GLFW_KEY_C) || key(GLFW_KEY_LEFT_SHIFT) || key(GLFW_KEY_RIGHT_SHIFT);
    in.grab = grab_down && !grab_was_down;
    grab_was_down = grab_down;
    if (demo) {  // camina ida y vuelta y salta cada ~1.5 s
      in.move_x = (sim_steps / 150) % 2 ? -1.f : 1.f;
      in.jump = sim_steps % 90 == 45;
      in.interact = sim_steps % 300 == 43;  // ~x=96: pasa por el switch y lo alterna
    }
    // Paso fijo 60 Hz (con --frames: 1 paso por frame, determinista).
    const double now = glfwGetTime();
    acc += max_frames ? ew::game::Sim::kDt : now - last;
    last = now;
    for (int n = 0; acc >= ew::game::Sim::kDt && n < 5; ++n, acc -= ew::game::Sim::kDt) {
      sim.step(in);
      in.jump = false;
      in.interact = false;
      in.grab = false;
      ++sim_steps;
    }
    if (acc > ew::game::Sim::kDt) acc = 0;
    if (sim.finished() && !announced_goal) {
      std::printf("echo_engine: ¡meta! camino completado\n");
      announced_goal = true;
    }
    const auto& pl = sim.player();
    const float px = pl.pos.x, py = pl.pos.y;

    FrameView v;
    v.cam_x = std::round(scene.cam_x()); v.cam_y = std::round(scene.cam_y());  // pixel-perfect
    v.chunk_x_px = scene.nb_origin_x();  // vecindario 3×3 alrededor del chunk activo
    v.chunk_y_px = scene.nb_origin_y();
    v.tiles = scene.active_tiles().data();
    v.tiles_version = scene.chunk_version();
    push_entities(v, scene);
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
      const float f = float(sim.grab_flash()) / float(ew::game::Sim::kGrabFlashFrames);
      push_marker(v, node.pos.x, node.pos.y, 3.f + (1.f - f) * 10.f, 0, 1.0f, 0.9f * f, 0.35f * f);
    }
    push_marker(v, px, py, ew::game::Sim::kPlayerRadius, 0, 0.95f, 0.95f, 0.9f);  // player
    for (const auto& w : sim.water()) {  // hasta kMaxWaterBodies; solo los que tocan la vista
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

    int fw, fh; glfwGetFramebufferSize(win, &fw, &fh);
    renderer.render(static_cast<float>(glfwGetTime()), fw, fh, v);
    glfwSwapBuffers(win);
    if (max_frames && ++frames >= max_frames) break;
  }
  if (!dump_path.empty() && !renderer.dump_ppm(dump_path)) std::fprintf(stderr, "echo_engine: no se pudo escribir %s\n", dump_path.c_str());
  std::printf("echo_engine: %d frames, salida limpia\n", frames);
  renderer.shutdown();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
