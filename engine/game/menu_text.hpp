#pragma once
// Texto de la página Controles del menú de pausa (sin GL). echo_engine lo empaqueta en
// uniforms (4 glifos de 6 bits por int) y light.frag lo dibuja con su fuente 5×5.
#include "game/hero.hpp"
#include "game/menu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace ew::game {

inline constexpr int kTxtRows = 12, kTxtCols = 32, kTxtInts = kTxtRows * kTxtCols / 4;
inline constexpr int kGlyphNone = 63;
// Orden de la fuente de light.frag: A-Z, 0-9, '>', ' ', '-', '?', '/', '<', '.', ':', '!', ',', '''.
inline constexpr const char* kGlyphChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789> -?/<.:!,'";
inline constexpr int kGlyphCount = 47;

inline int glyph_index(char c) {
  for (int i = 0; kGlyphChars[i]; ++i)
    if (kGlyphChars[i] == c) return i;
  return kGlyphNone;  // fuera de la fuente: no se dibuja
}

// Fila 0 = título, 1..kCtlRows = acciones + POR DEFECTO + VOLVER, última = ayuda.
inline std::array<std::string, kTxtRows> controls_text(const PauseMenu& m) {
  std::array<std::string, kTxtRows> t{};
  t[0] = "CONTROLES";
  const auto& b = m.bindings();
  auto pad_to = [](std::string s, std::size_t n) { s.resize(std::max(s.size(), n), ' '); return s; };
  for (int r = 0; r < PauseMenu::kCtlRows; ++r) {
    std::string s = (r == m.ctl_cursor()) ? "> " : "  ";
    if (r < kBindCount) {
      s += pad_to(bind_action_name(r), 10);
      if (m.waiting() && r == m.ctl_cursor()) {
        s += pad_bindable_action(r) ? "TECLA O BOTON..." : "APRETA UNA TECLA";
      } else {
        const auto& k = b.key[std::size_t(r)];
        std::string ks = key_name(k[0]);
        if (k[1] != key::kNone) ks += "/" + key_name(k[1]);
        s += pad_to(ks, 12);
        const auto& p = b.pad[std::size_t(r)];
        std::string ps = pad_bindable_action(r) ? pad_name(p[0]) : "";
        if (pad_bindable_action(r) && p[1] != pad::kNone) ps += "/" + pad_name(p[1]);
        s += ps;
      }
    } else {
      s += r == PauseMenu::kCtlDefaults ? "POR DEFECTO" : "VOLVER";
    }
    t[std::size_t(r + 1)] = s;
  }
  t[kTxtRows - 1] = m.waiting() ? "ESC CANCELA" : "ENTER CAMBIA   ESC VUELVE";
  for (auto& s : t)
    if (s.size() > std::size_t(kTxtCols)) s.resize(std::size_t(kTxtCols));
  return t;
}

// Fila 0 = PAUSA, 1..9 = opciones del menú con "> " en cursor(), última = versión.
inline std::array<std::string, kTxtRows> pause_text(const PauseMenu& m, std::string_view version_str) {
  std::array<std::string, kTxtRows> t{};
  t[0] = "PAUSA";
  const int pct = int(std::lround(std::clamp(m.volume_setting(), 0.f, 1.f) * 100.f));
  const std::array<std::string, PauseMenu::kItemCount> labels = {
      "SEGUIR",
      "CONTINUAR",
      "REINICIAR",
      m.muted() ? std::string("MUTE") : ("VOL: " + std::to_string(pct)),
      "CONTROLES",
      std::string("BRISA: ") + (m.foliage() ? "SI" : "NO"),
      std::string("IMPACTO: ") + (m.impact_fx() ? "SI" : "NO"),
      std::string("HEROE: ") + hero_short_label(m.hero()),
      "SALIR",
  };
  for (int i = 0; i < PauseMenu::kItemCount; ++i)
    t[std::size_t(i + 1)] = std::string(m.cursor() == i ? "> " : "  ") + labels[std::size_t(i)];
  t[kTxtRows - 1] = "V" + std::string(version_str);
  for (auto& s : t)
    if (s.size() > std::size_t(kTxtCols)) s.resize(std::size_t(kTxtCols));
  return t;
}

// Selector de héroe: título, 3 personajes con ventaja pasiva debajo y ayuda al pie.
inline std::array<std::string, kTxtRows> hero_select_text(const PauseMenu& m) {
  std::array<std::string, kTxtRows> t{};
  t[0] = "ELIGE TU HEROE";
  static constexpr const char* kDesc[kHeroCount] = {
      "  LUZ AMPLIA Y FAROL LEJANO",
      "  PASO VELOZ Y SALTO AGIL",
      "  NADO FUERTE Y AGARRE FIRME",
  };
  const int sel = std::clamp(m.hero_cursor(), 0, kHeroCount - 1);
  for (int h = 0; h < kHeroCount; ++h) {
    t[std::size_t(1 + h * 2)] = std::string(sel == h ? "> " : "  ") + hero_label(HeroKind(h));
    t[std::size_t(2 + h * 2)] = kDesc[h];
  }
  t[kTxtRows - 1] = "ARRIBA/ABAJO  ENTER JUEGA";
  for (auto& s : t)
    if (s.size() > std::size_t(kTxtCols)) s.resize(std::size_t(kTxtCols));
  return t;
}

// Texto del overlay de Mapa Expandido (M / Tab): zona actual, % explorado, huevos y Tres Sellos.
inline std::array<std::string, kTxtRows> world_map_text(std::string_view zone_name, int explored_pct, int items,
                                                        int items_total, uint32_t seals_mask, int seals_total) {
  std::array<std::string, kTxtRows> t{};
  t[0] = "MAPA DEL MUNDO";
  t[1] = "ZONA: " + std::string(zone_name);
  t[9] = "EXPLORADO: " + std::to_string(std::clamp(explored_pct, 0, 100)) + "   HUEVOS: " +
         (items_total > 0 ? (std::to_string(items) + "/" + std::to_string(items_total)) : std::string("-"));
  if (seals_total > 0) {
    const bool s0 = (seals_mask & 1u) != 0u;
    const bool s1 = (seals_mask & 2u) != 0u;
    const bool s2 = (seals_mask & 4u) != 0u;
    const int broken = int(s0) + int(s1) + int(s2);
    t[10] = "SELLOS " + std::to_string(broken) + "/" + std::to_string(seals_total) + "  " +
            (s0 ? "PIEDRA " : "--- ") + (s1 ? "SOMBRA " : "--- ") + (s2 ? "AGUA" : "---");
  }
  t[kTxtRows - 1] = "M / TAB / ESC CIERRA";
  for (auto& s : t)
    if (s.size() > std::size_t(kTxtCols)) s.resize(std::size_t(kTxtCols));
  return t;
}

inline std::array<int32_t, kTxtInts> pack_text(const std::array<std::string, kTxtRows>& t) {
  std::array<int32_t, kTxtInts> out{};
  for (int r = 0; r < kTxtRows; ++r)
    for (int c = 0; c < kTxtCols; ++c) {
      const auto& s = t[std::size_t(r)];
      const int g = std::size_t(c) < s.size() ? glyph_index(s[std::size_t(c)]) : kGlyphNone;
      const int i = r * kTxtCols + c;
      out[std::size_t(i / 4)] |= int32_t(g) << ((i % 4) * 6);
    }
  return out;
}

}  // namespace ew::game
