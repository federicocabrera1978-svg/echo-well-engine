#pragma once
// Sistema multipunto de iluminación en C++ puro (sin dependencias de GL/GLFW).
// Calcula la oscuridad ambiental por bioma, la posición del farol del héroe
// y recolecta luces dinámicas y ambientales deterministas de la escena.
#include "game/frame_view.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"
#include "map/backdrop.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ew::game {

// Tipos de luz puntual (empaquetado en point_light_pos[i * 4 + 3]).
enum class PointLightKind : uint8_t { Torch = 0, Crystal = 1, Lava = 2, Shrine = 3 };

// Oscuridad ambiental base según el bioma del chunk activo (0 = sin oscuridad).
inline float biome_darkness(ew::map::BackdropTheme t) {
  switch (t) {
    case ew::map::BackdropTheme::Vacio:
      return 0.f;
    case ew::map::BackdropTheme::Abismo:
    case ew::map::BackdropTheme::Guarida:
    case ew::map::BackdropTheme::Cripta:
      return 0.85f;
    case ew::map::BackdropTheme::Pozo:
    case ew::map::BackdropTheme::Raices:
    case ew::map::BackdropTheme::Grieta:
    case ew::map::BackdropTheme::Cueva:
    case ew::map::BackdropTheme::Invertido:
    case ew::map::BackdropTheme::Arena:
      return 0.68f;
    case ew::map::BackdropTheme::Cisterna:
    case ew::map::BackdropTheme::Nido:
    case ew::map::BackdropTheme::Engranajes:
    case ew::map::BackdropTheme::Santuario:
      return 0.48f;
  }
  return 0.68f;
}

namespace lighting_detail {

inline uint32_t hash_u32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

inline uint32_t chunk_slot_hash(int ncx, int ncy, uint16_t seed, int tag, int slot) {
  const uint32_t k = uint32_t(seed) * 747796405u ^
                     uint32_t(ncx) * 2891336453u ^
                     uint32_t(ncy) * 277803737u ^
                     uint32_t(tag) * 1597334677u ^
                     uint32_t(slot) * 2246822519u;
  return hash_u32(k);
}

// Consulta const-correcta contra el vecindario 3×3 empaquetado (capa COL) y puertas cerradas.
inline bool solid_at_world_px(const Scene& scene, float wx, float wy) {
  if (scene.chunk_x() < 0 || scene.chunk_y() < 0) return true;
  if (wx < 0.f || wy < 0.f || wx >= float(scene.width_px()) || wy >= float(scene.height_px())) return true;
  const int tx = int(std::floor(wx / float(ew::map::kTileSize)));
  const int ty = int(std::floor(wy / float(ew::map::kTileSize)));
  const int ox = tx - (scene.chunk_x() - 1) * int(ew::map::kChunkW);
  const int oy = ty - (scene.chunk_y() - 1) * int(ew::map::kChunkH);
  if (ox < 0 || oy < 0 || ox >= Scene::kNbW || oy >= Scene::kNbH) return true;
  const std::size_t idx =
      (std::size_t(oy) * std::size_t(Scene::kNbW) + std::size_t(ox)) * std::size_t(Scene::kPackedCh);
  return scene.active_tiles()[idx] != 0 || scene.door_blocks(tx, ty);
}

inline bool has_torch_theme(ew::map::BackdropTheme t) {
  using BT = ew::map::BackdropTheme;
  return t == BT::Pozo || t == BT::Cripta || t == BT::Cueva || t == BT::Santuario ||
         t == BT::Abismo || t == BT::Arena;
}

inline bool has_crystal_theme(ew::map::BackdropTheme t) {
  using BT = ew::map::BackdropTheme;
  return t == BT::Cripta || t == BT::Santuario || t == BT::Abismo || t == BT::Cisterna ||
         t == BT::Invertido || t == BT::Raices;
}

inline bool has_lava_theme(ew::map::BackdropTheme t) {
  using BT = ew::map::BackdropTheme;
  return t == BT::Grieta || t == BT::Guarida || t == BT::Arena;
}

}  // namespace lighting_detail

// Recolecta la luz principal del farol y hasta kMaxPointLights luces puntuales de la escena.
inline void collect_room_lights(FrameData& v, const Scene& scene, const Sim& sim, [[maybe_unused]] float time_s) {
  const auto& pl = sim.player();
  v.light_x = pl.pos.x + float(sim.player_facing()) * 4.f;
  v.light_y = pl.pos.y - 8.f * sim.down();
  const auto cur_bd = scene.backdrop(scene.chunk_x(), scene.chunk_y());
  const auto cur_theme = static_cast<ew::map::BackdropTheme>(ew::map::backdrop_sanitize(cur_bd.theme));
  v.ambient_darkness = biome_darkness(cur_theme);
  v.lantern_radius = 125.f;
  v.point_light_count = 0;

  const float cam_x = scene.cam_x();
  const float cam_y = scene.cam_y();
  constexpr float kCullMargin = 48.f;
  const float x_min = cam_x - kCullMargin;
  const float x_max = cam_x + float(kInternalW) + kCullMargin;
  const float y_min = cam_y - kCullMargin;
  const float y_max = cam_y + float(kInternalH) + kCullMargin;

  auto push_light = [&](float x, float y, float radius, PointLightKind kind,
                        float r, float g, float b, float phase) {
    if (v.point_light_count >= kMaxPointLights) return;
    if (x < x_min || x > x_max || y < y_min || y > y_max) return;
    const int i = v.point_light_count++;
    const std::size_t base = std::size_t(i) * 4;
    v.point_light_pos[base + 0] = x;
    v.point_light_pos[base + 1] = y;
    v.point_light_pos[base + 2] = radius;
    v.point_light_pos[base + 3] = float(kind);
    v.point_light_col[base + 0] = r;
    v.point_light_col[base + 1] = g;
    v.point_light_col[base + 2] = b;
    v.point_light_col[base + 3] = phase;
  };

  // 1) Luces de entidades dinámicas (prioridad: brasas de Wyrm -> hitos y reliquias).
  for (const auto& fb : sim.fireballs()) {
    push_light(fb.x, fb.y, 44.f, PointLightKind::Lava, 1.0f, 0.45f, 0.14f, fb.x * 0.07f + fb.y * 0.05f);
  }
  for (const auto& e : scene.entities()) {
    if (e.type == "checkpoint") {
      if (e.active)
        push_light(e.x, e.y - 4.f, 34.f, PointLightKind::Shrine, 0.28f, 0.95f, 0.55f, e.x * 0.03f);
      else
        push_light(e.x, e.y - 4.f, 28.f, PointLightKind::Shrine, 0.24f, 0.62f, 0.85f, e.x * 0.03f);
    } else if (is_switch_type(e.type)) {
      if (e.active)
        push_light(e.x, e.y - 4.f, 26.f, PointLightKind::Shrine, 0.25f, 1.0f, 0.50f, e.x * 0.04f);
      else
        push_light(e.x, e.y - 4.f, 26.f, PointLightKind::Shrine, 1.0f, 0.55f, 0.12f, e.x * 0.04f);
    } else if (!e.active) {
      if (e.type == "ability_double_jump")
        push_light(e.x, e.y, 32.f, PointLightKind::Crystal, 0.35f, 0.92f, 1.0f, e.x * 0.05f);
      else if (e.type == "ability_dash")
        push_light(e.x, e.y, 32.f, PointLightKind::Crystal, 1.0f, 0.52f, 0.24f, e.x * 0.05f);
      else if (e.type == "ability_wall_climb")
        push_light(e.x, e.y, 32.f, PointLightKind::Crystal, 0.35f, 1.0f, 0.60f, e.x * 0.05f);
      else if (e.type == "collectible")
        push_light(e.x, e.y - 1.f, 32.f, PointLightKind::Crystal, 0.95f, 0.85f, 0.45f, e.x * 0.05f);
    }
  }

  // 2) Luces ambientales deterministas por bioma en el vecindario 3×3 (chunk activo primero).
  const int cx = scene.chunk_x();
  const int cy = scene.chunk_y();
  const int map_w = scene.map().header().map_w_chunks;
  const int map_h = scene.map().header().map_h_chunks;
  constexpr float kTs = float(ew::map::kTileSize);
  constexpr int kCw = int(ew::map::kChunkW);
  constexpr int kCh = int(ew::map::kChunkH);

  for (int dy : {0, -1, 1}) {
    for (int dx : {0, -1, 1}) {
      if (v.point_light_count >= kMaxPointLights) return;
      const int ncx = cx + dx;
      const int ncy = cy + dy;
      if (ncx < 0 || ncy < 0 || ncx >= map_w || ncy >= map_h) continue;

      const auto bd = scene.backdrop(ncx, ncy);
      const auto theme = static_cast<ew::map::BackdropTheme>(ew::map::backdrop_sanitize(bd.theme));
      if (theme == ew::map::BackdropTheme::Vacio) continue;

      const float cx0 = float(ncx * kCw * int(ew::map::kTileSize));
      const float cy0 = float(ncy * kCh * int(ew::map::kTileSize));
      auto tile_cx = [&](int tx) { return cx0 + (float(tx) + 0.5f) * kTs; };
      auto tile_cy = [&](int ty) { return cy0 + (float(ty) + 0.5f) * kTs; };

      if (lighting_detail::has_torch_theme(theme)) {
        for (int slot = 0; slot < 3; ++slot) {
          const uint32_t h = lighting_detail::chunk_slot_hash(ncx, ncy, bd.seed, 1, slot);
          const int tx_min = 2 + slot * 12;
          const int tx0 = int(h % 12u);
          const int ty0 = int((h >> 8) % 16u);
          bool placed = false;
          for (int iy = 0; iy < 16 && !placed; ++iy) {
            const int ty = 3 + ((ty0 + iy) % 16);
            const float wy = tile_cy(ty);
            for (int ix = 0; ix < 12; ++ix) {
              const int tx = tx_min + ((tx0 + ix) % 12);
              const float wx = tile_cx(tx);
              if (lighting_detail::solid_at_world_px(scene, wx, wy)) continue;
              const bool near_wall = lighting_detail::solid_at_world_px(scene, wx - 10.f, wy) ||
                                     lighting_detail::solid_at_world_px(scene, wx + 10.f, wy) ||
                                     lighting_detail::solid_at_world_px(scene, wx, wy - 10.f);
              if (!near_wall) continue;
              const float phase = float(ncx * 13 + ncy * 7 + slot) * 1.7f;
              push_light(wx, wy, 58.f, PointLightKind::Torch, 1.0f, 0.62f, 0.22f, phase);
              placed = true;
              break;
            }
          }
        }
      }

      if (lighting_detail::has_crystal_theme(theme)) {
        for (int slot = 0; slot < 2; ++slot) {
          const uint32_t h = lighting_detail::chunk_slot_hash(ncx, ncy, bd.seed, 2, slot);
          const int tx_min = 3 + slot * 18;
          const int tx0 = int(h % 16u);
          const bool prefer_ceil = (h & 1u) == 0u;
          const bool cyan = ((ncx + ncy + slot + int((h >> 4) & 1u)) & 1) == 0;
          const float cr = cyan ? 0.26f : 0.62f;
          const float cg = cyan ? 0.88f : 0.40f;
          const float cb = 1.0f;
          const float phase = float(ncx * 11 + ncy * 5 + slot) * 2.1f;

          auto try_crystal_band = [&](int ty_start, int ty_end, int ty_step, float anchor_dy) {
            for (int ty = ty_start; ty != ty_end; ty += ty_step) {
              const float wy = tile_cy(ty);
              for (int ix = 0; ix < 16; ++ix) {
                const int tx = tx_min + ((tx0 + ix) % 16);
                const float wx = tile_cx(tx);
                if (lighting_detail::solid_at_world_px(scene, wx, wy)) continue;
                if (!lighting_detail::solid_at_world_px(scene, wx, wy + anchor_dy)) continue;
                push_light(wx, wy, 46.f, PointLightKind::Crystal, cr, cg, cb, phase);
                return true;
              }
            }
            return false;
          };

          const bool placed = prefer_ceil
              ? (try_crystal_band(1, 9, 1, -8.f) || try_crystal_band(21, 13, -1, 8.f))
              : (try_crystal_band(21, 13, -1, 8.f) || try_crystal_band(1, 9, 1, -8.f));
          (void)placed;
        }
      }

      if (lighting_detail::has_lava_theme(theme)) {
        for (int slot = 0; slot < 2; ++slot) {
          const uint32_t h = lighting_detail::chunk_slot_hash(ncx, ncy, bd.seed, 3, slot);
          const int tx_min = 4 + slot * 17;
          const int tx0 = int(h % 16u);
          bool placed = false;
          for (int ty = 21; ty >= 12 && !placed; --ty) {
            const float wy = tile_cy(ty);
            for (int ix = 0; ix < 16; ++ix) {
              const int tx = tx_min + ((tx0 + ix) % 16);
              const float wx = tile_cx(tx);
              if (lighting_detail::solid_at_world_px(scene, wx, wy)) continue;
              if (!lighting_detail::solid_at_world_px(scene, wx, wy + 8.f) &&
                  !lighting_detail::solid_at_world_px(scene, wx, wy + 12.f))
                continue;
              const float phase = float(ncx * 9 + ncy * 17 + slot) * 1.3f;
              push_light(wx, wy, 54.f, PointLightKind::Lava, 1.0f, 0.34f, 0.10f, phase);
              placed = true;
              break;
            }
          }
        }
      }
    }
  }
}

}  // namespace ew::game
