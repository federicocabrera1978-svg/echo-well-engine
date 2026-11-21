#include "render/sprite_atlas.hpp"

#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 256
#include "stb_image.h"

#include <algorithm>
#include <fstream>

namespace ew::render {
namespace {
struct Color { uint8_t r, g, b, a; };

void put_px(std::vector<uint8_t>& px, int cx, int cy, int x, int y, Color c) {
  if (x < 0 || y < 0 || x >= kSpriteCellPx || y >= kSpriteCellPx) return;
  const int gx = cx * kSpriteCellPx + x;
  const int gy = cy * kSpriteCellPx + y;
  uint8_t* p = &px[(std::size_t(gy) * kSpriteAtlasPx + std::size_t(gx)) * 4];
  p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}

void fill_rect(std::vector<uint8_t>& px, int cx, int cy, int x0, int y0, int w, int h, Color c) {
  for (int y = y0; y < y0 + h; ++y)
    for (int x = x0; x < x0 + w; ++x)
      put_px(px, cx, cy, x, y, c);
}

void draw_hero_pose(std::vector<uint8_t>& px, int col, int row) {
  constexpr Color kOutline{20, 18, 34, 140};
  constexpr Color kCapeDark{46, 44, 78, 178};
  constexpr Color kCapeMid{70, 68, 114, 210};
  constexpr Color kCapeHigh{104, 102, 158, 232};
  constexpr Color kFace{16, 14, 26, 156};
  constexpr Color kEye{255, 224, 130, 250};
  constexpr Color kScarf{136, 56, 52, 198};
  constexpr Color kBoot{84, 54, 40, 192};
  constexpr Color kMetal{128, 136, 152, 215};
  constexpr Color kFlame{255, 172, 54, 248};
  constexpr Color kCore{255, 246, 182, 255};
  constexpr Color kWing{46, 186, 232, 244};
  constexpr Color kWingHi{170, 246, 255, 255};

  const int bob = (row == 0 && col == 2) || (row == 1 && (col == 1 || col == 3)) ? 1 : 0;
  const int dy = (row == 2 || row == 3) ? -1 : bob;

  // Contorno base y capucha puntiaguda (cabe dentro de x=3..12, y=2..13)
  fill_rect(px, col, row, 4, 2 + dy, 6, 4, kOutline);
  fill_rect(px, col, row, 5, 3 + dy, 4, 2, kCapeMid);
  put_px(px, col, row, 5, 3 + dy, kCapeHigh);
  fill_rect(px, col, row, 7, 4 + dy, 2, 2, kFace);
  put_px(px, col, row, 8, 5 + dy, kEye);

  // Cuerpo / capa + bufanda
  fill_rect(px, col, row, 4, 6 + dy, 6, 5, kOutline);
  fill_rect(px, col, row, 5, 6 + dy, 4, 1, kScarf);
  fill_rect(px, col, row, 5, 7 + dy, 4, 3, (col & 1) ? kCapeMid : kCapeDark);
  if (row == 1 && (col == 0 || col == 2)) put_px(px, col, row, 3, 8 + dy, kCapeDark);

  // Botas según pose/cuadro
  const int stride = (row == 1) ? ((col == 0) ? -1 : (col == 2 ? 1 : 0)) : 0;
  fill_rect(px, col, row, 5 + stride, 11 + dy, 2, 2, kBoot);
  fill_rect(px, col, row, 8 - stride, 11 + dy, 2, 2, kBoot);
  if (row == 4) {  // trepando cuerda: manos arriba alternadas
    put_px(px, col, row, 7, 1 + (col & 1), kMetal);
    put_px(px, col, row, 8, 1 + ((col + 1) & 1), kMetal);
  }
  if (row == 5) {  // nadando: reflejos de agua
    fill_rect(px, col, row, 2 + (col & 1), 10, 3, 1, kWingHi);
    fill_rect(px, col, row, 10 - (col & 1), 10, 2, 1, kWing);
  }

  // Farol en la mano derecha
  const int lx = 11 + (row == 1 && col == 2 ? 1 : 0);
  const int ly = 7 + dy - (row == 2 ? 2 : 0);
  put_px(px, col, row, lx, ly, kMetal);
  put_px(px, col, row, lx, ly + 1, (col & 1) ? kCore : kFlame);
  put_px(px, col, row, lx, ly + 2, (col & 2) ? kFlame : kCore);

  // Alas dracónicas cian para Doble Salto (row 3)
  if (row == 3) {
    const int spread = 3 - (col & 1);
    for (int i = 0; i <= spread; ++i) {
      put_px(px, col, row, 3 - (i / 2), 3 + i, (col & 1) ? kWingHi : kWing);
      put_px(px, col, row, 2, 4 + i, kWing);
    }
  }
}

void draw_egg_frame(std::vector<uint8_t>& px, int col) {
  constexpr Color kOutline{30, 16, 20, 145};
  constexpr Color kDark{118, 34, 32, 184};
  constexpr Color kRed{176, 56, 44, 214};
  constexpr Color kGold{222, 156, 60, 232};
  constexpr Color kEmber{255, 192, 74, 250};
  constexpr Color kSpark{255, 242, 172, 255};
  constexpr int row = int(SpriteRow::Items);

  fill_rect(px, col, row, 5, 3, 6, 9, kOutline);
  fill_rect(px, col, row, 4, 5, 8, 6, kOutline);
  fill_rect(px, col, row, 6, 4, 4, 7, kDark);
  fill_rect(px, col, row, 5, 6, 6, 4, kRed);
  put_px(px, col, row, 6, 5, kGold);
  put_px(px, col, row, 9, 8, kGold);
  put_px(px, col, row, 7 + (col & 1), 6, kEmber);
  put_px(px, col, row, 8 - ((col >> 1) & 1), 7, kSpark);
}

void draw_relic_frame(std::vector<uint8_t>& px, int col) {
  constexpr Color kOutline{18, 32, 50, 145};
  constexpr Color kCyanDark{42, 142, 196, 204};
  constexpr Color kCyan{88, 216, 248, 232};
  constexpr Color kEdge{208, 246, 255, 248};
  constexpr Color kGem{116, 248, 255, 250};
  constexpr Color kCore{244, 255, 255, 255};
  constexpr int row = int(SpriteRow::Items);

  const int wing_y = 3 + (col & 1);
  fill_rect(px, col, row, 6, 5, 4, 6, kOutline);
  fill_rect(px, col, row, 2, wing_y, 3, 4, kCyanDark);
  fill_rect(px, col, row, 11, wing_y, 3, 4, kCyanDark);
  put_px(px, col, row, 2, wing_y, kEdge);
  put_px(px, col, row, 13, wing_y, kEdge);
  fill_rect(px, col, row, 7, 6, 2, 4, kCyan);
  put_px(px, col, row, 7 + (col & 1), 7, kCore);
  put_px(px, col, row, 8 - (col & 1), 8, (col & 2) ? kCore : kGem);
}

AtlasLoad procedural() { return {build_sprite_atlas(), AtlasSource::Procedural}; }

AtlasLoad from_rgba(const uint8_t* img, int w, int h) {
  if (!img || w != kSpriteAtlasPx || h != kSpriteAtlasPx) return procedural();
  AtlasLoad a;
  a.px.assign(img, img + std::size_t(kSpriteAtlasPx) * kSpriteAtlasPx * 4);
  a.source = AtlasSource::Png;
  return a;
}
}  // namespace

std::vector<uint8_t> build_sprite_atlas() {
  std::vector<uint8_t> px(std::size_t(kSpriteAtlasPx) * kSpriteAtlasPx * 4, 0);
  for (int row = 0; row <= int(SpriteRow::Swim); ++row)
    for (int col = 0; col < 4; ++col)
      draw_hero_pose(px, col, row);
  for (int col = 0; col < 4; ++col) draw_egg_frame(px, col);
  for (int col = 4; col < 8; ++col) draw_relic_frame(px, col);
  return px;
}

AtlasLoad load_sprite_atlas_mem(const void* data, std::size_t n) {
  if (!data || n < 8) return procedural();
  int w = 0, h = 0, comp = 0;
  unsigned char* img = stbi_load_from_memory(
      static_cast<const stbi_uc*>(data), int(n > 0x7fffffff ? 0x7fffffff : n), &w, &h, &comp, 4);
  AtlasLoad a = from_rgba(img, w, h);
  if (img) stbi_image_free(img);
  return a;
}

AtlasLoad load_sprite_atlas_file(const char* path) {
  if (!path || !*path) return procedural();
  std::ifstream f(path, std::ios::binary);
  if (!f) return procedural();
  f.seekg(0, std::ios::end);
  const auto sz = f.tellg();
  if (sz <= 0 || sz > 4 * 1024 * 1024) return procedural();
  f.seekg(0);
  std::vector<unsigned char> buf(static_cast<std::size_t>(sz));
  if (!f.read(reinterpret_cast<char*>(buf.data()), sz)) return procedural();
  return load_sprite_atlas_mem(buf.data(), buf.size());
}
}  // namespace ew::render
