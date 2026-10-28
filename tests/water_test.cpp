// Test sin GL: agua Hooke de hello.ewmap (ondas, amortiguación, flotación).
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
  CHECK(sim.water().size() == 2);
  auto& w = sim.water()[0];
  CHECK(w.x() == 160 && w.rest_y() == 136 && w.bottom() == 160);
  // Contención: cada cuerpo tiene piso sólido y paredes sólidas a ambos lados hasta la superficie.
  for (const auto& b : sim.water()) {
    const int l = int(b.x()) / 8 - 1, r = int(b.x() + b.width()) / 8;
    for (int ty = int(b.rest_y()) / 8; ty < int(b.bottom()) / 8; ++ty)
      CHECK(scene.solid_tile(l, ty) && scene.solid_tile(r, ty));
    for (int tx = l + 1; tx < r; ++tx) CHECK(scene.solid_tile(tx, int(b.bottom()) / 8));
  }
  // Resorte aislado: un impulso se propaga y se amortigua.
  physics::WaterBody solo(0, 0, 64, 24, 0.02f, 0.04f);
  solo.disturb(32, 3.f);
  solo.step();
  const float e0 = solo.energy();
  float edge_max = 0;
  for (int i = 0; i < 60; ++i) { solo.step(); edge_max = std::fmax(edge_max, std::fabs(solo.surface_col(2))); }
  CHECK(edge_max > 0.05f);  // la onda llegó al borde
  for (int i = 0; i < 600; ++i) solo.step();
  CHECK(solo.energy() < e0 * 0.01f);
  // Player cae al agua desde arriba: salpica y flota cerca de la superficie.
  auto& p = sim.world().points()[sim.player_index()];
  p.pos = p.prev = {192, 100};
  float max_disp = 0;
  for (int i = 0; i < 90; ++i) {
    sim.step({});
    for (int c = 0; c < w.cols(); ++c) max_disp = std::fmax(max_disp, std::fabs(w.surface_col(c) - w.rest_y()));
  }
  std::printf("salpicadura max: %.2f px\n", max_disp);
  CHECK(max_disp > 1.f);
  for (int i = 0; i < 600; ++i) sim.step({});
  std::printf("flota en y=%.2f (superficie %.0f, fondo %.0f)\n", sim.player().pos.y, w.rest_y(), w.bottom());
  CHECK(sim.player().pos.y < w.bottom() - 6.f && sim.player().pos.y > w.rest_y() - 6.f);
  CHECK(sim.player_in_water());
  // Segundo cuerpo (chunk 1) también simula: perturbación se amortigua dentro de su rect.
  auto& w2 = sim.water()[1];
  w2.disturb(w2.x() + 8, 4.f);
  for (int i = 0; i < 30; ++i) sim.step({});
  for (int c = 0; c < w2.cols(); ++c) CHECK(w2.surface_col(c) < w2.bottom() && w2.surface_col(c) > w2.rest_y() - 24);
  std::puts("water_test: OK");
  return 0;
}
