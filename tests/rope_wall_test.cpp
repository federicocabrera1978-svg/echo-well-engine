// Test sin GL y sin mapa: segmentos de cuerda no atraviesan tiles sólidos.
#include "physics/verlet.hpp"
#include <cmath>
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)
using namespace ew::physics;

// Pared vertical en tx=10 (x=80..88), piso en ty>=20 (y>=160), bordes cerrados.
static bool wall(int tx, int ty) {
  if (tx < 0 || ty < 0 || tx == 10 || ty >= 20) return true;
  return false;
}

// Bloque 4×4 tiles en (10..13, 10..13) = (80..112, 80..112), piso ty>=22.
static bool block(int tx, int ty) {
  if (tx < 0 || ty < 0 || ty >= 22) return true;
  return tx >= 10 && tx <= 13 && ty >= 10 && ty <= 13;
}

// El segmento entra al INTERIOR de un tile sólido (no cuenta rozar la cara).
static bool segment_through(Vec2 a, Vec2 b, const SolidFn& solid, float ts = 8.f) {
  const float len = length(b - a);
  const int n = std::max(8, int(std::ceil(len / 1.5f)));
  for (int i = 1; i < n; ++i) {  // extremos: los cubre la colisión de puntos
    const float t = float(i) / float(n);
    const Vec2 p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    const int tx = int(std::floor(p.x / ts)), ty = int(std::floor(p.y / ts));
    if (!solid(tx, ty)) continue;
    const float l = tx * ts, r = (tx + 1) * ts, u = ty * ts, d = (ty + 1) * ts;
    // interior estricto (margen 0.2 px): estar sobre la cara no cuenta
    if (p.x > l + 0.2f && p.x < r - 0.2f && p.y > u + 0.2f && p.y < d - 0.2f) return true;
  }
  return false;
}

static bool any_stick_through(const World& w, const SolidFn& solid) {
  const auto& pts = w.points();
  for (const auto& s : w.sticks())
    if (segment_through(pts[s.a].pos, pts[s.b].pos, solid)) return true;
  return false;
}

int main() {
  // 1) Stick que ya atraviesa la pared: tras simular, ambos extremos del mismo
  // lado y el segmento no cruza el interior.
  {
    World w;
    w.tile_size = 8;
    const auto ia = w.add_point({70, 100}, 1.5f, 1);
    const auto ib = w.add_point({100, 100}, 1.5f, 1);
    w.add_stick(ia, ib);
    CHECK(segment_through(w.points()[ia].pos, w.points()[ib].pos, wall));  // precondición
    for (int i = 0; i < 90; ++i) w.step(1.f / 60.f, wall);
    const auto& a = w.points()[ia];
    const auto& b = w.points()[ib];
    std::printf("spanning: a=(%.1f,%.1f) b=(%.1f,%.1f) len=%.1f through=%d\n",
                a.pos.x, a.pos.y, b.pos.x, b.pos.y, length(b.pos - a.pos),
                int(segment_through(a.pos, b.pos, wall)));
    CHECK(!segment_through(a.pos, b.pos, wall));
    const bool left = a.pos.x < 80.f && b.pos.x < 80.f;
    const bool right = a.pos.x > 88.f && b.pos.x > 88.f;
    CHECK(left || right);
    CHECK(std::fabs(length(b.pos - a.pos) - 30.f) < 30.f * 0.15f);
  }

  // 2) Cuerda que se balancea contra la pared: ningún segmento atraviesa,
  // el ancla queda fija, los nodos no entran al interior.
  {
    World w;
    w.tile_size = 8;
    const auto first = w.add_rope({64, 24}, 16, 6, 1.5f);
    auto& pts = w.points();
    int hits = 0;
    for (int i = 0; i < 240; ++i) {
      for (int k = 1; k <= 16; ++k) pts[first + uint32_t(k)].force += {3000, 0};
      w.step(1.f / 60.f, wall);
      if (any_stick_through(w, wall)) ++hits;
    }
    std::printf("swing: anchor=(%.1f,%.1f) tip=(%.1f,%.1f) hits=%d\n",
                pts[first].pos.x, pts[first].pos.y,
                pts[first + 16].pos.x, pts[first + 16].pos.y, hits);
    CHECK(pts[first].pos.x == 64.f && pts[first].pos.y == 24.f);
    CHECK(hits == 0);
    CHECK(!any_stick_through(w, wall));
    for (int k = 1; k <= 16; ++k) CHECK(pts[first + uint32_t(k)].pos.x <= 80.f - 1.5f + 0.2f);
  }

  // 3) Envolverse en una esquina: cuerda al costado de un bloque, se empuja
  // por encima. El segmento no corta el interior; el ancla no se mueve.
  {
    World w;
    w.tile_size = 8;
    // Ancla a la izquierda del bloque (bloque x=80..112, y=80..112). 16×6=96.
    const auto first = w.add_rope({72, 48}, 16, 6, 1.5f);
    auto& pts = w.points();
    int hits = 0;
    for (int i = 0; i < 300; ++i) {
      for (int k = 1; k <= 16; ++k) pts[first + uint32_t(k)].force += {2000, -800};
      w.step(1.f / 60.f, block);
      if (any_stick_through(w, block)) ++hits;
    }
    std::printf("corner: hits=%d tip=(%.1f,%.1f) anchor=(%.1f,%.1f)\n", hits,
                pts[first + 16].pos.x, pts[first + 16].pos.y,
                pts[first].pos.x, pts[first].pos.y);
    CHECK(pts[first].pos.x == 72.f && pts[first].pos.y == 48.f);
    CHECK(hits == 0);
    CHECK(!any_stick_through(w, block));
    // Algún nodo llegó a la zona de la esquina / encima del bloque (se envolvió).
    float max_x = 0, min_y = 1e9f;
    for (int k = 0; k <= 16; ++k) {
      max_x = std::fmax(max_x, pts[first + uint32_t(k)].pos.x);
      min_y = std::fmin(min_y, pts[first + uint32_t(k)].pos.y);
    }
    std::printf("corner reach: max_x=%.1f min_y=%.1f\n", max_x, min_y);
    CHECK(max_x > 70.f);
  }

  std::puts("rope_wall_test: OK");
  return 0;
}
