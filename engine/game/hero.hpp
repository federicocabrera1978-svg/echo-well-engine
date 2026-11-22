#pragma once
// Selección de héroe jugable y metadatos sin dependencias de GL/GLFW.
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace ew::game {

enum class HeroKind : uint8_t { Lantern = 0, Opossum = 1, Beaver = 2 };

inline constexpr int kHeroCount = 3;

// Normaliza un byte arbitrario a un héroe válido (Lantern por defecto).
inline HeroKind sanitize_hero(uint8_t v) {
  return v < uint8_t(kHeroCount) ? HeroKind(v) : HeroKind::Lantern;
}

// Columna inicial en el atlas 16×8 de sprites para cada héroe (0, 4 u 8).
inline int hero_sprite_col_offset(HeroKind h) {
  switch (sanitize_hero(uint8_t(h))) {
    case HeroKind::Lantern: return 0;
    case HeroKind::Opossum: return 4;
    case HeroKind::Beaver:  return 8;
  }
  return 0;
}

// Nombre completo en mayúsculas compatible con la fuente 5×5 del HUD.
inline const char* hero_label(HeroKind h) {
  switch (sanitize_hero(uint8_t(h))) {
    case HeroKind::Lantern: return "SR DEL FAROL";
    case HeroKind::Opossum: return "LA ZARIGUEYA";
    case HeroKind::Beaver:  return "EL CASTOR";
  }
  return "SR DEL FAROL";
}

// Nombre corto para la línea de estado en el menú de pausa.
inline const char* hero_short_label(HeroKind h) {
  switch (sanitize_hero(uint8_t(h))) {
    case HeroKind::Lantern: return "FAROL";
    case HeroKind::Opossum: return "ZARIGUEYA";
    case HeroKind::Beaver:  return "CASTOR";
  }
  return "FAROL";
}

// Parsea argumento de CLI (--hero) de forma case-insensitive.
inline bool hero_from_arg(std::string_view sv, HeroKind& out) {
  std::string s;
  s.reserve(sv.size());
  for (unsigned char ch : sv) s.push_back(char(std::tolower(ch)));
  if (s == "farol" || s == "sr_farol" || s == "lantern") {
    out = HeroKind::Lantern;
    return true;
  }
  if (s == "zarigueya" || s == "opossum") {
    out = HeroKind::Opossum;
    return true;
  }
  if (s == "castor" || s == "beaver") {
    out = HeroKind::Beaver;
    return true;
  }
  return false;
}

// Avanza o retrocede ciclando entre los kHeroCount héroes.
inline HeroKind cycle_hero(HeroKind cur, int delta) {
  const int base = int(sanitize_hero(uint8_t(cur)));
  const int next = ((base + delta) % kHeroCount + kHeroCount) % kHeroCount;
  return HeroKind(next);
}

}  // namespace ew::game
