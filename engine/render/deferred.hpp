#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "render/gl_loader.hpp"

namespace ew::render {
inline constexpr int kInternalW = 320;
inline constexpr int kInternalH = 180;

inline constexpr int kMaxMarkers = 48;
inline constexpr int kMaxSprites = 16;
inline constexpr int kMaxHazards = 32;
inline constexpr int kMaxWaterCols = 64;
inline constexpr int kMaxWaterBodies = 4;
inline constexpr int kMaxMovers = 8;
inline constexpr int kTilesW = 120, kTilesH = 69;  // 3×3 chunks de 40×23

// Lo que el juego le pasa al renderer cada frame (coords de mundo en px, y hacia abajo).
struct FrameView {
  float cam_x = 0, cam_y = 0;
  float chunk_x_px = 0, chunk_y_px = 0;      // origen del vecindario 3×3 (chunk activo - 1)
  const uint8_t* tiles = nullptr;            // kTilesW×kTilesH RGB8 (R=COL, G=VIS, B=FG), 3×3 chunks
  uint32_t tiles_version = 0;                // re-sube la textura si cambia
  // Marcadores: (x, y, radio, forma 0=círculo 1=cuadrado) + color RGB.
  int marker_count = 0;
  std::array<float, kMaxMarkers * 4> markers{};
  std::array<float, kMaxMarkers * 3> marker_colors{};
  // Sprites pixel-art (16×16 px): (x, y, col_signed, row_signed) + tinte (r, g, b, emit_boost).
  // col_signed >= 0 → col 0..7 normal; col_signed < 0 → espejo horizontal de col = -col_signed - 1.
  // row_signed >= 0 → row 0..7 normal; row_signed < 0 → espejo vertical de row = -row_signed - 1.
  int sprite_count = 0;
  std::array<float, kMaxSprites * 4> sprites{};
  std::array<float, kMaxSprites * 4> sprite_tint{};
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
  std::array<int32_t, 96> ctl_txt{};         // game::pack_text(controls_text(menu))
  // HUD de collectibles (light pass): ícono de huevo + "n/total" arriba a la izquierda.
  // total == 0 → no se dibuja. flash 0..1 = destello al agarrar uno.
  int items = 0, items_total = 0;
  float items_flash = 0;
  // HUD de habilidades (light pass): bitmask kAbility* + destello 0..1 al desbloquear.
  int abilities = 0;
  float ability_flash = 0;
  // Plataformas móviles: rect (x, y, w, h) en px mundo + familia de atlas (1..15).
  int mover_count = 0;
  std::array<float, kMaxMovers * 4> mover_rect{};
  std::array<float, kMaxMovers> mover_fam{};
  // Pinchos pixel-art (META 4): tramos horizontales (x0, y0, w_px, dir: +1 piso, -1 techo).
  int hazard_count = 0;
  std::array<float, kMaxHazards * 4> hazards{};
  // Fondos (backdrops) del vecindario 3×3, [oy*3+ox] desde chunk_x_px/chunk_y_px:
  // tema (ew::map::BackdropTheme) y seed. Default: cueva.
  std::array<int, 9> bg_theme{};
  std::array<int, 9> bg_seed{};
};


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
