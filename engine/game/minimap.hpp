#pragma once
// Lógica pura del mini-mapa exploratorio Metroidvania (sin dependencias de GL/GLFW).
#include "game/scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

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
  uint32_t seals_mask = 0;  // bit 0=Piedra (Pozo), bit 1=Sombra (Catacumbas), bit 2=Agua (Archivo)
  int seals_total = 0;      // 3 en mapas con Pórtico de los Tres Sellos (world), 0 en el resto
};

inline std::string_view zone_name_for(const Scene& scene) {
  const std::string_view map_name = scene.map().string_at(1);
  const int cx = scene.chunk_x();
  const int cy = scene.chunk_y();
  if (map_name == "world") {
    if (cy == 0) return "EL POZO";
    if (cy == 1 || cy == 2) return cx >= 4 ? "LAS CATACUMBAS" : "GUARIDA DEL WYRM";
    if (cy == 3) return "ARCHIVO SUMERGIDO";
    return "CAMPANARIO HUNDIDO";
  }
  if (map_name == "slice") return "EL POZO";
  if (map_name == "level2") return "LAS CATACUMBAS";
  if (map_name == "level3") return "GUARIDA DEL WYRM";
  const auto bd = static_cast<ew::map::BackdropTheme>(scene.backdrop(cx, cy).theme);
  switch (bd) {
    case ew::map::BackdropTheme::Pozo: return "EL POZO";
    case ew::map::BackdropTheme::Cripta:
    case ew::map::BackdropTheme::Abismo: return "LAS CATACUMBAS";
    case ew::map::BackdropTheme::Guarida: return "GUARIDA DEL WYRM";
    case ew::map::BackdropTheme::Aqueduct: return "ARCHIVO SUMERGIDO";
    case ew::map::BackdropTheme::Tower: return "CAMPANARIO HUNDIDO";
    case ew::map::BackdropTheme::Santuario: return "EL SANTUARIO";
    default: return "SUBSUELO";
  }
}

// Calcula el estado del mini-mapa a partir de la escena actual: chunks visitados,
// puertas/muros/cristales cerrados, huevos pendientes, sellos rotos y porcentaje explorado.
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

  uint32_t seal_present = 0;
  for (const auto& e : scene.entities()) {
    if (e.name == "door_seal_pozo") {
      seal_present |= 1u;
      if (e.active) st.seals_mask |= 1u;
    } else if (e.name == "door_seal_cat") {
      seal_present |= 2u;
      if (e.active) st.seals_mask |= 2u;
    } else if (e.name == "door_seal_archivo") {
      seal_present |= 4u;
      if (e.active) st.seals_mask |= 4u;
    } else if (e.name == "boss_pozo" && e.active) {
      st.seals_mask |= 1u;
    } else if (e.name == "boss_catacomb" && e.active) {
      st.seals_mask |= 2u;
    } else if (e.name == "boss_sentinel" && e.active) {
      st.seals_mask |= 4u;
    }
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
  if (seal_present == 7u) {
    st.seals_total = 3;
  } else {
    st.seals_total = 0;
    st.seals_mask = 0;
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

// Nodo de Viaje Rápido (Santuario principal por zona) desbloqueado al visitar su chunk.
struct FastTravelNode {
  std::string_view label;
  std::string_view entity_name;
  int cx = 0, cy = 0;
  float x = 0.f, y = 0.f;
  int ent_idx = -1;
  bool unlocked = false;
};

inline std::vector<FastTravelNode> list_fast_travel_nodes(const Scene& scene) {
  std::vector<FastTravelNode> out;
  const auto& hdr = scene.map().header();
  const int mw = int(hdr.map_w_chunks);
  const int mh = int(hdr.map_h_chunks);
  if (mw <= 0 || mh <= 0) return out;
  constexpr float kChunkPxW = float(ew::map::kChunkW * ew::map::kTileSize);
  constexpr float kChunkPxH = float(ew::map::kChunkH * ew::map::kTileSize);
  const uint64_t vis = scene.visited_chunks();

  struct Spec {
    std::string_view label;
    std::string_view entity_name;
    bool by_type;
  };
  static constexpr Spec kSpecs[] = {
      {"EL POZO", "player_spawn", true},
      {"LAS CATACUMBAS", "ck_hub2", false},
      {"GUARIDA DEL WYRM", "ck_santuario3", false},
      {"ARCHIVO SUMERGIDO", "aq_ck_entrada", false},
      {"CAMPANARIO HUNDIDO", "tw_ck", false},
  };
  const auto& ents = scene.entities();
  for (const auto& sp : kSpecs) {
    for (std::size_t i = 0; i < ents.size(); ++i) {
      const auto& e = ents[i];
      const bool match = sp.by_type ? (e.type == sp.entity_name) : (e.name == sp.entity_name);
      if (!match) continue;
      const int cx = std::clamp(int(std::floor(e.x / kChunkPxW)), 0, mw - 1);
      const int cy = std::clamp(int(std::floor(e.y / kChunkPxH)), 0, mh - 1);
      const int bit = cy * mw + cx;
      const bool unlocked = (bit >= 0 && bit < 64) && (((vis >> unsigned(bit)) & 1ull) != 0ull);
      out.push_back(FastTravelNode{sp.label, e.name, cx, cy, e.x, e.y, int(i), unlocked});
      break;
    }
  }
  if (out.size() <= 1) out.clear();  // solo ofrecer viaje rápido si hay 2+ santuarios en el mapa
  return out;
}

}  // namespace ew::game
