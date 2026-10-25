// echo_engine: ventana + loop + Deferred 320x180 sobre un mapa .ewmap.
//   echo_engine [--scale N] [--frames N] [--shaders DIR] [--map FILE] [--dump FILE.ppm]
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "game/scene.hpp"
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
    if (e.type == "switch") push_marker(v, e.x, e.y + 4, 3, 1, 0.9f, 0.75f, 0.2f);
    else if (e.type == "door") push_marker(v, e.x + 4, e.y + 4, 4, 1, 0.55f, 0.35f, 0.75f);
    else if (e.type == "player_spawn") push_marker(v, e.x, e.y + 7, 1, 1, 0.3f, 0.9f, 0.4f);
  }
}
}  // namespace

int main(int argc, char** argv) {
  int scale = 4, max_frames = 0;
  std::string shader_dir = EW_SHADER_DIR, map_path = EW_DEFAULT_MAP, dump_path;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--scale" && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (a == "--frames" && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (a == "--shaders" && i + 1 < argc) shader_dir = argv[++i];
    else if (a == "--map" && i + 1 < argc) map_path = argv[++i];
    else if (a == "--dump" && i + 1 < argc) dump_path = argv[++i];
  }
  if (scale < 1) scale = 1;

  ew::game::Scene scene;
  std::string err;
  if (!scene.load(map_path, err)) { std::fprintf(stderr, "echo_engine: %s\n", err.c_str()); return 1; }
  const auto* spawn = scene.find_type("player_spawn");
  float px = spawn ? spawn->x : 16.f, py = spawn ? spawn->y : 16.f;
  std::printf("echo_engine: mapa %s (%dx%d px, %zu entidades)\n", map_path.c_str(), scene.width_px(),
              scene.height_px(), scene.entities().size());

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

  int frames = 0;
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    // Movimiento libre del marker (se reemplaza por física Verlet), sin atravesar COL.
    const float sp = 1.5f;
    float dx = float(glfwGetKey(win, GLFW_KEY_RIGHT) - glfwGetKey(win, GLFW_KEY_LEFT)) * sp;
    float dy = float(glfwGetKey(win, GLFW_KEY_DOWN) - glfwGetKey(win, GLFW_KEY_UP)) * sp;
    if (!scene.solid_px(px + dx, py)) px += dx;
    if (!scene.solid_px(px, py + dy)) py += dy;
    scene.follow(px, py);

    FrameView v;
    v.cam_x = scene.cam_x(); v.cam_y = scene.cam_y();
    v.chunk_x_px = float(scene.chunk_x() * ew::map::kChunkW * ew::map::kTileSize);
    v.chunk_y_px = float(scene.chunk_y() * ew::map::kChunkH * ew::map::kTileSize);
    v.tiles = scene.active_tiles().data();
    v.tiles_version = scene.chunk_version();
    push_entities(v, scene);
    push_marker(v, px, py, 4, 0, 0.95f, 0.95f, 0.9f);  // player
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
