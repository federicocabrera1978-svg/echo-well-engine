// Test sin GL: boss (patrón determinista, pisotón × 3, contacto mata, puerta + meta al
// vencerlo, respawn/checkpoint lo curan, reset lo restablece) sobre assets/maps/src/boss.yaml.
#include "game/sim.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)
using namespace ew;
using game::Input;
using game::Sim;

static void place(Sim& s, float x, float y, float vy = 0.f) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = {x, y};
  p.prev = {x, y - vy};
  p.force = {};
  p.grounded = false;
}

// Deja caer al player sobre el centro del boss (desde 6 px arriba de su cabeza, bajando).
static void stomp(Sim& s) {
  const auto& b = s.boss(0);
  const float top = b.y - Sim::kBossH;
  place(s, b.x, top - Sim::kPlayerRadius + 1.5f, 2.f);
  s.step({});
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string err;
  game::Scene scene;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  // --- patrón determinista: dos sims sin input recorren exactamente lo mismo ---
  {
    game::Scene s2;
    if (!s2.load(argv[1], err)) return 1;
    Sim a(scene), b(s2);
    CHECK(a.boss_count() == 1 && !a.bosses_defeated());
    CHECK(a.boss(0).x == 176.f && a.boss(0).y == 160.f && a.boss(0).hits == 0);
    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f;
    bool saw_tell = false, saw_hop = false;
    for (int i = 0; i < 900; ++i) {
      a.step({});
      b.step({});
      CHECK(a.boss(0).x == b.boss(0).x && a.boss(0).y == b.boss(0).y && a.boss(0).phase == b.boss(0).phase);
      min_x = std::fmin(min_x, a.boss(0).x);
      max_x = std::fmax(max_x, a.boss(0).x);
      min_y = std::fmin(min_y, a.boss(0).y);
      saw_tell = saw_tell || a.boss(0).phase == 1;
      saw_hop = saw_hop || a.boss(0).phase == 2;
    }
    std::printf("patrón: x %.1f..%.1f  min_y=%.1f\n", min_x, max_x, min_y);
    CHECK(saw_tell && saw_hop);
    CHECK(min_x >= 176.f - Sim::kBossRange - 0.01f && max_x <= 176.f + Sim::kBossRange + 0.01f);
    CHECK(min_x < 120.f && max_x > 230.f);       // recorre toda la patrulla
    CHECK(min_y < 160.f - 25.f);                  // salta (≈ 33 px)
    CHECK(a.deaths() == 0);                       // el spawn está fuera de su alcance
    // La meta está cerrada con el boss vivo (aunque la toque).
    place(a, 300.f, 152.f);
    a.step({});
    CHECK(!a.finished());
  }

  // --- contacto de costado: mata y reaparece en el checkpoint ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    place(sim, 48.f, 152.f);
    sim.step({});
    CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_arena");
    for (int i = 0; i < 40; ++i) sim.step({});
    const auto& b = sim.boss(0);
    place(sim, b.x - Sim::kBossW * 0.5f - 2.f, b.y - 4.f);
    sim.step({});
    CHECK(sim.deaths() == 1);
    CHECK(sim.take_sfx() & 2u);
    CHECK(std::fabs(sim.player().pos.x - 48.f) < 2.f);
  }

  // --- tres pisotones: lo vencen, se abre la puerta y la meta cuenta ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    const auto* door = s.find_type("door");
    CHECK(door && !door->active && s.solid_px(284.f, 150.f));
    for (int i = 0; i < 40; ++i) sim.step({});
    sim.take_sfx();
    stomp(sim);
    CHECK(sim.boss(0).hits == 1 && sim.boss(0).hurt > 0.f && sim.deaths() == 0);
    CHECK(sim.take_sfx() & 16u);
    CHECK(sim.player().velocity().y < 0.f);  // rebota
    CHECK(sim.boss(0).speed() > Sim::kBossSpeed);  // más rápido con cada golpe
    // Herido: tocarlo no mata ni cuenta otro golpe.
    stomp(sim);
    CHECK(sim.boss(0).hits == 1 && sim.deaths() == 0);
    place(sim, sim.boss(0).x, sim.boss(0).y - 4.f);
    sim.step({});
    CHECK(sim.deaths() == 0);
    // Un respawn (R corto) lo cura: vuelve a su lugar con 0 golpes.
    { Input r; r.restart = true; sim.step(r); }
    CHECK(sim.boss(0).hits == 0 && sim.boss(0).x == 176.f && sim.boss(0).hurt == 0.f);
    for (int k = 0; k < Sim::kBossHits; ++k) {
      for (int i = 0; i < 70; ++i) sim.step({});  // pasa la invulnerabilidad del boss
      stomp(sim);
      CHECK(sim.boss(0).hits == k + 1);
    }
    CHECK(sim.boss(0).defeated && sim.bosses_defeated());
    CHECK(sim.take_sfx() & 4u);
    CHECK(s.find_type("boss")->active && door->active);
    for (int i = 0; i < 30; ++i) sim.step({});
    CHECK(!s.solid_px(284.f, 150.f));  // puerta abierta
    // Vencido: ya no mata ni se mueve.
    const float bx = sim.boss(0).x;
    place(sim, bx, sim.boss(0).y - 4.f);
    for (int i = 0; i < 30; ++i) sim.step({});
    CHECK(sim.deaths() == 0 && sim.boss(0).x == bx);
    // Morir/R corto con el boss vencido: sigue vencido y la puerta abierta.
    { Input r; r.restart = true; sim.step(r); }
    CHECK(sim.boss(0).defeated && door->active);
    // Caminar a la meta por la puerta.
    for (int i = 0; i < 600 && !sim.finished(); ++i) sim.step({1.f});
    CHECK(sim.finished());
    // R largo: todo de nuevo (boss vivo, puerta cerrada).
    { Input full; full.restart_full = true; sim.step(full); }
    CHECK(!sim.boss(0).defeated && sim.boss(0).hits == 0 && !door->active && !s.find_type("boss")->active);
  }

  // --- gravedad invertida: el pisotón es desde abajo (durante el salto del boss) ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    for (int i = 0; i < 600 && !(sim.boss(0).phase == 2 && sim.boss(0).vy > -60.f && sim.boss(0).vy < 0.f); ++i) sim.step({});
    CHECK(sim.boss(0).phase == 2);
    sim.set_gravity_flipped(true);
    const auto& b = sim.boss(0);
    place(sim, b.x, b.y + Sim::kPlayerRadius - 1.5f, -2.f);  // bajo los pies, subiendo
    sim.step({});
    CHECK(sim.boss(0).hits == 1 && sim.deaths() == 0);
    CHECK(sim.player().velocity().y > 0.f);  // rebote hacia su "arriba" (+Y)
  }

  // --- jugable: esperar al boss, saltar y caerle encima (input, sin teleport) ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    int steps = 0;
    for (; steps < 6000 && !sim.boss(0).defeated && sim.deaths() == 0; ++steps) {
      const auto& b = sim.boss(0);
      const float x = sim.player().pos.x;
      Input in;
      const bool air = !sim.player().grounded;
      if (!air) {
        // En el piso: quedarse a ~40 px a la izquierda del boss; saltar cuando camina hacia el player.
        const float want = b.x - 44.f;
        in.move_x = std::clamp((want - x) / 6.f, -1.f, 1.f);
        if (b.hurt <= 0.f && b.phase == 0 && b.dir < 0 && std::fabs(x - want) < 6.f) in.jump = true;
      } else {
        in.move_x = std::clamp((b.x - x) / 4.f, -1.f, 1.f);  // en el aire, apuntar a la cabeza
      }
      sim.step(in);
    }
    std::printf("pelea jugada: pasos=%d golpes=%d muertes=%d\n", steps, sim.boss(0).hits, sim.deaths());
    CHECK(sim.boss(0).defeated && sim.deaths() == 0);
  }

  std::puts("boss_test: OK");
  return 0;
}
