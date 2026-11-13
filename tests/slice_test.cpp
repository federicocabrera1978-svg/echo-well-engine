// Test sin GL: vertical slice sobre slice.ewmap (loader + camino start→goal).
// Recorre el camino con input scriptado: switch→puerta, agua, seam, cuerda, meta.
#include "game/sim.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)
using namespace ew;
using game::Input;

// El círculo del player (r) entra al interior de un tile sólido (margen eps).
static bool circle_in_solid(game::Scene& s, physics::Vec2 p, float r, float eps = 0.15f) {
  const float ts = float(map::kTileSize);
  const int x0 = int(std::floor((p.x - r) / ts)), x1 = int(std::floor((p.x + r) / ts));
  const int y0 = int(std::floor((p.y - r) / ts)), y1 = int(std::floor((p.y + r) / ts));
  for (int ty = y0; ty <= y1; ++ty)
    for (int tx = x0; tx <= x1; ++tx) {
      if (!s.solid_tile(tx, ty)) continue;
      const physics::Vec2 c{std::clamp(p.x, tx * ts, (tx + 1) * ts),
                            std::clamp(p.y, ty * ts, (ty + 1) * ts)};
      if (physics::length(p - c) < r - eps) return true;
    }
  return false;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  game::Scene scene;
  std::string err;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  // --- loader: geometría, entidades, capas, cuerda, agua ---
  CHECK(scene.width_px() == 960 && scene.height_px() == 184);  // 3×1 chunks
  const auto* spawn = scene.find_type("player_spawn");
  const auto* sw = scene.find_type("switch");
  const auto* door = scene.find_type("door");
  const auto* goal = scene.find_type("goal");
  CHECK(spawn && spawn->x == 32.f && spawn->y == 152.f);
  CHECK(sw && door && goal && !sw->active && !door->active);
  CHECK(goal->x == 840.f && goal->y == 56.f);
  CHECK(scene.solid_px(200.f + 4.f, 136.f + 4.f));             // puerta cerrada
  CHECK(scene.tile(29, 17, map::Layer::META) == 2);             // zona de agua
  CHECK(scene.tile(105, 6, map::Layer::META) == game::Sim::kMetaGoal);
  CHECK(scene.tile(104, 0, map::Layer::FG) == 230);             // autotile FG: fam 14, máscara E|S
  CHECK(scene.tile(44, 10, map::Layer::VIS) == 167);            // autotile: fam 10, máscara N|E|S
  CHECK(scene.chunk_x() == 0 && scene.chunk_y() == 0);
  // FG de chunk 2 entra al vecindario al ir al chunk 1 (ox=2, local 24,0).
  {
    scene.follow(400, 150);
    CHECK(scene.chunk_x() == 1);
    constexpr int W = game::Scene::kNbW, C = game::Scene::kPackedCh;
    const std::size_t o = (std::size_t(23 + 0) * W + std::size_t(80 + 24)) * C + 2;
    CHECK(scene.active_tiles()[o] == 230);  // mismo FG, packed canal B
    scene.follow(spawn->x, spawn->y);  // volver al spawn para el camino
    CHECK(scene.chunk_x() == 0);
  }

  game::Sim sim(scene);
  CHECK(sim.rope_count() == 1 && sim.water().size() == 1);
  CHECK(sim.rope_points() == 17);
  CHECK(!sim.finished());
  auto& pts = sim.world().points();
  const uint32_t pi = sim.player_index();

  // Trigger de meta: teleport al goal (unitario, independiente del camino).
  {
    game::Scene s2;
    if (!s2.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    game::Sim sim2(s2);
    auto& p2 = sim2.world().points()[sim2.player_index()];
    p2.pos = p2.prev = {840.f, 56.f};
    sim2.step({});
    CHECK(sim2.finished());
    // META sin entidad cerca: tile META 3 en (102*8+4, 5*8+4) = (820, 44)
    game::Scene s3;
    if (!s3.load(argv[1], err)) return 1;
    // quitar el alcance al entity alejándolo: pisamos un tile META 3 que no esté
    // a ≤16 px del entity (840,56). Tile (102,5) centro = (820, 44); dist a goal ≈ 23.
    game::Sim sim3(s3);
    auto& p3 = sim3.world().points()[sim3.player_index()];
    p3.pos = p3.prev = {820.f, 44.f};
    sim3.step({});
    CHECK(sim3.finished());
  }

  // --- camino jugable ---
  // 1) Puerta cerrada bloquea el paso.
  for (int i = 0; i < 400; ++i) sim.step({1, false});
  std::printf("bloqueado por la puerta en x=%.1f\n", sim.player().pos.x);
  CHECK(sim.player().pos.x <= door->x - 4.f + 1.f);
  CHECK(scene.chunk_x() == 0);

  // 2) Volver al switch, activarlo, la puerta se abre.
  pts[pi].pos = pts[pi].prev = {sw->x, 156.f};
  for (int i = 0; i < 10; ++i) sim.step({});
  { Input in; in.interact = true; sim.step(in); }
  CHECK(sw->active && door->active);
  for (int i = 0; i < 30; ++i) sim.step({});  // esperar la animación de apertura
  CHECK(door->anim == 1.f);
  CHECK(!scene.door_blocks(int(door->x) / 8, int(door->y) / 8));

  // 3) Caminar, saltar el borde de la pileta, nadar y cruzar el seam (x=320).
  bool entered_water = false;
  for (int i = 0; i < 900; ++i) {
    Input in;
    in.move_x = 1;
    const float x = sim.player().pos.x;
    if (sim.player_in_water()) { entered_water = true; in.jump = true; }  // nadar hacia arriba
    else if (sim.player().grounded && x > 210.f && x < 360.f) in.jump = true;  // saltar bordes
    sim.step(in);
    if (sim.player().pos.x > 360.f && scene.chunk_x() == 1) break;
  }
  std::printf("después del agua: x=%.1f y=%.1f agua=%d chunk=%d\n",
              sim.player().pos.x, sim.player().pos.y, int(entered_water), scene.chunk_x());
  CHECK(entered_water);
  CHECK(sim.player().pos.x > 320.f);
  CHECK(scene.chunk_x() == 1);
  // Dead-zone / slide: la cámara está en transición o ya en la pantalla 1.
  CHECK(scene.cam_x() > 0.f);
  for (int i = 0; i < 60; ++i) sim.step({});
  CHECK(scene.cam_x() >= 300.f);  // terminó el slide hacia el chunk 1 (origen 320)

  // 4) Cuerda: acercarse, saltar, agarrar, trepar, saltar a la cornisa.
  const float rx = pts[sim.rope_first()].pos.x;
  CHECK(std::fabs(rx - 392.f) < 0.5f);
  // Caminar hasta debajo de la cuerda (la pared en x=400 bloquea más allá).
  for (int i = 0; i < 400 && sim.player().pos.x < rx - 6.f; ++i) sim.step({1, false});
  std::printf("bajo la cuerda: x=%.1f y=%.1f\n", sim.player().pos.x, sim.player().pos.y);
  // Saltar y agarrar durante la subida.
  { Input j; j.jump = true; j.move_x = 1; sim.step(j); }
  for (int i = 0; i < 50 && !sim.on_rope(); ++i) {
    Input in;
    in.move_x = 0.3f;
    in.grab = true;
    sim.step(in);
  }
  std::printf("agarre: on_rope=%d nodo=%d y=%.1f\n", int(sim.on_rope()), sim.rope_node(),
              sim.player().pos.y);
  CHECK(sim.on_rope());
  CHECK(pts[sim.rope_first() + uint32_t(sim.rope_node())].radius == game::Sim::kPlayerRadius);

  // Colgado contra la pared (tile 50, x=400): el círculo del player (r=4) no entra.
  int wall_hits = 0;
  float hang_max_x = 0;
  for (int i = 0; i < 120; ++i) {
    sim.step({1.f});
    hang_max_x = std::fmax(hang_max_x, sim.player().pos.x);
    if (circle_in_solid(scene, {sim.player().pos.x, sim.player().pos.y}, game::Sim::kPlayerRadius))
      ++wall_hits;
  }
  std::printf("colgado vs pared: max_x=%.1f hits=%d (pared x=400)\n", hang_max_x, wall_hits);
  CHECK(sim.on_rope());
  CHECK(wall_hits == 0);
  CHECK(hang_max_x <= 400.f - game::Sim::kPlayerRadius + 0.2f);

  // Trepar hasta cerca del tope (cornisa a y=64; queremos y ≲ 60).
  { Input up; up.climb = -1;
    for (int i = 0; i < 240 && sim.player().pos.y > 50.f; ++i) sim.step(up); }
  std::printf("trepó a nodo %d y=%.1f\n", sim.rope_node(), sim.player().pos.y);
  CHECK(sim.on_rope());
  CHECK(sim.player().pos.y < 70.f);

  // Balancear a la derecha y saltar a la cornisa (tope de la pared + plataforma, y=64).
  for (int i = 0; i < 25; ++i) sim.step({1.f});
  { Input j; j.jump = true; j.move_x = 1; sim.step(j); }
  for (int i = 0; i < 90; ++i) sim.step({1.f});
  std::printf("cornisa: x=%.1f y=%.1f on_rope=%d grounded=%d\n",
              sim.player().pos.x, sim.player().pos.y, int(sim.on_rope()),
              int(sim.player().grounded));
  CHECK(!sim.on_rope());
  for (int i = 0; i < sim.rope_points(); ++i)
    CHECK(pts[sim.rope_first() + uint32_t(i)].radius == game::Sim::kRopeRadius);  // al soltar, radio original
  CHECK(sim.player().pos.y < 80.f);          // sobre la cornisa (stand ≈ 60)
  CHECK(sim.player().pos.x > 400.f);         // a la derecha de la cuerda / sobre la pared

  // 5) Caminar por la cornisa, cruzar el seam x=640 (chunk 2) y tocar el goal.
  for (int i = 0; i < 800 && !sim.finished(); ++i) sim.step({1.f});
  std::printf("meta: x=%.1f y=%.1f chunk=%d finished=%d cam_x=%.1f\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), int(sim.finished()),
              scene.cam_x());
  CHECK(scene.chunk_x() == 2);
  CHECK(sim.player().pos.x > 640.f);
  CHECK(sim.finished());
  // La cámara llegó a la pantalla del chunk 2 (origen 640) tras el slide.
  for (int i = 0; i < 60; ++i) sim.step({});
  CHECK(scene.cam_x() >= 620.f);
  CHECK(sim.finished());  // se mantiene
  std::puts("slice_test: OK");
  return 0;
}
