#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include "render/tile_atlas.hpp"
using namespace ew::render;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)
int main() {
  auto a = build_tile_atlas(), b = build_tile_atlas();
  CHECK(a.size() == std::size_t(kAtlasPx) * kAtlasPx * 4);
  CHECK(b.size() == a.size() && std::memcmp(a.data(), b.data(), a.size()) == 0);  // determinístico
  CHECK(tile_uv(0).x == 0 && tile_uv(0).y == 0);
  CHECK(tile_uv(17).x == 8 && tile_uv(17).y == 8);
  CHECK(tile_uv(255).x == 120 && tile_uv(255).y == 120);
  // Cada tile es distinto de sus vecinos (al menos los primeros 16 ids).
  std::set<std::vector<uint8_t>> uniq;
  for (int id = 0; id < 16; ++id) {
    std::vector<uint8_t> t;
    auto o = tile_uv(uint8_t(id));
    for (int y = 0; y < kTilePx; ++y)
      t.insert(t.end(), &a[((o.y + y) * kAtlasPx + o.x) * 4], &a[((o.y + y) * kAtlasPx + o.x + kTilePx) * 4]);
    uniq.insert(t);
  }
  CHECK(uniq.size() == 16);
  for (std::size_t i = 0; i < a.size(); i += 4) CHECK(a[i] | a[i + 1] | a[i + 2]);  // sin negro puro
  // Autotiling: familia 0 estilo liso — máscara 0 bisela bordes (A más baja);
  // máscara 15 (4 vecinos) es interior, altura llena y sin oscurecer el borde.
  auto at = [&](int id, int x, int y) -> const uint8_t* {
    auto o = tile_uv(uint8_t(id));
    return &a[(std::size_t(o.y + y) * kAtlasPx + (o.x + x)) * 4];
  };
  CHECK(at(0, 7, 4)[3] < 200);     // mask 0, borde E bajo
  CHECK(at(0, 4, 7)[3] < 200);     // borde S
  CHECK(at(15, 7, 4)[3] == 255);   // mask 15, sin bisel
  CHECK(at(15, 4, 7)[3] == 255);
  CHECK(at(15, 4, 4)[3] == 255);
  // mask solo E (2): borde E sin bisel, W sí
  CHECK(at(2, 7, 4)[3] == 255);
  CHECK(at(2, 0, 4)[0] > at(15, 0, 4)[0]);  // W expuesto, más claro que el interior de mask 15
  // misma familia (fila 1 = ids 16..31): interior de dos masks iguales (sin contar el bisel)
  { auto p = at(16, 3, 3), q = at(31, 3, 3);
    CHECK(p[0] == q[0] && p[1] == q[1] && p[2] == q[2] && p[3] == q[3]); }
  if (const char* out = std::getenv("EW_ATLAS_PPM")) {
    FILE* f = std::fopen(out, "wb"); std::fprintf(f, "P6\n%d %d\n255\n", kAtlasPx, kAtlasPx);
    for (std::size_t i = 0; i < a.size(); i += 4) std::fwrite(&a[i], 1, 3, f);
    std::fclose(f);
  }
  std::puts("atlas ok");
  return 0;
}
