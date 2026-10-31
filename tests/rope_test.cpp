// Test sin GL: cuerda jugable (agarrar, balancear, trepar, soltar con salto).
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
  auto& pts = sim.world().points();
  const uint32_t pi = sim.player_index();
  for (int i = 0; i < 60; ++i) sim.step({});  // la cuerda se asienta
  // Lejos de la cuerda: grab no hace nada.
  game::Input g; g.grab = true;
  sim.step(g);
  CHECK(!sim.on_rope());
  // Poner al player junto al extremo inferior y agarrar.
  const auto tip = pts[sim.rope_first() + uint32_t(sim.rope_points() - 1)].pos;
  pts[pi].pos = pts[pi].prev = tip + physics::Vec2{3, 0};
  sim.step(g);
  CHECK(sim.on_rope());
  const int n0 = sim.rope_node();
  std::printf("agarrado en nodo %d\n", n0);
  CHECK(n0 >= 14);
  // Colgado no cae al piso (y < 156) y sigue al nodo.
  for (int i = 0; i < 60; ++i) sim.step({});
  CHECK(sim.on_rope());
  CHECK(sim.player().pos.y < 140.f);
  // Balanceo: empujar a la derecha 40 pasos, luego a la izquierda: amplitud horizontal.
  const float ax = pts[sim.rope_first()].pos.x;
  float max_dx = 0;
  for (int k = 0; k < 4; ++k)
    for (int i = 0; i < 40; ++i) {
      sim.step({k % 2 ? -1.f : 1.f});
      max_dx = std::fmax(max_dx, std::fabs(sim.player().pos.x - ax));
    }
  std::printf("amplitud balanceo: %.1f px\n", max_dx);
  CHECK(max_dx > 25.f);
  for (int i = 0; i < 120; ++i) sim.step({});
  // Trepar hacia arriba: el nodo baja de índice y el player sube.
  const float y0 = sim.player().pos.y;
  game::Input up; up.climb = -1;
  for (int i = 0; i < 120; ++i) sim.step(up);
  std::printf("trepó a nodo %d (y %.1f -> %.1f)\n", sim.rope_node(), y0, sim.player().pos.y);
  CHECK(sim.rope_node() <= n0 - 10);
  CHECK(sim.player().pos.y < y0 - 40.f);
  // Bajar de nuevo un poco.
  game::Input down; down.climb = 1;
  const int n1 = sim.rope_node();
  for (int i = 0; i < 30; ++i) sim.step(down);
  CHECK(sim.rope_node() > n1);
  // Saltar suelta y da impulso hacia arriba.
  const float yj = sim.player().pos.y;
  game::Input j; j.jump = true;
  sim.step(j);
  CHECK(!sim.on_rope());
  float min_y = yj;
  for (int i = 0; i < 20; ++i) { sim.step({}); min_y = std::fmin(min_y, sim.player().pos.y); }
  CHECK(min_y < yj - 10.f);
  // Termina en el piso; la cuerda mantiene su largo y el ancla fija.
  for (int i = 0; i < 240; ++i) sim.step({});
  CHECK(std::fabs(sim.player().pos.y - 156.f) < 0.6f);
  CHECK(pts[sim.rope_first()].pos.y == 24.f);
  for (const auto& s : sim.world().sticks()) {
    const float len = physics::length(pts[s.b].pos - pts[s.a].pos);
    CHECK(std::fabs(len - s.rest) < s.rest * 0.15f);
  }
  // Cuerdas desde el mapa: hello.yaml define 2 (sección ROPE).
  CHECK(sim.rope_count() == 2);
  CHECK(pts[sim.rope_first(0)].pos.x == 128.f);
  CHECK(pts[sim.rope_first(1)].pos.x == 440.f && pts[sim.rope_first(1)].pos.y == 24.f);
  CHECK(sim.rope_points(1) == 11);
  // Agarrar la 2ª cuerda: el player queda en esa cuerda y trepa dentro de ella.
  const auto tip2 = pts[sim.rope_first(1) + uint32_t(sim.rope_points(1) - 1)].pos;
  pts[pi].pos = pts[pi].prev = tip2 + physics::Vec2{3, 0};
  sim.step(g);
  CHECK(sim.on_rope() && sim.held_rope() == 1);
  CHECK(sim.rope_node() >= 8);
  const int m0 = sim.rope_node();
  game::Input up2; up2.climb = -1;
  for (int i = 0; i < 30; ++i) sim.step(up2);
  CHECK(sim.held_rope() == 1 && sim.rope_node() < m0);
  CHECK(std::fabs(sim.player().pos.x - 440.f) < 40.f);
  std::puts("rope_test: OK");
  return 0;
}
