// Test sin GL: switch → puerta de hello.ewmap (link, colisión, toggle por proximidad).
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
  const game::Entity* door = scene.find_type("door");
  const game::Entity* sw = scene.find_type("switch");
  CHECK(door && sw && !door->active && !sw->active);
  const int dtx = int(door->x) / 8, dty = int(door->y) / 8;
  CHECK(scene.solid_tile(dtx, dty) && scene.solid_tile(dtx, dty + 2));  // cerrada = sólida
  CHECK(scene.interact(sw->x + 40, sw->y) == -1);                        // lejos: nada
  CHECK(!door->active);

  game::Sim sim(scene);
  auto& p = sim.world().points()[sim.player_index()];
  p.pos = p.prev = {248, 156};  // pasada la pileta 1
  for (int i = 0; i < 400; ++i) sim.step({1, false});
  const float blocked_x = sim.player().pos.x;
  std::printf("bloqueado por la puerta en x=%.1f\n", blocked_x);
  CHECK(blocked_x <= door->x - 4.f + 0.6f);
  // Volver al switch y activarlo (flanco interact).
  p.pos = p.prev = {sw->x, 156};
  for (int i = 0; i < 10; ++i) sim.step({});
  game::Input in; in.interact = true;
  sim.step(in);
  CHECK(sw->active && door->active);
  CHECK(!scene.solid_tile(dtx, dty) && !scene.door_blocks(dtx, dty + 1));
  p.pos = p.prev = {248, 156};
  for (int i = 0; i < 400; ++i) sim.step({1, false});
  std::printf("puerta abierta: llegó a x=%.1f\n", sim.player().pos.x);
  CHECK(sim.player().pos.x > door->x + 8.f);
  // Toggle de vuelta: se cierra.
  scene.interact(sw->x, sw->y);
  CHECK(!sw->active && !door->active && scene.solid_tile(dtx, dty));
  std::puts("door_test: OK");
  return 0;
}
