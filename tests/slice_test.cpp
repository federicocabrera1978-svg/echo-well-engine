// Test sin GL: vertical slice sobre slice.ewmap (loader + camino start→goal).
// Recorre el camino: switch→puerta, agua, seam, cuerda, checkpoint, salta el hueco, meta.
// Sala 3 (chunk 4): con el agua baja, switch_well abre el pasillo; cuerda 2 → repisa → huevo.
// Sala 4 (chunk 5): plataformas móviles sobre un foso de pinchos.
// Sala 5 (chunk 6): gravedad invertida (orbes), techo con hueco y plataforma colgante → meta.
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

// Sala 4: desde la repisa de entrada (o cualquier punto antes de x=1640 sobre la repisa del
// chunk 4/5), esperar a plat_h, viajar encima, bajar al pilar, esperar a lift_v arriba,
// subir, bajar con él y caminar hasta el checkpoint de la sala 5. Devuelve los pasos usados
// (o -1 si no llegó).
static float steer(float x, float target) { return std::clamp((target - x) / 6.f, -1.f, 1.f); }
static int solve_room4(game::Sim& sim, int max_steps = 3000) {
  const auto& plat = sim.mover(0);  // plat_h
  const auto& lift = sim.mover(1);  // lift_v
  int phase = 0, steps = 0;
  for (; steps < max_steps && !sim.finished(); ++steps) {
    const float x = sim.player().pos.x;
    Input in;
    switch (phase) {
      case 0:  // caminar hasta el borde de la repisa de entrada
        in.move_x = steer(x, 1636.f);
        if (std::fabs(x - 1636.f) < 1.5f && plat.x <= 1648.5f && sim.player().grounded) phase = 1;
        break;
      case 1:  // subir a plat_h y quedarse en el centro
        in.move_x = steer(x, plat.x + 12.f);
        if (plat.x >= 1759.5f && sim.ridden() == 0) phase = 2;
        break;
      case 2:  // bajar al pilar
        in.move_x = steer(x, 1796.f);
        if (std::fabs(x - 1796.f) < 1.5f && lift.y <= 64.5f && sim.player().grounded) phase = 3;
        break;
      case 3:  // subir a lift_v
        in.move_x = steer(x, lift.x + 12.f);
        if (lift.y >= 135.5f && sim.ridden() == 1) phase = 4;
        break;
      default:  // piso seguro → puerta a la sala 5
        in.move_x = 1.f;
        break;
    }
    sim.step(in);
    if (sim.checkpoint() && sim.checkpoint()->name == "ck_grav") return steps + 1;
  }
  return -1;
}

// Sala 5: desde ck_grav, caminar al orbe del piso (gravedad invertida), quedarse quieto hasta
// "caer" al techo, esperar a plat_inv al borde del hueco, cruzar colgado de su cara de abajo,
// seguir por el techo hasta el orbe que devuelve la gravedad y caer al piso de la meta.
static int solve_room5(game::Sim& sim, int max_steps = 3000) {
  const auto& plat = sim.mover(2);  // plat_inv
  int phase = 0, steps = 0;
  for (; steps < max_steps && !sim.finished(); ++steps) {
    const float x = sim.player().pos.x;
    Input in;
    switch (phase) {
      case 0:  // hasta el orbe del piso
        in.move_x = 1.f;
        if (sim.gravity_flipped()) phase = 1;
        break;
      case 1:  // cae hacia el techo
        if (sim.player().grounded && sim.player().pos.y < 32.f) phase = 2;
        break;
      case 2:  // borde del hueco del techo
        in.move_x = steer(x, 2036.f);
        if (std::fabs(x - 2036.f) < 1.5f && plat.x <= 2048.5f && sim.player().grounded) phase = 3;
        break;
      case 3:  // colgado de plat_inv
        in.move_x = steer(x, plat.x + 12.f);
        if (plat.x >= 2119.5f && sim.ridden() == 2) phase = 4;
        break;
      default:  // techo → orbe → piso de la meta
        in.move_x = 1.f;
        break;
    }
    sim.step(in);
  }
  return sim.finished() ? steps : -1;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  game::Scene scene;
  std::string err;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  // --- loader: geometría, entidades, capas, cuerda, agua ---
  CHECK(scene.width_px() == 2240 && scene.height_px() == 184);  // 7×1 chunks
  const auto* spawn = scene.find_type("player_spawn");
  const auto* sw = scene.find_type("switch");
  const auto* door = scene.find_type("door");
  const auto* goal = scene.find_type("goal");
  CHECK(spawn && spawn->x == 32.f && spawn->y == 152.f);
  CHECK(sw && door && goal && !sw->active && !door->active);
  CHECK(goal->x == 2208.f && goal->y == 152.f);
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
  CHECK(scene.tile(275, 18, map::Layer::META) == game::Sim::kMetaGoal);
  CHECK(scene.tile(235, 18, map::Layer::META) == 0);             // ni en la sala 4
  // Sala 5: puerta al piso desde la sala 4, techo con hueco, pinchos arriba y abajo.
  CHECK(scene.solid_tile(239, 16) && !scene.solid_tile(239, 18));
  CHECK(scene.solid_tile(250, 2) && !scene.solid_tile(256, 2) && !scene.solid_tile(267, 1));
  CHECK(scene.solid_tile(268, 2));
  CHECK(scene.tile(260, 1, map::Layer::META) == game::Sim::kMetaHazard);
  CHECK(scene.tile(247, 19, map::Layer::META) == game::Sim::kMetaHazard);
  CHECK(scene.tile(270, 19, map::Layer::META) == game::Sim::kMetaHazard);
  CHECK(scene.tile(271, 19, map::Layer::META) == 0);
  CHECK(scene.tile(145, 6, map::Layer::META) == 0);             // la meta ya no está en el chunk 3
  // Sala 4: hueco hacia la sala 4 sobre la repisa, repisa de entrada, pilar, pinchos.
  CHECK(!scene.solid_tile(199, 7) && scene.solid_tile(199, 8) && scene.solid_tile(199, 15));
  CHECK(scene.solid_tile(200, 8) && scene.solid_tile(205, 8) && !scene.solid_tile(206, 8));
  CHECK(scene.solid_tile(224, 8) && scene.solid_tile(224, 19) && !scene.solid_tile(226, 8));
  CHECK(scene.tile(210, 19, map::Layer::META) == game::Sim::kMetaHazard);
  CHECK(scene.tile(228, 19, map::Layer::META) == game::Sim::kMetaHazard);
  CHECK(scene.tile(230, 19, map::Layer::META) == 0);  // piso seguro de la meta
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
  CHECK(sim.rope_count() == 2 && sim.water().size() == 2);
  CHECK(sim.collectible_total() == 1 && sim.collected() == 0);
  CHECK(sim.water()[1].x() == 808.f && sim.water()[1].rest_y() == 136.f);
  CHECK(sim.rope_points() == 17);
  CHECK(!sim.finished());
  CHECK(sim.mover_count() == 3);
  CHECK(sim.mover(2).x == 2048.f && sim.mover(2).y == 16.f);
  CHECK(!sim.gravity_flipped());
  CHECK(sim.mover(0).x == 1648.f && sim.mover(0).y == 64.f && sim.mover(0).w == 24.f);
  CHECK(sim.mover(1).x == 1808.f && sim.mover(1).y == 64.f);
  auto& pts = sim.world().points();
  const uint32_t pi = sim.player_index();

  // Trigger de meta: teleport al goal (unitario, independiente del camino).
  {
    game::Scene s2;
    if (!s2.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    game::Sim sim2(s2);
    auto& p2 = sim2.world().points()[sim2.player_index()];
    p2.pos = p2.prev = {2208.f, 152.f};
    sim2.step({});
    CHECK(sim2.finished());
    // META sin entidad cerca: tile META 3 (272,17) centro = (2180, 140); dist a goal ≈ 30.
    game::Scene s3;
    if (!s3.load(argv[1], err)) return 1;
    game::Sim sim3(s3);
    auto& p3 = sim3.world().points()[sim3.player_index()];
    p3.pos = p3.prev = {2180.f, 140.f};
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
    CHECK(sim4.take_sfx() & 2u);  // kSfxDie
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

  // Sala 3: puertas del pasillo + huevo (unitario). Las puertas cierran la pared de la pileta.
  {
    game::Scene s7;
    if (!s7.load(argv[1], err)) return 1;
    game::Sim sim7(s7);
    CHECK(s7.solid_tile(141, 15) && s7.solid_tile(141, 18));   // door_well_hi / _lo cerradas
    CHECK(s7.solid_tile(141, 12) && !s7.solid_tile(150, 15));  // pared fija arriba; pasillo libre
    CHECK(!s7.solid_tile(167, 8) && s7.solid_tile(165, 8) && s7.solid_tile(172, 8));  // hueco de la cuerda 2
    CHECK(sim7.rope_first(1) > sim7.rope_first(0) && sim7.rope_points(1) == 17);
    const auto* egg = s7.find_type("collectible");
    CHECK(egg && egg->x == 1440.f && egg->y == 56.f && !egg->active);
    // Teleport al huevo: se agarra (SFX + destello), y sobrevive a muerte/R corto.
    auto& p7 = sim7.world().points()[sim7.player_index()];
    p7.pos = p7.prev = {1440.f, 60.f};
    sim7.take_sfx();
    sim7.step({});
    CHECK(sim7.collected() == 1 && egg->active);
    CHECK(sim7.take_sfx() & 64u);  // kSfxPickup
    CHECK(sim7.pickup_flash() > 0);
    sim7.step({});
    CHECK(sim7.collected() == 1);  // no se cuenta dos veces
    { Input rst; rst.restart = true; sim7.step(rst); }
    CHECK(sim7.collected() == 1);
    auto& p7b = sim7.world().points()[sim7.player_index()];
    p7b.pos = p7b.prev = {700.f, 156.f};  // pinchos
    for (int i = 0; i < 40; ++i) sim7.step({});
    CHECK(sim7.deaths() >= 1 && sim7.collected() == 1);
    { Input full; full.restart_full = true; sim7.step(full); }
    CHECK(sim7.collected() == 0 && !egg->active);  // R largo: partida limpia
    // Sin el switch_well, nadando contra las puertas no se entra al pasillo.
    auto& p7c = sim7.world().points()[sim7.player_index()];
    p7c.pos = p7c.prev = {1100.f, 134.f};
    for (int i = 0; i < 240; ++i) { Input in; in.move_x = 1.f; in.jump = (i % 15 == 0); sim7.step(in); }
    CHECK(sim7.player().pos.x < 1128.f && sim7.collected() == 0);
  }

  // Sala 3 jugable: desde el checkpoint de la cornisa, SIN inundar: saltar el hueco, caer a la
  // pileta baja, nadar al switch_well, abrir, nadar por la puerta al pasillo, cuerda 2, repisa,
  // huevo; volver por arriba saltando el hueco de la cuerda y llegar a la meta con 1/1.
  {
    game::Scene s8;
    if (!s8.load(argv[1], err)) return 1;
    game::Sim sim8(s8);
    auto& P = sim8.world().points();
    const uint32_t pi8 = sim8.player_index();
    P[pi8].pos = P[pi8].prev = {536.f, 56.f};
    sim8.step({});
    CHECK(sim8.checkpoint() && sim8.checkpoint()->x == 536.f);
    for (int i = 0; i < 50 && sim8.player().pos.x < 600.f; ++i) sim8.step({1.f});
    { Input j; j.jump = true; j.move_x = 1; sim8.step(j); }
    for (int i = 0; i < 200 && sim8.player().pos.x < 790.f; ++i) sim8.step({1.f});
    CHECK(sim8.player().pos.y < 80.f && sim8.deaths() == 0);  // cruzó el hueco de pinchos
    // Caer a la pileta baja y nadar a la derecha hasta el switch_well.
    const auto* sww = &s8.entities()[0];
    for (const auto& e : s8.entities()) if (e.name == "switch_well") sww = &e;
    CHECK(sww->name == "switch_well" && !sww->active);
    bool wet = false;
    for (int i = 0; i < 900 && sim8.player().pos.x < sww->x - 2.f; ++i) {
      Input in; in.move_x = 1.f;
      sim8.step(in);
      wet = wet || sim8.player_in_water();
    }
    std::printf("sala3 switch_well: x=%.1f y=%.1f agua=%d\n", sim8.player().pos.x, sim8.player().pos.y, int(wet));
    CHECK(wet && sim8.water()[1].rest_y() == 136.f);  // agua baja (sin flood)
    { Input a; a.interact = true; sim8.step(a); }
    CHECK(sww->active);
    for (int i = 0; i < 30; ++i) sim8.step({});
    CHECK(!s8.solid_tile(141, 15) && !s8.solid_tile(141, 18));  // puertas abiertas
    // Nadar por la puerta al pasillo y caminar hasta debajo de la cuerda 2.
    const float rx2 = P[sim8.rope_first(1)].pos.x;
    CHECK(std::fabs(rx2 - 1352.f) < 0.5f);
    for (int i = 0; i < 900 && sim8.player().pos.x < rx2 - 6.f; ++i) sim8.step({1.f});
    std::printf("sala3 bajo cuerda 2: x=%.1f y=%.1f chunk=%d\n", sim8.player().pos.x, sim8.player().pos.y, s8.chunk_x());
    CHECK(sim8.player().pos.x > 1340.f && sim8.player().pos.y > 140.f);
    CHECK(s8.chunk_x() == 4);
    // Misma receta que la cuerda 1: saltar, agarrar, trepar, balancear y saltar a la repisa.
    { Input j; j.jump = true; j.move_x = 1; sim8.step(j); }
    for (int i = 0; i < 50 && !sim8.on_rope(); ++i) { Input in; in.move_x = 0.3f; in.grab = true; sim8.step(in); }
    CHECK(sim8.on_rope() && sim8.held_rope() == 1);
    for (int i = 0; i < 120; ++i) sim8.step({1.f});
    { Input up; up.climb = -1;
      for (int i = 0; i < 240 && sim8.player().pos.y > 50.f; ++i) sim8.step(up); }
    CHECK(sim8.on_rope() && sim8.player().pos.y < 70.f);
    for (int i = 0; i < 25; ++i) sim8.step({1.f});
    { Input j; j.jump = true; j.move_x = 1; sim8.step(j); }
    for (int i = 0; i < 200 && sim8.collected() == 0; ++i) sim8.step({1.f});
    std::printf("sala3 huevo: x=%.1f y=%.1f huevos=%d/%d\n", sim8.player().pos.x, sim8.player().pos.y,
                sim8.collected(), sim8.collectible_total());
    CHECK(sim8.collected() == 1 && sim8.collectible_total() == 1);
    CHECK(sim8.player().pos.y < 80.f);  // sobre la repisa
    // La repisa sigue a la sala 4: checkpoint de entrada y plataformas hasta la meta.
    for (int i = 0; i < 300 && !(sim8.checkpoint() && sim8.checkpoint()->name == "ck_mover"); ++i)
      sim8.step({1.f});
    CHECK(sim8.checkpoint() && sim8.checkpoint()->name == "ck_mover");
    CHECK(s8.chunk_x() == 5 && sim8.player().pos.y < 70.f);
    const int st = solve_room4(sim8);
    CHECK(st > 0 && sim8.deaths() == 0);
    const int st5 = solve_room5(sim8);
    std::printf("sala4+5 meta: pasos=%d+%d x=%.1f y=%.1f muertes=%d\n", st, st5, sim8.player().pos.x,
                sim8.player().pos.y, sim8.deaths());
    CHECK(st5 > 0 && sim8.finished() && sim8.collected() == 1 && sim8.deaths() == 0);
    CHECK(!sim8.gravity_flipped());
    // R corto: el huevo se conserva; R largo: vuelve a 0/1.
    { Input rst; rst.restart = true; sim8.step(rst); }
    CHECK(!sim8.finished() && sim8.collected() == 1);
    { Input full; full.restart_full = true; sim8.step(full); }
    CHECK(sim8.collected() == 0 && !s8.find_type("collectible")->active);
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

  // 7) Cornisa de salida → saltar el hueco de la cuerda 2 → repisa del huevo → sala 4.
  CHECK(sim.checkpoint() && sim.checkpoint()->x == 1152.f);  // ck_exit
  for (int i = 0; i < 400 && sim.player().pos.x < 1316.f; ++i) sim.step({1.f});
  { Input j; j.jump = true; j.move_x = 1; sim.step(j); }
  for (int i = 0; i < 400 && !(sim.checkpoint() && sim.checkpoint()->name == "ck_mover"); ++i)
    sim.step({1.f});
  std::printf("entrada sala 4: x=%.1f y=%.1f chunk=%d muertes=%d\n", sim.player().pos.x,
              sim.player().pos.y, scene.chunk_x(), sim.deaths());
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_mover");
  CHECK(!sim.finished() && sim.player().pos.y < 70.f);
  const int d_room4 = sim.deaths();
  // Caer al foso de la sala 4: muere y vuelve a ck_mover; las plataformas se reinician.
  for (int i = 0; i < 200 && sim.deaths() == d_room4; ++i) sim.step({1.f});
  CHECK(sim.deaths() == d_room4 + 1);
  CHECK(std::fabs(sim.player().pos.x - 1620.f) < 4.f);
  CHECK(sim.mover(0).x == 1648.f && sim.mover(1).y == 64.f);
  for (int i = 0; i < 40; ++i) sim.step({});  // fin de la invulnerabilidad
  const int steps4 = solve_room4(sim);
  std::printf("sala 4 → ck_grav: pasos=%d x=%.1f y=%.1f\n", steps4, sim.player().pos.x, sim.player().pos.y);
  CHECK(steps4 > 0 && sim.player().pos.x > 1910.f);
  // Sala 5: caer al foso del piso sin invertir → muere, vuelve a ck_grav con gravedad normal.
  {
    const int d5 = sim.deaths();
    for (int i = 0; i < 40; ++i) sim.step({});
    auto& pp = sim.world().points()[sim.player_index()];
    pp.pos = pp.prev = {2000.f, 150.f};  // sobre el foso (más allá del orbe)
    for (int i = 0; i < 60 && sim.deaths() == d5; ++i) sim.step({});
    CHECK(sim.deaths() == d5 + 1 && !sim.gravity_flipped());
    CHECK(std::fabs(sim.player().pos.x - 1936.f) < 4.f);
    // Invertido, chocar contra los pinchos del hueco del techo también mata, y el checkpoint
    // (tomado con gravedad normal) reaparece normal.
    for (int i = 0; i < 40; ++i) sim.step({});
    sim.set_gravity_flipped(true);
    // Bajo el hueco, lejos de plat_inv (que se mueve entre x 2048 y 2144).
    const float hx = sim.mover(2).x < 2090.f ? 2134.f : 2056.f;
    pp.pos = pp.prev = {hx, 30.f};
    for (int i = 0; i < 60 && sim.deaths() == d5 + 1; ++i) sim.step({});
    CHECK(sim.deaths() == d5 + 2 && !sim.gravity_flipped());
    for (int i = 0; i < 40; ++i) sim.step({});
  }
  const int steps5 = solve_room5(sim);
  std::printf("meta: pasos=%d x=%.1f y=%.1f chunk=%d finished=%d cam_x=%.1f\n", steps5,
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), int(sim.finished()),
              scene.cam_x());
  CHECK(steps5 > 0);
  CHECK(scene.chunk_x() == 6);
  CHECK(sim.player().pos.x > 2168.f);
  CHECK(sim.finished());
  CHECK(!sim.gravity_flipped() && sim.flips() >= 2);
  CHECK(sim.deaths() == d_room4 + 3);
  for (int i = 0; i < 60; ++i) sim.step({});
  CHECK(scene.cam_x() >= 1900.f);
  CHECK(sim.finished());  // se mantiene
  CHECK(sim.finish_t() >= 0.999f);  // 60 pasos ≥ 0.75 s de fade
  CHECK(sim.collected() == 1);  // la repisa de la sala 3 pasa por el huevo (la meta no lo exige: sim2)

  // Congelado: el input de movimiento no saca al player de la meta.
  {
    const auto px = sim.player().pos.x, py = sim.player().pos.y;
    Input go; go.move_x = -1; go.jump = true;
    for (int i = 0; i < 10; ++i) sim.step(go);
    CHECK(sim.finished());
    CHECK(std::fabs(sim.player().pos.x - px) < 8.f);
    (void)py;
  }

  // R corto: vuelve al último checkpoint (ck_grav, puerta sigue abierta). R largo: spawn limpio.
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_grav");
  { Input rst; rst.restart = true; sim.step(rst); }
  CHECK(!sim.finished());
  CHECK(sim.finish_t() == 0.f);
  CHECK(!sim.on_rope());
  CHECK(std::fabs(sim.player().pos.x - 1936.f) < 4.f);
  CHECK(std::fabs(sim.player().pos.y - 152.f) < 10.f);
  CHECK(!sim.gravity_flipped());
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
