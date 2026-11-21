#pragma once
#include "render/tile_atlas.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

// Atlas de sprites pixel-art (sin GL): 8×8 celdas de 16×16 px -> textura RGBA8 128×128.
// Filas: 0 Idle, 1 Walk, 2 Jump, 3 DoubleJump, 4 Climb, 5 Swim (protagonista), 6 Objetos (huevo/reliquia),
//        7 NPC (Vigía/Cronista, cols 0..3) y Estela dracónica (cols 4..7).
// Alpha: 0 = transparente, 1..255 = altura/relieve para el normal pass (>240 = auto-emisión suave).
namespace ew::render {
inline constexpr int kSpriteCellPx = 16;
inline constexpr int kSpriteGrid = 8;
inline constexpr int kSpriteAtlasPx = kSpriteCellPx * kSpriteGrid;  // 128

enum class SpriteRow : int {
  Idle = 0,
  Walk = 1,
  Jump = 2,
  DoubleJump = 3,
  Climb = 4,
  Swim = 5,
  Items = 6,
  Npc = 7,
};

struct SpriteUV { int x, y; };
constexpr SpriteUV sprite_uv(int col, int row) {
  return {(col & 7) * kSpriteCellPx, (row & 7) * kSpriteCellPx};
}

// Genera el atlas procedural de respaldo (128×128×4 bytes, fila 0 arriba). Determinístico.
std::vector<uint8_t> build_sprite_atlas();

// Carga un PNG RGBA de 128×128. Si falta, está corrupto o difiere en tamaño, devuelve el procedural.
AtlasLoad load_sprite_atlas_mem(const void* data, std::size_t n);
AtlasLoad load_sprite_atlas_file(const char* path);
}  // namespace ew::render
