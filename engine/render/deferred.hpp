#pragma once
#include <string>
#include "render/gl_loader.hpp"

namespace ew::render {
inline constexpr int kInternalW = 320;
inline constexpr int kInternalH = 180;

// G-buffer stub a 320x180: albedo (RGBA8), normal (RGBA8, n*0.5+0.5) + depth24.
// Pase de luz -> target "lit" 320x180 -> blit escalado entero (NEAREST) a pantalla.
class DeferredRenderer {
 public:
  bool init(const std::string& shader_dir, std::string& err);
  void shutdown();
  void render(float time_s, int fb_w, int fb_h);

 private:
  GLuint gbuf_fbo_ = 0, albedo_ = 0, normal_ = 0, depth_ = 0;
  GLuint lit_fbo_ = 0, lit_tex_ = 0;
  GLuint vao_ = 0, geom_prog_ = 0, light_prog_ = 0;
};
}  // namespace ew::render
