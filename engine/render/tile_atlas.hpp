#pragma once
#include <cstdint>
#include <vector>

// Atlas de tiles procedural (sin GL): 16×16 tiles de 8×8 px -> textura RGBA8 128×128.
// tile id = valor VIS del mapa (0..255). RGB = albedo, A = altura (0..255) para el bisel.
namespace ew::render {
inline constexpr int kTilePx = 8;
inline constexpr int kAtlasTiles = 16;                       // por lado
inline constexpr int kAtlasPx = kTilePx * kAtlasTiles;       // 128

struct TileUV { int x, y; };  // origen en px del tile dentro del atlas
constexpr TileUV tile_uv(uint8_t id) { return {(id % kAtlasTiles) * kTilePx, (id / kAtlasTiles) * kTilePx}; }

// Genera el atlas (kAtlasPx*kAtlasPx*4 bytes, fila 0 arriba). Determinístico.
std::vector<uint8_t> build_tile_atlas();
}  // namespace ew::render
