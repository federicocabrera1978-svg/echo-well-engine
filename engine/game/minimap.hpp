#pragma once
// Lógica pura del mini-mapa exploratorio Metroidvania (sin dependencias de GL/GLFW).
#include "game/scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ew::game {

// Estado derivado del mapa actual para el HUD del mini-mapa.
struct MinimapState {
  int map_w = 0, map_h = 0, cur_cx = 0, cur_cy = 0;
  uint64_t visited_mask = 0;
  uint64_t blocked_doors_mask = 0;
  uint64_t missing_eggs_mask = 0;
  int visited_count = 0;
  int total_chunks = 0;
  int explored_pct = 0;
};

// Calcula el estado del mini-mapa a partir de la escena actual: chunks visitados,
// puertas/muros cerrados, huevos pendientes de recoger y porcentaje explorado.
inline MinimapState compute_minimap(const Scene& scene) {
  MinimapState st;
  const auto& hdr = scene.map().header();
  st.map_w = int(hdr.map_w_chunks);
  st.map_h = int(hdr.map_h_chunks);
  st.cur_cx = scene.chunk_x();
  st.cur_cy = scene.chunk_y();
  st.visited_mask = scene.visited_chunks();

  if (st.map_w <= 0 || st.map_h <= 0) return st;

  st.total_chunks = std::min(64, st.map_w * st.map_h);

  constexpr float kChunkPxW = float(ew::map::kChunkW * ew::map::kTileSize);
  constexpr float kChunkPxH = float(ew::map::kChunkH * ew::map::kTileSize);

  for (const auto& e : scene.entities()) {
    if (e.active) continue;
    const bool is_blocker =
        (e.type == "door" || e.type == "one_way_door" || e.type == "break_wall" || e.type == "crystal");
    const bool is_egg = (e.type == "collectible");
    if (!is_blocker && !is_egg) continue;

    const int cx = std::clamp(int(std::floor(e.x / kChunkPxW)), 0, st.map_w - 1);
    const int cy = std::clamp(int(std::floor(e.y / kChunkPxH)), 0, st.map_h - 1);
    const int bit = cy * st.map_w + cx;
    if (bit < 0 || bit >= 64) continue;

    const uint64_t mask = uint64_t(1) << unsigned(bit);
    if (is_blocker) st.blocked_doors_mask |= mask;
    if (is_egg) st.missing_eggs_mask |= mask;
  }

  int count = 0;
  const int limit = std::min(64, st.total_chunks);
  for (int i = 0; i < limit; ++i) {
    if ((st.visited_mask >> unsigned(i)) & 1ull) ++count;
  }
  st.visited_count = count;
  st.explored_pct = st.total_chunks > 0
                        ? std::clamp((st.visited_count * 100) / st.total_chunks, 0, 100)
                        : 0;
  return st;
}

}  // namespace ew::game
