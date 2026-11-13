#pragma once
// Autotiling de 4 vecinos (bitmask) para capas visuales (VIS / FG).
// Bits: N=1, E=2, S=4, W=8. Familia 1..15 (nunca 0: el id 0 es "vacío").
// Id compilado = familia*16 + máscara  (16..255). El atlas pone la familia
// en la fila (id/16) y la máscara en la columna (id%16); bisel solo en lados
// sin vecino. Fuente YAML: `familia = (tile % 15) + 1` (tile 0 se deja vacío).
#include <cstdint>
#include <vector>

namespace ew::map {

inline constexpr uint8_t kAtN = 1, kAtE = 2, kAtS = 4, kAtW = 8;
inline constexpr int kAutotileFamilies = 16;  // fila del atlas (0 reservada en ids compilados)

inline uint8_t autotile_family(uint16_t src) {
  if (src == 0) return 0;
  return uint8_t((src % 15) + 1);  // 1..15
}
inline uint16_t autotile_id(uint8_t family, uint8_t mask) {
  return uint16_t((uint16_t(family) * kAutotileFamilies) + (mask & 15));
}
inline uint8_t autotile_mask_of(uint16_t compiled) { return uint8_t(compiled & 15); }
inline uint8_t autotile_family_of(uint16_t compiled) { return uint8_t(compiled / kAutotileFamilies); }

// Reescribe in-place cada celda no nula: mismo id fuente = vecino conectado.
// Fuera del mapa = sin vecino (lado expuesto). COL/META no deben pasarse acá.
inline void apply_autotile4(uint16_t* g, int w, int h) {
  if (!g || w <= 0 || h <= 0) return;
  const std::size_t n = std::size_t(w) * std::size_t(h);
  std::vector<uint16_t> src(g, g + n);
  auto at = [&](int x, int y) -> uint16_t {
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    return src[std::size_t(y) * std::size_t(w) + std::size_t(x)];
  };
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const uint16_t v = at(x, y);
      if (!v) continue;
      uint8_t m = 0;
      if (at(x, y - 1) == v) m |= kAtN;
      if (at(x + 1, y) == v) m |= kAtE;
      if (at(x, y + 1) == v) m |= kAtS;
      if (at(x - 1, y) == v) m |= kAtW;
      g[std::size_t(y) * std::size_t(w) + std::size_t(x)] = autotile_id(autotile_family(v), m);
    }
}

}  // namespace ew::map
