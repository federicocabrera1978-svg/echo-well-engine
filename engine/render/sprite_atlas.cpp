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
  uint8_t* p = &px[(std::size_t(gy) * kSpriteAtlasW + std::size_t(gx)) * 4];
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

void draw_opossum_pose(std::vector<uint8_t>& px, int col, int row) {
  constexpr Color kOutline{22, 20, 32, 140};
  constexpr Color kBackDark{74, 78, 96, 178};
  constexpr Color kFurMid{138, 144, 162, 210};
  constexpr Color kFurHigh{186, 194, 212, 228};
  constexpr Color kCream{238, 232, 218, 232};
  constexpr Color kMask{42, 40, 56, 165};
  constexpr Color kPink{236, 134, 156, 208};
  constexpr Color kPinkDark{196, 92, 118, 185};
  constexpr Color kEye{255, 220, 92, 250};
  constexpr Color kStrap{124, 134, 150, 210};
  constexpr Color kLanternCyan{100, 220, 248, 246};
  constexpr Color kLanternGold{255, 236, 152, 254};
  constexpr Color kWing{64, 224, 248, 246};
  constexpr Color kWingHi{188, 252, 255, 255};
  constexpr Color kWater{90, 210, 238, 225};

  const int f = col & 3;
  const int bob = (row == 0 && f == 2) || (row == 1 && (f == 1 || f == 3)) ? 1 : 0;
  const int dy = (row == 2 || row == 3) ? -1 : (row == 5 ? 1 : bob);

  // Orejas redondeadas con interior rosa
  fill_rect(px, col, row, 4, 2 + dy, 2, 2, kOutline);
  fill_rect(px, col, row, 8, 2 + dy, 2, 2, kOutline);
  put_px(px, col, row, 5, 3 + dy, kPink);
  put_px(px, col, row, 8, 3 + dy, kPink);

  // Cabeza gris plata, máscara oscura y hocico blanco puntiagudo con nariz rosa
  fill_rect(px, col, row, 3, 4 + dy, 7, 3, kOutline);
  fill_rect(px, col, row, 4, 4 + dy, 4, 1, kBackDark);
  fill_rect(px, col, row, 5, 4 + dy, 2, 1, kFurHigh);
  fill_rect(px, col, row, 4, 5 + dy, 2, 1, kFurMid);
  fill_rect(px, col, row, 6, 5 + dy, 2, 1, kMask);
  put_px(px, col, row, 7, 5 + dy, kEye);
  fill_rect(px, col, row, 8, 5 + dy, 4, 2, kCream);
  put_px(px, col, row, 12, 6 + dy, kPink);

  // Cuerpo con lomo oscuro, flanco gris plata y vientre crema
  fill_rect(px, col, row, 4, 7 + dy, 6, 4, kOutline);
  fill_rect(px, col, row, 5, 7 + dy, 2, 3, kBackDark);
  fill_rect(px, col, row, 6, 7 + dy, 2, 3, kFurMid);
  fill_rect(px, col, row, 8, 7 + dy, 2, 3, kCream);

  // Cola prensil rosa larga y enroscada detrás
  const int tail_y = 7 + dy + (f == 1 ? 1 : 0) - (row == 2 && f >= 2 ? 2 : 0);
  fill_rect(px, col, row, 1, tail_y, 3, 3, kOutline);
  put_px(px, col, row, 2, tail_y + 1, kPink);
  put_px(px, col, row, 1, tail_y + 1, kPinkDark);
  put_px(px, col, row, 2, tail_y + 2, kPink);
  put_px(px, col, row, 3, tail_y + 2, kPinkDark);

  // Patas rosas según pose
  const int stride = (row == 1) ? ((f == 0) ? -1 : (f == 2 ? 1 : 0)) : 0;
  fill_rect(px, col, row, 5 + stride, 11 + dy, 2, 2, kPink);
  fill_rect(px, col, row, 8 - stride, 11 + dy, 2, 2, kPink);

  if (row == 4) {  // trepando cuerda
    put_px(px, col, row, 7, 1 + (f & 1), kPink);
    put_px(px, col, row, 8, 1 + ((f + 1) & 1), kPink);
  }
  if (row == 5) {  // nado con estelas de agua
    fill_rect(px, col, row, 1 + (f & 1), 10, 3, 1, kWater);
    fill_rect(px, col, row, 10 - (f & 1), 10, 3, 1, kLanternCyan);
  }

  // Pequeño farol celeste-dorado colgando
  const int lx = 11 + (row == 1 && f == 2 ? 1 : 0);
  const int ly = 7 + dy - (row == 2 ? 2 : 0);
  put_px(px, col, row, lx, ly, kStrap);
  put_px(px, col, row, lx, ly + 1, (f & 1) ? kLanternGold : kLanternCyan);
  put_px(px, col, row, lx, ly + 2, (f & 2) ? kLanternCyan : kLanternGold);

  // Alas espectrales cian en Doble Salto (row 3)
  if (row == 3) {
    const int spread = 3 - (f & 1);
    for (int i = 0; i <= spread; ++i) {
      put_px(px, col, row, 3 - (i / 2), 2 + i, (f & 1) ? kWingHi : kWing);
      put_px(px, col, row, 2, 3 + i, kWing);
    }
    if (f == 3) put_px(px, col, row, 4, 10, kWing);
  }
}

void draw_beaver_pose(std::vector<uint8_t>& px, int col, int row) {
  constexpr Color kOutline{26, 16, 14, 140};
  constexpr Color kFurDark{88, 42, 26, 182};
  constexpr Color kFurMid{138, 68, 40, 212};
  constexpr Color kFurHigh{178, 98, 60, 228};
  constexpr Color kCream{226, 198, 162, 220};
  constexpr Color kTeeth{248, 246, 240, 238};
  constexpr Color kScarf{188, 42, 46, 215};
  constexpr Color kScarfHi{226, 76, 66, 232};
  constexpr Color kEye{255, 196, 72, 250};
  constexpr Color kTailDark{58, 34, 24, 196};
  constexpr Color kTailGrid{92, 56, 38, 212};
  constexpr Color kBrass{166, 134, 72, 218};
  constexpr Color kFlame{255, 168, 52, 248};
  constexpr Color kCore{255, 242, 174, 255};
  constexpr Color kWingGold{255, 218, 94, 248};
  constexpr Color kWingCyan{92, 236, 255, 252};
  constexpr Color kWater{90, 210, 238, 225};

  const int f = col & 3;
  const int bob = (row == 0 && f == 2) || (row == 1 && (f == 1 || f == 3)) ? 1 : 0;
  const int dy = (row == 2 || row == 3) ? -1 : (row == 5 ? 1 : bob);

  // Orejas redondas chiquitas
  put_px(px, col, row, 5, 2 + dy, kOutline);
  put_px(px, col, row, 6, 2 + dy, kFurDark);
  put_px(px, col, row, 8, 2 + dy, kOutline);
  put_px(px, col, row, 9, 2 + dy, kFurDark);

  // Cabeza castaño-rojiza + ojo ámbar + hocico crema + dos dientes incisivos blancos
  fill_rect(px, col, row, 3, 3 + dy, 8, 4, kOutline);
  fill_rect(px, col, row, 4, 3 + dy, 5, 2, kFurMid);
  fill_rect(px, col, row, 5, 3 + dy, 3, 1, kFurHigh);
  put_px(px, col, row, 8, 4 + dy, kEye);
  fill_rect(px, col, row, 8, 5 + dy, 4, 1, kCream);
  put_px(px, col, row, 9, 6 + dy, kTeeth);
  put_px(px, col, row, 10, 6 + dy, kTeeth);

  // Pañuelo rojo de aventurero
  fill_rect(px, col, row, 3, 7 + dy, 7, 1, kScarf);
  put_px(px, col, row, 2 + (f & 1), 7 + dy, kScarfHi);

  // Cuerpo robusto castaño-rojizo con panza crema
  fill_rect(px, col, row, 4, 8 + dy, 6, 4, kOutline);
  fill_rect(px, col, row, 5, 8 + dy, 3, 3, kFurDark);
  fill_rect(px, col, row, 8, 8 + dy, 2, 3, kCream);

  // Cola ancha plana cuadriculada marrón oscura detrás
  const int ty = 8 + dy - (row == 2 && f >= 2 ? 1 : 0) + (row == 5 ? (f & 1) : 0);
  fill_rect(px, col, row, 1, ty, 4, 3, kOutline);
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x)
      put_px(px, col, row, 1 + x, ty + y, ((x + y + (f & 1)) & 1) ? kTailGrid : kTailDark);

  // Patas
  const int stride = (row == 1) ? ((f == 0) ? -1 : (f == 2 ? 1 : 0)) : 0;
  fill_rect(px, col, row, 5 + stride, 12 + dy, 2, 2, kTailDark);
  fill_rect(px, col, row, 8 - stride, 12 + dy, 2, 2, kTailDark);

  if (row == 4) {  // trepando cuerda
    put_px(px, col, row, 7, 1 + (f & 1), kFurMid);
    put_px(px, col, row, 8, 1 + ((f + 1) & 1), kFurMid);
  }
  if (row == 5) {  // golpe de cola en el agua
    fill_rect(px, col, row, 1 + (f & 1), 10, 4, 1, kWater);
    fill_rect(px, col, row, 10 - (f & 1), 10, 3, 1, kWingCyan);
  }

  // Farol de latón cálido
  const int lx = 11 + (row == 1 && f == 2 ? 1 : 0);
  const int ly = 8 + dy - (row == 2 ? 2 : 0);
  put_px(px, col, row, lx, ly, kBrass);
  put_px(px, col, row, lx, ly + 1, (f & 1) ? kCore : kFlame);
  put_px(px, col, row, lx, ly + 2, (f & 2) ? kFlame : kCore);

  // Alas doradas-cian brillantes en Doble Salto (row 3)
  if (row == 3) {
    const int spread = 3 - (f & 1);
    for (int i = 0; i <= spread; ++i) {
      put_px(px, col, row, 3 - (i / 2), 2 + i, (f & 1) ? kWingCyan : kWingGold);
      put_px(px, col, row, 2, 3 + i, kWingGold);
    }
    if (f == 3) put_px(px, col, row, 4, 11, kWingCyan);
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

void draw_npc_frame(std::vector<uint8_t>& px, int col) {
  constexpr Color kOutline{22, 16, 24, 145};
  constexpr Color kRobeDark{86, 44, 40, 185};
  constexpr Color kRobeMid{128, 66, 52, 212};
  constexpr Color kGold{214, 162, 74, 232};
  constexpr Color kBeard{212, 206, 194, 215};
  constexpr Color kEye{255, 224, 130, 250};
  constexpr Color kStaff{102, 74, 48, 200};
  constexpr Color kOrb{255, 196, 82, 250};
  constexpr Color kOrbHi{255, 246, 188, 255};
  constexpr int row = int(SpriteRow::Npc);

  const int bob = (col == 2) ? 1 : 0;
  fill_rect(px, col, row, 4, 2 + bob, 5, 4, kOutline);
  fill_rect(px, col, row, 5, 3 + bob, 3, 2, kRobeMid);
  put_px(px, col, row, 5, 5 + bob, kEye);
  fill_rect(px, col, row, 5, 5 + bob, 3, 2, kBeard);
  put_px(px, col, row, 5, 5 + bob, kEye);
  fill_rect(px, col, row, 4, 6 + bob, 5, 7 - bob, (col & 1) ? kRobeMid : kRobeDark);
  fill_rect(px, col, row, 4, 7 + bob, 5, 1, kGold);
  // Báculo del vigía a la izquierda (mirando hacia el jugador que entra desde la izq.)
  fill_rect(px, col, row, 2, 5, 1, 9, kStaff);
  put_px(px, col, row, 2, 4, (col & 1) ? kOrbHi : kOrb);
  put_px(px, col, row, 2, 3, (col & 2) ? kOrbHi : kOrb);
}

void draw_lore_frame(std::vector<uint8_t>& px, int col) {
  constexpr Color kOutline{18, 20, 30, 145};
  constexpr Color kStoneDark{52, 56, 74, 188};
  constexpr Color kStoneMid{78, 84, 108, 218};
  constexpr Color kStoneHi{112, 118, 148, 232};
  constexpr Color kRune{255, 174, 62, 248};
  constexpr Color kRuneHi{255, 236, 152, 255};
  constexpr int row = int(SpriteRow::Npc);

  fill_rect(px, col, row, 4, 2, 8, 12, kOutline);
  fill_rect(px, col, row, 5, 3, 6, 10, kStoneDark);
  fill_rect(px, col, row, 5, 3, 5, 9, kStoneMid);
  fill_rect(px, col, row, 5, 2, 6, 1, kStoneHi);
  fill_rect(px, col, row, 3, 12, 10, 2, kStoneDark);
  // Runas dracónicas animadas
  put_px(px, col, row, 7, 5, (col & 1) ? kRuneHi : kRune);
  put_px(px, col, row, 8, 6, (col & 2) ? kRuneHi : kRune);
  put_px(px, col, row, 7, 8, (col == 2 || col == 3) ? kRuneHi : kRune);
  put_px(px, col, row, 8, 9, kRune);
}

AtlasLoad procedural() { return {build_sprite_atlas(), AtlasSource::Procedural}; }

AtlasLoad from_rgba(const uint8_t* img, int w, int h) {
  if (!img) return procedural();
  if (w == kSpriteAtlasW && h == kSpriteAtlasH) {
    AtlasLoad a;
    a.px.assign(img, img + std::size_t(kSpriteAtlasW) * kSpriteAtlasH * 4);
    a.source = AtlasSource::Png;
    return a;
  }
  // Retrocompatibilidad: PNG legacy 128×128 se pega en cols 0..7 sobre el atlas 256×128 procedural.
  if (w == 128 && h == 128) {
    AtlasLoad a{build_sprite_atlas(), AtlasSource::Png};
    for (int y = 0; y < 128; ++y) {
      const uint8_t* src = img + std::size_t(y) * 128 * 4;
      uint8_t* dst = &a.px[std::size_t(y) * kSpriteAtlasW * 4];
      std::copy_n(src, std::size_t(128) * 4, dst);
    }
    return a;
  }
  return procedural();
}
}  // namespace

std::vector<uint8_t> build_sprite_atlas() {
  std::vector<uint8_t> px(std::size_t(kSpriteAtlasW) * kSpriteAtlasH * 4, 0);
  for (int row = 0; row <= int(SpriteRow::Swim); ++row) {
    for (int col = 0; col < 4; ++col) draw_hero_pose(px, col, row);
    for (int col = 4; col < 8; ++col) draw_opossum_pose(px, col, row);
    for (int col = 8; col < 12; ++col) draw_beaver_pose(px, col, row);
  }
  for (int col = 0; col < 4; ++col) draw_egg_frame(px, col);
  for (int col = 4; col < 8; ++col) draw_relic_frame(px, col);
  for (int col = 0; col < 4; ++col) draw_npc_frame(px, col);
  for (int col = 4; col < 8; ++col) draw_lore_frame(px, col);
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
