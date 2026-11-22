#pragma once
#include <string>
#include "game/frame_view.hpp"
#include "render/gl_loader.hpp"

namespace ew::render {
using ew::game::kInternalH;
using ew::game::kInternalW;
using ew::game::kMaxBosses;
using ew::game::kMaxHazards;
using ew::game::kMaxMarkers;
using ew::game::kMaxMovers;
using ew::game::kMaxSprites;
using ew::game::kMaxWaterBodies;
using ew::game::kMaxWaterCols;
using ew::game::kTilesH;
using ew::game::kTilesW;

using FrameView = ew::game::FrameData;


// G-buffer a 320x180: albedo (RGBA8), normal (RGBA8, n*0.5+0.5) + depth24.
// Pase de luz -> target "lit" 320x180 -> blit escalado entero (NEAREST) a pantalla.
class DeferredRenderer {
 public:
  // atlas_path / sprite_atlas_path vacíos o inválidos: procedural. PNG 128×128 válido: los usa.
  bool init(const std::string& shader_dir, std::string& err, const std::string& atlas_path = {},
            const std::string& sprite_atlas_path = {});
  void shutdown();
  // Vuelca el target "lit" 320×180 a un PPM (debug / verificación headless).
  bool dump_ppm(const std::string& path) const;
  // Fuerza re-subida de la textura de tiles en el próximo frame (p. ej. al cambiar de nivel).
  void invalidate_tiles() { tiles_version_ = 0; }
  void render(float time_s, int fb_w, int fb_h, const FrameView& view);

 private:
  GLuint gbuf_fbo_ = 0, albedo_ = 0, normal_ = 0, depth_ = 0;
  GLuint lit_fbo_ = 0, lit_tex_ = 0;
  GLuint tiles_tex_ = 0, atlas_tex_ = 0, sprite_tex_ = 0; uint32_t tiles_version_ = 0;
  GLuint vao_ = 0, geom_prog_ = 0, light_prog_ = 0;
};
}  // namespace ew::render
