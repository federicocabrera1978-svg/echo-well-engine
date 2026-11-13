// Autotiling 4 vecinos: máscara, ids compilados, apply_autotile4 (sin GL).
#include "map/autotile.hpp"
#include <cstdio>
#include <vector>
using namespace ew::map;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main() {
  CHECK(autotile_family(0) == 0);
  CHECK(autotile_family(17) == 3 && autotile_family(5) == 6);
  CHECK(autotile_family(9) == 10 && autotile_family(21) == 7);
  CHECK(autotile_id(1, 0) == 16);   // nunca 0 (reservado a vacío)
  CHECK(autotile_id(15, 15) == 255);
  CHECK(autotile_mask_of(167) == 7 && autotile_family_of(167) == 10);
  for (uint8_t f = 1; f <= 15; ++f)
    for (uint8_t m = 0; m < 16; ++m) {
      const auto id = autotile_id(f, m);
      CHECK(id != 0 && id <= 255);
      CHECK(autotile_family_of(id) == f && autotile_mask_of(id) == m);
    }

  // Aislado
  std::vector<uint16_t> g(3 * 3, 0);
  g[1 * 3 + 1] = 17;
  apply_autotile4(g.data(), 3, 3);
  CHECK(g[1 * 3 + 1] == autotile_id(autotile_family(17), 0));
  CHECK(g[0] == 0);

  // Fila horizontal de 3 (mismo id): extremos 1 vecino, medio 2.
  std::vector<uint16_t> row(5, 0);
  row[1] = row[2] = row[3] = 17;
  apply_autotile4(row.data(), 5, 1);
  const uint8_t f17 = autotile_family(17);
  CHECK(row[1] == autotile_id(f17, kAtE));
  CHECK(row[2] == autotile_id(f17, kAtE | kAtW));
  CHECK(row[3] == autotile_id(f17, kAtW));
  CHECK(row[0] == 0 && row[4] == 0);

  // 3×3 lleno: centro 4 vecinos, lados 3, esquinas 2. Ids distintos no conectan.
  std::vector<uint16_t> blk(3 * 3, 17);
  blk[0] = 5;  // esquina NW de otra familia
  apply_autotile4(blk.data(), 3, 3);
  CHECK(autotile_mask_of(blk[1 * 3 + 1]) == (kAtN | kAtE | kAtS | kAtW));  // centro sigue teniendo N=17? NW is 5, N is (1,0)=index 1 which is 17. Center (1,1). N=(1,0)=17, E=17, S=17, W=(0,1)=17. Yes 15.
  CHECK(autotile_family_of(blk[1 * 3 + 1]) == f17);
  // (0,0) era 5, aislado respecto de 17s: vecinos E=17, S=17 → no conecta, mask 0
  CHECK(blk[0] == autotile_id(autotile_family(5), 0));
  // (2,0) NE esquina original 17: W ahora es 5, no conecta; E fuera; N fuera; S=17
  CHECK(blk[2] == autotile_id(f17, kAtS | kAtW));  // (2,0) NE: S y W son 17
  // (0,2) SW (x=0,y=2): N=17, E=17, S fuera, W fuera → N|E
  CHECK(blk[2 * 3 + 0] == autotile_id(f17, kAtN | kAtE));
  // borde norte (1,0): S,E,W = 17, N fuera, W=(0,0)=5 no. → E|S
  CHECK(blk[0 * 3 + 1] == autotile_id(f17, kAtE | kAtS));

  // 2×2 del mismo tile: cada uno 2 vecinos.
  std::vector<uint16_t> q(2 * 2, 9);
  apply_autotile4(q.data(), 2, 2);
  const uint8_t f9 = autotile_family(9);
  CHECK(q[0] == autotile_id(f9, kAtE | kAtS));
  CHECK(q[1] == autotile_id(f9, kAtW | kAtS));
  CHECK(q[2] == autotile_id(f9, kAtE | kAtN));
  CHECK(q[3] == autotile_id(f9, kAtW | kAtN));

  // slice: columna 2×16 de tile 9 en (44,4)..; (44,10) → N|E|S
  constexpr int tw = 120, th = 23;
  std::vector<uint16_t> sl(std::size_t(tw) * th, 0);
  for (int y = 4; y < 20; ++y)
    for (int x = 44; x < 46; ++x) sl[std::size_t(y) * tw + x] = 9;
  apply_autotile4(sl.data(), tw, th);
  const uint16_t got = sl[std::size_t(10) * tw + 44];
  CHECK(got == autotile_id(f9, kAtN | kAtE | kAtS));  // 167
  CHECK(got == 167);

  std::puts("autotile ok");
  return 0;
}
