// Test sin GL: Verlet sobre hello.ewmap (caída, apoyo, caminar, salto, cuerda).
#include "game/sim.hpp"
#include <cmath>
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)
using namespace ew;

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  game::Scene scene;
  std::string err;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  game::Sim sim(scene);
  // Cae desde más arriba y se apoya en el piso (top del piso = y 160).
  sim.world().points()[sim.player_index()].pos.y -= 40;
  sim.world().points()[sim.player_index()].prev.y -= 40;
  for (int i = 0; i < 180; ++i) sim.step({});
  const auto& p = sim.player();
  std::printf("reposo: (%.2f, %.2f) grounded=%d\n", p.pos.x, p.pos.y, int(p.grounded));
  CHECK(std::fabs(p.pos.y - 156.f) < 0.6f);
  CHECK(p.grounded);
  // Caminar a la derecha
  const float x0 = p.pos.x;
  for (int i = 0; i < 60; ++i) sim.step({1, false});
  CHECK(sim.player().pos.x > x0 + 50);
  // Saltar
  sim.step({0, true});
  float min_y = 1e9f;
  for (int i = 0; i < 40; ++i) { sim.step({}); min_y = std::fmin(min_y, sim.player().pos.y); }
  std::printf("altura salto: %.1f px\n", 156.f - min_y);
  CHECK(min_y < 156.f - 20.f);
  for (int i = 0; i < 120; ++i) sim.step({});
  CHECK(std::fabs(sim.player().pos.y - 156.f) < 0.6f);
  // No atraviesa la pared derecha (tile x=39 → x ≥ 312)
  for (int i = 0; i < 300; ++i) sim.step({1, false});
  CHECK(sim.player().pos.x <= 312.f - 4.f + 0.6f);
  // Cuerda: el ancla sigue fija y los segmentos mantienen su largo (±10 %).
  const auto& pts = sim.world().points();
  CHECK(pts[sim.rope_first()].pos.y == 24.f);
  for (const auto& s : sim.world().sticks()) {
    const float len = physics::length(pts[s.b].pos - pts[s.a].pos);
    CHECK(std::fabs(len - s.rest) < s.rest * 0.1f);
  }
  std::puts("physics_test: OK");
  return 0;
}
