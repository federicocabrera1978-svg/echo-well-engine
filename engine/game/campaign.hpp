#pragma once
// Progresión de niveles de campaña y resolución segura de mapas .ewmap (sin GL).
#include "core/paths.hpp"

#include <array>
#include <string>
#include <string_view>

namespace ew::game {

struct LevelInfo {
  std::string_view map_name;
  std::string_view title;
  std::string_view next_map;
};

inline constexpr std::array<LevelInfo, 4> kCampaignLevels{{
    {"world", "EL REINO DEL POZO", ""},
    {"slice", "NIVEL 1: EL POZO", "level2"},
    {"level2", "NIVEL 2: LAS CATACUMBAS", "level3"},
    {"level3", "NIVEL 3: LA GUARIDA DEL WYRM", ""},
}};

// Valida nombre lógico de mapa (evita traversal o separadores cuando viene de save.ewsv).
inline bool valid_map_name(std::string_view name) {
  if (name.empty() || name.size() > 64) return false;
  for (unsigned char c : name) {
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

inline const LevelInfo* find_campaign_level(std::string_view map_name) {
  for (const auto& lvl : kCampaignLevels)
    if (lvl.map_name == map_name) return &lvl;
  return nullptr;
}

inline std::string_view next_campaign_map(std::string_view map_name) {
  const auto* lvl = find_campaign_level(map_name);
  return lvl ? lvl->next_map : std::string_view{};
}

// Título legible para la fuente 5×5 del HUD/pantalla de final.
inline std::string level_title(std::string_view map_name) {
  if (const auto* lvl = find_campaign_level(map_name)) return std::string(lvl->title);
  std::string out;
  out.reserve(map_name.size());
  for (unsigned char c : map_name) {
    if (c >= 'a' && c <= 'z') out.push_back(char(c - 'a' + 'A'));
    else if (c == '_') out.push_back(' ');
    else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == ' ') out.push_back(char(c));
  }
  return out.empty() ? std::string("NIVEL") : out;
}

// Busca `<map_name>.ewmap` junto al mapa actual, junto al ejecutable, en cwd o en `build_maps_dir`.
inline std::string resolve_map_file(std::string_view map_name, const std::string& current_map_path,
                                    const std::string& exe_root, const std::string& build_maps_dir = {}) {
  if (!valid_map_name(map_name)) return {};
  const std::string fname = std::string(map_name) + ".ewmap";
  if (!current_map_path.empty()) {
    const std::string cand = ew::core::join(ew::core::parent_path(current_map_path), fname);
    if (ew::core::file_exists(cand)) return cand;
  }
  if (!exe_root.empty()) {
    const std::string cand = ew::core::join(exe_root, "assets/maps/bin/" + fname);
    if (ew::core::file_exists(cand)) return cand;
  }
  {
    const std::string cand = "assets/maps/bin/" + fname;
    if (ew::core::file_exists(cand)) return cand;
  }
  if (!build_maps_dir.empty()) {
    const std::string cand = ew::core::join(build_maps_dir, fname);
    if (ew::core::file_exists(cand)) return cand;
  }
  return {};
}

}  // namespace ew::game
