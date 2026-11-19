// Test sin GL: plataformas móviles (carry, colisión, determinismo, reset, aplastamiento).
#include "game/sim.hpp"
#include <cmath>
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)
using namespace ew;
using game::Input;

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  game::Scene scene;
  std::string err;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  game::Sim sim(scene);
  CHECK(sim.mover_count() == 2);
  CHECK(sim.ridden() < 0);
  const auto& m0 = sim.mover(0);
  CHECK(std::fabs(m0.x - 96.f) < 1e-4f && std::fabs(m0.y - 128.f) < 1e-4f);
  CHECK(std::fabs(m0.w - 24.f) < 1e-4f && std::fabs(m0.h - 8.f) < 1e-4f);
  CHECK(std::fabs(m0.speed - 80.f) < 1e-4f && std::fabs(m0.pause - 0.25f) < 1e-4f);
  CHECK(sim.rope_count() == 1 && sim.water().size() == 1);

  // --- carry: el player parado encima se mueve con la plataforma ---
  {
    auto& p = sim.world().points()[sim.player_index()];
    p.pos = p.prev = {96.f + 12.f, 128.f - game::Sim::kPlayerRadius};
    p.force = {};
    p.grounded = false;
    const float px0 = p.pos.x, mx0 = sim.mover(0).x;
    for (int i = 0; i < 60; ++i) sim.step({});
    const float dpx = sim.player().pos.x - px0, dmx = sim.mover(0).x - mx0;
    std::printf("carry: player dx=%.2f mover dx=%.2f grounded=%d\n", dpx, dmx, int(sim.player().grounded));
    CHECK(dmx > 70.f && dmx < 90.f);  // 80 px/s × 1 s
    CHECK(std::fabs(dpx - dmx) < 2.f);
    CHECK(sim.player().grounded);
    CHECK(sim.ridden() == 0);
    CHECK(std::fabs(sim.player().pos.y - (sim.mover(0).y - game::Sim::kPlayerRadius)) < 1.f);
  }

  // --- colisión desde abajo: no atraviesa, no queda "subido" ---
  {
    game::Scene s2;
    if (!s2.load(argv[1], err)) return 1;
    game::Sim sim2(s2);
    auto& p = sim2.world().points()[sim2.player_index()];
    // Centro 2 px por debajo del fondo (y=136); radio 4 → solapa 2 px.
    p.pos = p.prev = {96.f + 12.f, 136.f + 2.f};
    p.force = {};
    sim2.step({});
    CHECK(sim2.player().pos.y >= 136.f + game::Sim::kPlayerRadius - 0.6f);
    CHECK(sim2.player().pos.y < 136.f + game::Sim::kPlayerRadius + 4.f);
    CHECK(sim2.ridden() < 0);
    // no queda adentro del AABB
    const auto& m = sim2.mover(0);
    CHECK(sim2.player().pos.y + 0.01f >= m.y + m.h);
  }

  // --- colisión de costado ---
  {
    game::Scene s3;
    if (!s3.load(argv[1], err)) return 1;
    game::Sim sim3(s3);
    auto& p = sim3.world().points()[sim3.player_index()];
    // Izquierda del mover (x=96), solapa 2 px, a la altura del cuerpo.
    p.pos = p.prev = {96.f - 2.f, 128.f + 4.f};
    p.force = {};
    sim3.step({});
    const auto& m = sim3.mover(0);
    CHECK(sim3.player().pos.x <= m.x - game::Sim::kPlayerRadius + 0.6f);
    CHECK(sim3.player().pos.x > m.x - game::Sim::kPlayerRadius - 4.f);
    CHECK(sim3.player().pos.x + 0.01f <= m.x);
  }

  // --- determinismo: dos sims, mismos pasos, mismas posiciones ---
  {
    game::Scene a, b;
    if (!a.load(argv[1], err) || !b.load(argv[1], err)) return 1;
    game::Sim sa(a), sb(b);
    auto put = [](game::Sim& s) {
      auto& p = s.world().points()[s.player_index()];
      p.pos = p.prev = {108.f, 128.f - game::Sim::kPlayerRadius};
    };
    put(sa); put(sb);
    for (int i = 0; i < 180; ++i) {
      Input in;
      in.move_x = (i % 40 < 20) ? 1.f : 0.f;
      if (i == 30) in.jump = true;
      sa.step(in);
      sb.step(in);
    }
    CHECK(sa.player().pos.x == sb.player().pos.x);
    CHECK(sa.player().pos.y == sb.player().pos.y);
    CHECK(sa.mover(0).x == sb.mover(0).x);
    CHECK(sa.mover(0).y == sb.mover(0).y);
  }

  // --- reset() y respawn() devuelven la plataforma al origen ---
  {
    game::Scene s4;
    if (!s4.load(argv[1], err)) return 1;
    game::Sim sim4(s4);
    for (int i = 0; i < 90; ++i) sim4.step({});
    CHECK(sim4.mover(0).x > 96.f + 20.f);
    sim4.reset();
    CHECK(std::fabs(sim4.mover(0).x - 96.f) < 1e-4f);
    CHECK(std::fabs(sim4.mover(0).y - 128.f) < 1e-4f);
    CHECK(sim4.ridden() < 0);
    for (int i = 0; i < 90; ++i) sim4.step({});
    CHECK(sim4.mover(0).x > 96.f + 20.f);
    { Input r; r.restart = true; sim4.step(r); }
    CHECK(std::fabs(sim4.mover(0).x - 96.f) < 1e-4f);
    CHECK(std::fabs(sim4.player().pos.x - 32.f) < 2.f);
  }

  // --- aplastamiento: el player atrapado entre el mover y la columna muere ---
  {
    game::Scene s5;
    if (!s5.load(argv[1], err)) return 1;
    game::Sim sim5(s5);
    // El player viaja en el lift (mover 1) que sube contra el techo en y=72.
    CHECK(std::fabs(sim5.mover(1).x - 264.f) < 1e-4f);
    CHECK(std::fabs(sim5.mover(1).y - 136.f) < 1e-4f);
    auto& p = sim5.world().points()[sim5.player_index()];
    p.pos = p.prev = {264.f + 12.f, 136.f - game::Sim::kPlayerRadius};
    p.force = {};
    const int deaths0 = sim5.deaths();
    bool died = false;
    for (int i = 0; i < 90; ++i) {
      sim5.step({});
      if (sim5.deaths() > deaths0) { died = true; break; }
    }
    CHECK(died);
    // Tras morir, respawn en el spawn y las plataformas vuelven al inicio.
    CHECK(std::fabs(sim5.player().pos.x - 32.f) < 2.f);
    CHECK(std::fabs(sim5.mover(0).x - 96.f) < 1e-4f);
    CHECK(std::fabs(sim5.mover(1).y - 136.f) < 1e-4f);
  }

  // --- cuerda y agua siguen coherentes con la plataforma en movimiento ---
  {
    const auto& pts = sim.world().points();
    CHECK(pts[sim.rope_first()].pos.y == 24.f);
    for (const auto& s : sim.world().sticks()) {
      const float len = physics::length(pts[s.b].pos - pts[s.a].pos);
      CHECK(std::fabs(len - s.rest) < s.rest * 0.15f);
    }
    CHECK(sim.water()[0].x() == 16.f);
  }

  std::puts("mover_test: OK");
  return 0;
}
