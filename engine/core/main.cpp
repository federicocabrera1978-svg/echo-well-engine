// echo_engine: ventana + loop + stub Deferred 320x180.
//   echo_engine [--scale N] [--frames N] [--shaders DIR]
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "render/deferred.hpp"

#ifndef EW_SHADER_DIR
#define EW_SHADER_DIR "assets/shaders"
#endif

int main(int argc, char** argv) {
  int scale = 4, max_frames = 0;
  std::string shader_dir = EW_SHADER_DIR;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--scale" && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (a == "--frames" && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (a == "--shaders" && i + 1 < argc) shader_dir = argv[++i];
  }
  if (scale < 1) scale = 1;

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
  std::string err;
  if (!renderer.init(shader_dir, err)) { std::fprintf(stderr, "echo_engine: %s\n", err.c_str()); return 1; }

  int frames = 0;
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    int fw, fh; glfwGetFramebufferSize(win, &fw, &fh);
    renderer.render(static_cast<float>(glfwGetTime()), fw, fh);
    glfwSwapBuffers(win);
    if (max_frames && ++frames >= max_frames) break;
  }
  std::printf("echo_engine: %d frames, salida limpia\n", frames);
  renderer.shutdown();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
