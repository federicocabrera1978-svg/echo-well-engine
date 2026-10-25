#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "render/gl_loader.hpp"

namespace ew::render {
inline constexpr int kInternalW = 320;
inline constexpr int kInternalH = 180;

inline constexpr int kMaxMarkers = 48;
inline constexpr int kMaxWaterCols = 64;

// Lo que el juego le pasa al renderer cada frame (coords de mundo en px, y hacia abajo).
struct FrameView {
  float cam_x = 0, cam_y = 0;
  float chunk_x_px = 0, chunk_y_px = 0;      // origen del chunk activo
  const uint8_t* tiles = nullptr;            // 40×23 RG8 (R=COL, G=VIS) del chunk activo
  uint32_t tiles_version = 0;                // re-sube la textura si cambia
  // Marcadores: (x, y, radio, forma 0=círculo 1=cuadrado) + color RGB.
  int marker_count = 0;
  std::array<float, kMaxMarkers * 4> markers{};
  std::array<float, kMaxMarkers * 3> marker_colors{};
  // Agua: rect (x, y_reposo, w, fondo) + altura de superficie por columna (px mundo).
  int water_cols = 0;
  std::array<float, 4> water_rect{};
  std::array<float, kMaxWaterCols> water_surface{};
  float light_x = 160, light_y = 90;         // luz principal (px mundo)
};

// G-buffer a 320x180: albedo (RGBA8), normal (RGBA8, n*0.5+0.5) + depth24.
// Pase de luz -> target "lit" 320x180 -> blit escalado entero (NEAREST) a pantalla.
class DeferredRenderer {
 public:
  bool init(const std::string& shader_dir, std::string& err);
  void shutdown();
  // Vuelca el target "lit" 320×180 a un PPM (debug / verificación headless).
  bool dump_ppm(const std::string& path) const;
  void render(float time_s, int fb_w, int fb_h, const FrameView& view);

 private:
  GLuint gbuf_fbo_ = 0, albedo_ = 0, normal_ = 0, depth_ = 0;
  GLuint lit_fbo_ = 0, lit_tex_ = 0;
  GLuint tiles_tex_ = 0; uint32_t tiles_version_ = 0;
  GLuint vao_ = 0, geom_prog_ = 0, light_prog_ = 0;
};
}  // namespace ew::render
