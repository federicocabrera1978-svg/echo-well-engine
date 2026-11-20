// Test sin GL: gravedad invertida (orbes gravity_flip, salto, carry de movers, aplastamiento,
// cuerda, agua, checkpoint, respawn y reset) sobre assets/maps/src/gravity.yaml.
#include "game/sim.hpp"
#include <cmath>
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)
using namespace ew;
using game::Input;
using game::Sim;

static void place(Sim& s, float x, float y) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = p.prev = {x, y};
  p.force = {};
  p.grounded = false;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string err;
  game::Scene scene;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  // --- orbe en el piso: caminar hasta él invierte la gravedad; el player cae al techo ---
  {
    Sim sim(scene);
    CHECK(!sim.gravity_flipped() && sim.down() == 1.f && sim.flips() == 0);
    CHECK(sim.world().points()[sim.player_index()].gscale == 1.f);
    for (int i = 0; i < 30; ++i) sim.step({});  // asentarse en el piso
    CHECK(sim.player().grounded && std::fabs(sim.player().pos.y - 156.f) < 1.f);
    for (int i = 0; i < 120 && !sim.gravity_flipped(); ++i) sim.step({1.f});
    CHECK(sim.gravity_flipped() && sim.down() == -1.f && sim.flips() == 1);
    CHECK(sim.take_sfx() & 16u);
    CHECK(scene.find_type("gravity_flip")->active);  // el orbe muestra "invertida"
    CHECK(sim.world().points()[sim.player_index()].gscale == -1.f);
    for (int i = 0; i < 90; ++i) sim.step({});
    std::printf("techo: y=%.2f grounded=%d\n", sim.player().pos.y, int(sim.player().grounded));
    CHECK(std::fabs(sim.player().pos.y - 28.f) < 1.f);  // techo en y=24, radio 4
    CHECK(sim.player().grounded);                         // "apoyado" en el techo
    CHECK(sim.flips() == 1);  // pasar lejos del orbe no lo vuelve a disparar

    // Salto invertido: se aleja del techo (y crece) y vuelve.
    { Input j; j.jump = true; sim.step(j); }
    float max_y = 0;
    for (int i = 0; i < 60; ++i) { sim.step({}); max_y = std::fmax(max_y, sim.player().pos.y); }
    std::printf("salto invertido: max_y=%.2f final=%.2f\n", max_y, sim.player().pos.y);
    CHECK(max_y > 60.f && max_y < 80.f);  // ≈ 44 px de altura, como el salto normal
    CHECK(std::fabs(sim.player().pos.y - 28.f) < 1.f && sim.player().grounded);

    // Caminar por el techo hasta el orbe del techo: vuelve la gravedad normal.
    for (int i = 0; i < 120 && sim.gravity_flipped(); ++i) sim.step({1.f});
    CHECK(!sim.gravity_flipped() && sim.flips() == 2);
    CHECK(!scene.find_type("gravity_flip")->active);
    for (int i = 0; i < 90; ++i) sim.step({});
    CHECK(std::fabs(sim.player().pos.y - 156.f) < 1.f && sim.player().grounded);

    // Rearme: quedarse dentro del alcance no repite; alejarse > kFlipRearm y volver, sí.
    place(sim, 64.f, 152.f);
    sim.step({});
    CHECK(sim.gravity_flipped() && sim.flips() == 3);
    place(sim, 66.f, 150.f);
    sim.step({});
    CHECK(sim.gravity_flipped() && sim.flips() == 3);
    place(sim, 64.f + Sim::kFlipRearm + 4.f, 150.f);
    sim.step({});
    place(sim, 64.f, 152.f);
    sim.step({});
    CHECK(!sim.gravity_flipped() && sim.flips() == 4);

    // R largo: gravedad normal, orbes en estado inicial.
    place(sim, 64.f, 152.f);
    sim.step({});  // desarmado: no dispara (todavía en alcance)
    sim.set_gravity_flipped(true);
    { Input full; full.restart_full = true; sim.step(full); }
    CHECK(!sim.gravity_flipped() && sim.flips() == 0);
    CHECK(sim.world().points()[sim.player_index()].gscale == 1.f);
    CHECK(!scene.find_type("gravity_flip")->active);
  }

  // --- checkpoint tomado con gravedad invertida: morir / R corto reaparece invertido ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    sim.set_gravity_flipped(true);
    place(sim, 180.f, 30.f);
    sim.step({});
    CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_ceil");
    sim.set_gravity_flipped(false);  // p. ej. tocó otro orbe después
    { Input r; r.restart = true; sim.step(r); }
    CHECK(sim.gravity_flipped());
    CHECK(std::fabs(sim.player().pos.x - 180.f) < 1.f);
    for (int i = 0; i < 40; ++i) sim.step({});
    CHECK(std::fabs(sim.player().pos.y - 28.f) < 1.f);  // se queda en el techo
    // Sin checkpoint, el respawn vuelve al spawn con gravedad normal.
    { Input full; full.restart_full = true; sim.step(full); }
    sim.set_gravity_flipped(true);
    { Input r; r.restart = true; sim.step(r); }
    CHECK(!sim.gravity_flipped());
  }

  // --- mover: invertido, el player "parado" en la cara de abajo viaja con la plataforma ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    CHECK(sim.mover_count() == 2);
    sim.set_gravity_flipped(true);
    const auto& m = sim.mover(0);  // under: y 96..104, x 200→240
    place(sim, m.x + 12.f, m.y + m.h + Sim::kPlayerRadius);
    const float px0 = sim.player().pos.x, mx0 = m.x;
    for (int i = 0; i < 45; ++i) sim.step({});
    const float dpx = sim.player().pos.x - px0, dmx = m.x - mx0;
    std::printf("carry invertido: player dx=%.2f mover dx=%.2f ridden=%d y=%.2f\n", dpx, dmx,
                sim.ridden(), sim.player().pos.y);
    CHECK(dmx > 25.f);
    CHECK(std::fabs(dpx - dmx) < 2.f);
    CHECK(sim.ridden() == 0 && sim.player().grounded);
    CHECK(std::fabs(sim.player().pos.y - (m.y + m.h + Sim::kPlayerRadius)) < 1.f);
    // Con gravedad normal, en la misma posición, se cae (no viaja).
    game::Scene s2;
    if (!s2.load(argv[1], err)) return 1;
    Sim sim2(s2);
    place(sim2, sim2.mover(0).x + 12.f, sim2.mover(0).y + sim2.mover(0).h + Sim::kPlayerRadius);
    for (int i = 0; i < 30; ++i) sim2.step({});
    CHECK(sim2.ridden() < 0 && sim2.player().pos.y > 130.f);
  }

  // --- aplastamiento invertido: colgado de la cara de abajo del pisón, que baja hasta 4 px del piso ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    sim.set_gravity_flipped(true);
    const auto& m = sim.mover(1);  // press: x 280..296, y 40 → 148
    place(sim, m.x + 8.f, m.y + m.h + Sim::kPlayerRadius);
    for (int i = 0; i < 40; ++i) sim.step({});  // invulnerable al arrancar no aplica (no hubo respawn)
    CHECK(sim.ridden() == 1);
    int steps = 0;
    for (; steps < 300 && sim.deaths() == 0; ++steps) sim.step({});
    std::printf("aplastado invertido: pasos=%d muertes=%d press_y=%.1f\n", steps, sim.deaths(), m.y);
    CHECK(sim.deaths() == 1);
    CHECK(sim.take_sfx() & 2u);
    CHECK(!sim.gravity_flipped());  // sin checkpoint → spawn con gravedad normal
  }

  // --- cuerda: invertido se agarra igual; al soltar cae hacia el techo; el salto empuja hacia abajo ---
  {
    game::Scene s;
    if (!s.load(argv[1], err)) return 1;
    Sim sim(s);
    CHECK(sim.rope_count() == 1);
    for (int i = 0; i < 120; ++i) sim.step({});  // la cuerda se asienta
    const auto& pts = sim.world().points();
    const auto tip = pts[sim.rope_first() + uint32_t(sim.rope_points() - 1)].pos;
    CHECK(tip.y > 60.f);  // cuelga hacia abajo (la cuerda no se invierte)
    sim.set_gravity_flipped(true);
    place(sim, tip.x, tip.y);
    { Input g; g.grab = true; sim.step(g); }
    CHECK(sim.on_rope());
    for (int i = 0; i < 30; ++i) sim.step({});
    CHECK(sim.on_rope() && sim.player().pos.y > 55.f);  // sigue colgado del nodo
    { Input j; j.jump = true; sim.step(j); }
    CHECK(!sim.on_rope());
    CHECK(sim.player().velocity().y > 0.f);  // empuje "hacia arriba" del player = hacia +Y
    float min_y = 999.f;
    // Apartarse de la cuerda (si no, queda apoyado "debajo" de la punta, que lo frena).
    for (int i = 0; i < 120; ++i) {
      sim.step({i < 20 ? -1.f : 0.f});
      min_y = std::fmin(min_y, sim.player().pos.y);
    }
    std::printf("cuerda invertida: x=%.1f y=%.1f min_y=%.1f\n", sim.player().pos.x, sim.player().pos.y, min_y);
    CHECK(std::fabs(sim.player().pos.y - 28.f) < 1.f);  // terminó en el techo
  }

  // --- agua: el empuje va contra la gravedad del player; el nado empuja contra su gravedad ---
  {
    game::Scene sa, sb;
    if (!sa.load(argv[1], err) || !sb.load(argv[1], err)) return 1;
    Sim a(sa), b(sb);
    b.set_gravity_flipped(true);
    place(a, 144.f, 150.f);
    place(b, 144.f, 150.f);
    a.step({});
    b.step({});
    CHECK(a.player_in_water() && b.player_in_water());
    // Normal: gravedad 600 abajo, empuje 1400 arriba → sube. Invertida: todo al revés → baja.
    CHECK(a.player().velocity().y < 0.f);
    CHECK(b.player().velocity().y > 0.f);
    { Input j; j.jump = true; b.step(j); }
    CHECK(b.player().velocity().y > 0.f);
  }

  std::puts("gravity_test: OK");
  return 0;
}
