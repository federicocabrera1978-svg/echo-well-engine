#include "render/tile_atlas.hpp"

#include <algorithm>

namespace ew::render {
namespace {
uint32_t hash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
uint8_t c8(float v) { return uint8_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); }
}  // namespace

std::vector<uint8_t> build_tile_atlas() {
  std::vector<uint8_t> px(std::size_t(kAtlasPx) * kAtlasPx * 4);
  for (int id = 0; id < 256; ++id) {
    const TileUV o = tile_uv(uint8_t(id));
    // Paleta base: misma familia que el tile_color() previo (piedra fría -> cálida).
    float h = float(id % 7) / 7.0f;
    float br = 0.24f + (0.42f - 0.24f) * h, bg = 0.30f + (0.34f - 0.30f) * h, bb = 0.36f + (0.30f - 0.36f) * h;
    if (id == 0) { br = 0.32f; bg = 0.30f; bb = 0.38f; }
    const int style = id % 4;  // 0 liso, 1 ladrillo, 2 granulado, 3 vetas
    for (int y = 0; y < kTilePx; ++y)
      for (int x = 0; x < kTilePx; ++x) {
        float k = 1.0f, height = 1.0f;
        const float n = float(hash(uint32_t(id * 64 + y * 8 + x)) & 0xff) / 255.0f;
        switch (style) {
          case 1: {  // ladrillo: juntas en y=3,7 y x alternado
            bool joint = (y == 3 || y == 7) || (y < 3 ? x == 3 : x == 7);
            if (joint) { k = 0.65f; height = 0.4f; } else k = 0.95f + 0.1f * n;
          } break;
          case 2: k = 0.85f + 0.3f * n; height = 0.7f + 0.3f * n; break;
          case 3: if ((x + y + id) % 5 == 0) { k = 1.2f; height = 0.8f; } else k = 0.95f; break;
          default: k = 0.97f + 0.06f * n; break;
        }
        // Bisel: borde claro arriba/izq, oscuro abajo/der.
        if (x == 0 || y == 0) k *= 1.12f;
        if (x == kTilePx - 1 || y == kTilePx - 1) { k *= 0.78f; height *= 0.5f; }
        uint8_t* p = &px[(std::size_t(o.y + y) * kAtlasPx + (o.x + x)) * 4];
        p[0] = c8(br * k); p[1] = c8(bg * k); p[2] = c8(bb * k); p[3] = c8(height);
      }
  }
  return px;
}
}  // namespace ew::render
