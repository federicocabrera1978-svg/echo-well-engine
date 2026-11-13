// Test sin GL: vertical slice sobre slice.ewmap (loader + camino start→goal).
// Recorre el camino: switch→puerta, agua, seam, cuerda, checkpoint, salta el hueco, meta.
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
  CHECK(scene.width_px() == 1280 && scene.height_px() == 184);  // 4×1 chunks
  const auto* spawn = scene.find_type("player_spawn");
  const auto* sw = scene.find_type("switch");
  const auto* door = scene.find_type("door");
  const auto* goal = scene.find_type("goal");
  CHECK(spawn && spawn->x == 32.f && spawn->y == 152.f);
  CHECK(sw && door && goal && !sw->active && !door->active);
  CHECK(goal->x == 1184.f && goal->y == 56.f);
  const auto* ck = scene.find_type("checkpoint");
  CHECK(ck && ck->x == 536.f && ck->y == 56.f && !ck->active);
  CHECK(scene.tile(78, 19, map::Layer::META) == game::Sim::kMetaHazard);
  CHECK(scene.tile(80, 19, map::Layer::META) == game::Sim::kMetaHazard);
  CHECK(!scene.solid_tile(78, 8) && !scene.solid_tile(81, 8));  // hueco en la cornisa
  const auto* flood = scene.find_type("flood_switch");
  CHECK(flood && flood->x == 776.f && !flood->active);
  CHECK(scene.solid_tile(100, 15) && scene.solid_tile(141, 15));  // paredes de la pileta
  CHECK(scene.solid_tile(142, 8) && !scene.solid_tile(120, 8));   // cornisa de salida; el hueco es agua
  CHECK(scene.solid_px(200.f + 4.f, 136.f + 4.f));             // puerta cerrada
  CHECK(scene.tile(29, 17, map::Layer::META) == 2);             // zona de agua
  CHECK(scene.tile(145, 6, map::Layer::META) == game::Sim::kMetaGoal);
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
  CHECK(sim.rope_count() == 1 && sim.water().size() == 2);
  CHECK(sim.water()[1].x() == 808.f && sim.water()[1].rest_y() == 136.f);
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
    p2.pos = p2.prev = {1184.f, 56.f};
    sim2.step({});
    CHECK(sim2.finished());
    // META sin entidad cerca: tile META 3 en (145*8+4, 5*8+4) = (1164, 44)
    game::Scene s3;
    if (!s3.load(argv[1], err)) return 1;
    // quitar el alcance al entity alejándolo: pisamos un tile META 3 que no esté
    // a ≤16 px del entity (1184,56). Tile (145,5) centro = (1164, 44); dist a goal ≈ 23.
    game::Sim sim3(s3);
    auto& p3 = sim3.world().points()[sim3.player_index()];
    p3.pos = p3.prev = {1164.f, 44.f};
    sim3.step({});
    CHECK(sim3.finished());
  }

  // Checkpoint + pinchos (unitario, independiente del camino).
  {
    game::Scene s4;
    if (!s4.load(argv[1], err)) return 1;
    game::Sim sim4(s4);
    CHECK(sim4.checkpoint_index() < 0 && sim4.deaths() == 0);
    auto& p4 = sim4.world().points()[sim4.player_index()];
    p4.pos = p4.prev = {536.f, 56.f};  // sobre el checkpoint
    sim4.step({});
    CHECK(sim4.checkpoint() && sim4.checkpoint()->x == 536.f);
    CHECK(s4.find_type("checkpoint")->active);
    // Caer a los pinchos: muere y reaparece en el checkpoint, puerta intacta.
    p4 = sim4.world().points()[sim4.player_index()];
    p4.pos = p4.prev = {640.f, 156.f};
    sim4.step({});
    CHECK(sim4.deaths() == 1);
    CHECK(std::fabs(sim4.player().pos.x - 536.f) < 2.f);
    CHECK(std::fabs(sim4.player().pos.y - 56.f) < 8.f);
    CHECK(sim4.invuln() > 0);
    CHECK(sim4.checkpoint_index() >= 0);
    // Durante invulnerabilidad no re-muere si lo volvemos a poner sobre pinchos.
    auto& p4b = sim4.world().points()[sim4.player_index()];
    p4b.pos = p4b.prev = {640.f, 156.f};
    sim4.step({});
    CHECK(sim4.deaths() == 1);
    // R corto: al checkpoint (no al spawn). R largo: spawn.
    { Input rst; rst.restart = true; sim4.step(rst); }
    CHECK(std::fabs(sim4.player().pos.x - 536.f) < 2.f);
    CHECK(sim4.deaths() == 1);  // respawn no incrementa muertes
    { Input full; full.restart_full = true; sim4.step(full); }
    CHECK(std::fabs(sim4.player().pos.x - 32.f) < 2.f);
    CHECK(!sim4.checkpoint() && sim4.deaths() == 0);
    CHECK(!s4.find_type("checkpoint")->active);
  }

  // Inundación (unitario): el flood_switch sube el agua; sin él no se alcanza la cornisa.
  {
    game::Scene s5;
    if (!s5.load(argv[1], err)) return 1;
    game::Sim sim5(s5);
    auto& w = sim5.water()[1];
    CHECK(w.rest_y() == 136.f && w.initial_rest_y() == 136.f && w.bottom() == 160.f);
    auto& p5 = sim5.world().points()[sim5.player_index()];
    p5.pos = p5.prev = {776.f, 56.f};  // sobre el flood_switch
    { Input a; a.interact = true; sim5.step(a); }
    CHECK(s5.find_type("flood_switch")->active);
    const float bot0 = w.bottom();
    for (int i = 0; i < 150; ++i) sim5.step({});
    CHECK(std::fabs(w.rest_y() - game::Sim::kFloodRestY) < 1.f);
    CHECK(std::fabs(w.bottom() - bot0) < 0.1f);  // el fondo no se mueve
    // Sin inundar: desde el agua no se llega a y≈60.
    game::Scene s6;
    if (!s6.load(argv[1], err)) return 1;
    game::Sim sim6(s6);
    auto& p6 = sim6.world().points()[sim6.player_index()];
    p6.pos = p6.prev = {960.f, 140.f};  // en la pileta, agua baja
    float min_y = 999.f;
    for (int i = 0; i < 180; ++i) {
      Input j; j.jump = (i % 12 == 0); j.move_x = 0;
      sim6.step(j);
      min_y = std::fmin(min_y, sim6.player().pos.y);
    }
    CHECK(min_y > 90.f);  // no alcanza la cornisa (y≈60)
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
  CHECK(sim.checkpoint() && scene.find_type("checkpoint")->active);

  // 5) Caer al hueco (pinchos): reaparece en el checkpoint; la puerta sigue abierta.
  {
    const int d0 = sim.deaths();
    for (int i = 0; i < 250 && sim.deaths() == d0; ++i) sim.step({1.f});
    std::printf("caída pinchos: deaths=%d x=%.1f y=%.1f (checkpoint)\n",
                sim.deaths(), sim.player().pos.x, sim.player().pos.y);
    CHECK(sim.deaths() == d0 + 1);
    CHECK(std::fabs(sim.player().pos.x - ck->x) < 4.f);
    CHECK(scene.find_type("door")->active);  // progreso conservado
  }

  // 6) Saltar el hueco (x=624–656), activar el flood_switch, nadar con el agua alta.
  for (int i = 0; i < 50 && sim.player().pos.x < 600.f; ++i) sim.step({1.f});
  { Input j; j.jump = true; j.move_x = 1; sim.step(j); }
  for (int i = 0; i < 200 && sim.player().pos.x < flood->x - 6.f; ++i) sim.step({1.f});
  std::printf("flood_switch: x=%.1f y=%.1f\n", sim.player().pos.x, sim.player().pos.y);
  CHECK(std::fabs(sim.player().pos.x - flood->x) < 16.f);
  CHECK(sim.player().pos.y < 80.f);
  { Input a; a.interact = true; sim.step(a); }
  CHECK(scene.find_type("flood_switch")->active);
  for (int i = 0; i < 150; ++i) sim.step({});  // el agua sube
  CHECK(std::fabs(sim.water()[1].rest_y() - game::Sim::kFloodRestY) < 1.f);

  // Entrar a la pileta y nadar a la cornisa de salida (x=1136).
  for (int i = 0; i < 40; ++i) sim.step({1.f});
  for (int i = 0; i < 500 && !(sim.player().pos.x > 1136.f && sim.player().pos.y < 80.f); ++i) {
    Input sw; sw.move_x = 1.f; sw.jump = (i % 10 == 0);
    sim.step(sw);
  }
  std::printf("salida inundación: x=%.1f y=%.1f agua=%.1f\n",
              sim.player().pos.x, sim.player().pos.y, sim.water()[1].rest_y());
  CHECK(sim.player().pos.x > 1136.f && sim.player().pos.y < 80.f);
  CHECK(scene.chunk_x() == 3);

  // 7) Caminar a la meta (chunk 3).
  for (int i = 0; i < 400 && !sim.finished(); ++i) sim.step({1.f});
  std::printf("meta: x=%.1f y=%.1f chunk=%d finished=%d cam_x=%.1f\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), int(sim.finished()),
              scene.cam_x());
  CHECK(scene.chunk_x() == 3);
  CHECK(sim.player().pos.x > 960.f);
  CHECK(sim.finished());
  for (int i = 0; i < 60; ++i) sim.step({});
  CHECK(scene.cam_x() >= 940.f);
  CHECK(sim.finished());  // se mantiene
  CHECK(sim.finish_t() >= 0.999f);  // 60 pasos ≥ 0.75 s de fade

  // Congelado: el input de movimiento no saca al player de la meta.
  {
    const auto px = sim.player().pos.x, py = sim.player().pos.y;
    Input go; go.move_x = -1; go.jump = true;
    for (int i = 0; i < 10; ++i) sim.step(go);
    CHECK(sim.finished());
    CHECK(std::fabs(sim.player().pos.x - px) < 8.f);
    (void)py;
  }

  // R corto: vuelve al último checkpoint (ck_exit, puerta sigue abierta). R largo: spawn limpio.
  CHECK(sim.checkpoint() && sim.checkpoint()->x == 1152.f);  // el de la cornisa de salida
  { Input rst; rst.restart = true; sim.step(rst); }
  CHECK(!sim.finished());
  CHECK(sim.finish_t() == 0.f);
  CHECK(!sim.on_rope());
  CHECK(std::fabs(sim.player().pos.x - 1152.f) < 4.f);
  CHECK(std::fabs(sim.player().pos.y - 56.f) < 10.f);
  CHECK(scene.find_type("door")->active);
  CHECK(sim.checkpoint_index() >= 0);
  { Input full; full.restart_full = true; sim.step(full); }
  CHECK(std::fabs(sim.player().pos.x - spawn->x) < 2.f);
  CHECK(std::fabs(sim.player().pos.y - spawn->y) < 8.f);
  CHECK(scene.chunk_x() == 0);
  CHECK(!sim.checkpoint() && sim.deaths() == 0);
  CHECK(!scene.find_type("switch")->active && !scene.find_type("door")->active);
  CHECK(scene.find_type("door")->anim == 0.f);
  CHECK(scene.solid_px(door->x + 4.f, door->y + 4.f));  // puerta sólida otra vez
  for (int i = 0; i < 400; ++i) sim.step({1, false});
  CHECK(sim.player().pos.x <= door->x - 4.f + 1.f);     // bloqueado de nuevo
  std::puts("slice_test: OK");
  return 0;
}
