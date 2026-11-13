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
    // Fila = familia (id/16), columna = máscara 4 vecinos (N=1,E=2,S=4,W=8).
    const int family = id / 16;
    const int mask = id % 16;
    const bool cn = mask & 1, ce = mask & 2, cs = mask & 4, cw = mask & 8;  // vecinos
    // Paleta / estilo por familia (los 16 masks de una familia se ven iguales por dentro).
    float h = float(family % 7) / 7.0f;
    float br = 0.24f + (0.42f - 0.24f) * h, bg = 0.30f + (0.34f - 0.30f) * h, bb = 0.36f + (0.30f - 0.36f) * h;
    if (family == 0) { br = 0.32f; bg = 0.30f; bb = 0.38f; }
    const int style = family % 4;  // 0 liso, 1 ladrillo, 2 granulado, 3 vetas
    for (int y = 0; y < kTilePx; ++y)
      for (int x = 0; x < kTilePx; ++x) {
        float k = 1.0f, height = 1.0f;
        const float n = float(hash(uint32_t(family * 64 + y * 8 + x)) & 0xff) / 255.0f;
        switch (style) {
          case 1: {  // ladrillo: juntas en y=3,7 y x alternado
            bool joint = (y == 3 || y == 7) || (y < 3 ? x == 3 : x == 7);
            if (joint) { k = 0.65f; height = 0.4f; } else k = 0.95f + 0.1f * n;
          } break;
          case 2: k = 0.85f + 0.3f * n; height = 0.7f + 0.3f * n; break;
          case 3: if ((x + y + family) % 5 == 0) { k = 1.2f; height = 0.8f; } else k = 0.95f; break;
          default: k = 0.97f + 0.06f * n; break;
        }
        // Bisel solo en lados expuestos (sin vecino en esa dirección).
        if ((!cw && x == 0) || (!cn && y == 0)) k *= 1.12f;
        if ((!ce && x == kTilePx - 1) || (!cs && y == kTilePx - 1)) { k *= 0.78f; height *= 0.5f; }
        uint8_t* p = &px[(std::size_t(o.y + y) * kAtlasPx + (o.x + x)) * 4];
        p[0] = c8(br * k); p[1] = c8(bg * k); p[2] = c8(bb * k); p[3] = c8(height);
      }
  }
  return px;
}
}  // namespace ew::render

#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 256
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fstream>

namespace ew::render {
namespace {
AtlasLoad procedural() { return {build_tile_atlas(), AtlasSource::Procedural}; }

// Convierte el buffer de stbi (req_comp = 4) al layout del atlas.
AtlasLoad from_rgba(const uint8_t* img, int w, int h) {
  if (!img || w != kAtlasPx || h != kAtlasPx) return procedural();
  AtlasLoad a;
  a.px.assign(img, img + std::size_t(kAtlasPx) * kAtlasPx * 4);
  a.source = AtlasSource::Png;
  return a;
}
}  // namespace

AtlasLoad load_tile_atlas_mem(const void* data, std::size_t n) {
  if (!data || n < 8) return procedural();  // un PNG válido es más largo
  int w = 0, h = 0, comp = 0;
  unsigned char* img = stbi_load_from_memory(static_cast<const stbi_uc*>(data), int(n > 0x7fffffff ? 0x7fffffff : n),
                                             &w, &h, &comp, 4);
  AtlasLoad a = from_rgba(img, w, h);
  if (img) stbi_image_free(img);
  return a;
}

AtlasLoad load_tile_atlas_file(const char* path) {
  if (!path || !*path) return procedural();
  std::ifstream f(path, std::ios::binary);
  if (!f) return procedural();
  f.seekg(0, std::ios::end);
  const auto sz = f.tellg();
  if (sz <= 0 || sz > 4 * 1024 * 1024) return procedural();  // tope 4 MiB
  f.seekg(0);
  std::vector<unsigned char> buf(static_cast<std::size_t>(sz));
  if (!f.read(reinterpret_cast<char*>(buf.data()), sz)) return procedural();
  return load_tile_atlas_mem(buf.data(), buf.size());
}
}  // namespace ew::render
