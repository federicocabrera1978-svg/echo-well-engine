// Test sin GL (Etapa 9): mini-mapa exploratorio (Scene::visited_chunks), enemigo menor (imp),
// cronómetro determinista (Sim::elapsed_sec) y victoria final / rango S-A-B en EndScreen.
#include "game/campaign.hpp"
#include "game/end_screen.hpp"
#include "game/sim.hpp"

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
  s.scene().follow(x, y);
}

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  std::string err;

  // --- 1) Mini-mapa exploratorio: chunk inicial marcado, nuevos chunks al explorar, R largo reinicia ---
  {
    game::Scene slice_sc;
    CHECK(slice_sc.load(argv[1], err));
    CHECK(slice_sc.chunk_visited(0, 0));
    CHECK(!slice_sc.chunk_visited(1, 0) && !slice_sc.chunk_visited(7, 0));
    slice_sc.follow(336.f, 152.f);
    CHECK(slice_sc.chunk_visited(0, 0) && slice_sc.chunk_visited(1, 0) && !slice_sc.chunk_visited(2, 0));
    slice_sc.follow(2264.f, 152.f);
    CHECK(slice_sc.chunk_visited(7, 0));
    CHECK(slice_sc.visited_chunks() == ((uint64_t(1) << 0) | (uint64_t(1) << 1) | (uint64_t(1) << 7)));
    CHECK(!slice_sc.chunk_visited(-1, 0) && !slice_sc.chunk_visited(8, 0));
    slice_sc.reset_state();
    CHECK(slice_sc.visited_chunks() == uint64_t(1));
  }

  // --- 2) Enemigo menor `imp` sobre level3.ewmap: patrulla, derrota con Dash o pisotón, respawn ---
  {
    game::Scene s3;
    CHECK(s3.load(argv[2], err));
    Sim sim(s3);
    CHECK(sim.imp_count() == 1 && !sim.imp(0).defeated);
    const float x0 = sim.imp(0).x;
    for (int i = 0; i < 40; ++i) sim.step({});
    CHECK(sim.imp(0).x != x0);
    CHECK(sim.elapsed_sec() > 0.6f);

    // Contacto lateral sin pisotón ni Dash mata y resetea la posición del imp.
    place(sim, sim.imp(0).x, sim.imp(0).y - 4.f);
    sim.step({});
    CHECK(sim.deaths() == 1);

    // Pisotón desde arriba elimina al imp y hace rebotar al player.
    for (int i = 0; i < 35; ++i) sim.step({});
    CHECK(sim.invuln() == 0);
    const auto& im = sim.imp(0);
    place(sim, im.x, im.y - Sim::kImpH - Sim::kPlayerRadius + 1.5f, 2.f);
    sim.step({});
    CHECK(sim.imp(0).defeated && sim.deaths() == 1);
    CHECK(sim.player().velocity().y < 0.f);

    // R corto (respawn) devuelve el imp a patrullar.
    { Input r; r.restart = true; sim.step(r); }
    CHECK(!sim.imp(0).defeated);

    // Con Dash: embestir al imp lo destruye al paso y recarga salto aéreo / dash.
    sim.grant_ability(game::kAbilityDoubleJump | game::kAbilityDash);
    for (int i = 0; i < 35; ++i) sim.step({});
    place(sim, sim.imp(0).x - 10.f, sim.imp(0).y - 4.f);
    { Input d; d.move_x = 1.f; d.dash = true; sim.step(d); }
    for (int i = 0; i < 6 && !sim.imp(0).defeated; ++i) sim.step({});
    CHECK(sim.imp(0).defeated && sim.deaths() == 1);

    // R largo reinicia cronómetro y mini-mapa.
    CHECK(s3.chunk_visited(0, 1));
    { Input full; full.restart_full = true; sim.step(full); }
    CHECK(sim.elapsed_sec() == 0.f && !sim.imp(0).defeated);
    CHECK(s3.chunk_visited(0, 0) && !s3.chunk_visited(0, 1));
  }

  // --- 3) EndScreen final de campaña: título VICTORIA, tiempo y rangos S / A / B ---
  {
    game::EndScreen es;
    game::EndStats s_rank{game::level_title("level3"), 2, 2, 0,
                          game::kAbilityDoubleJump | game::kAbilityDash, false, "", 125.f, true};
    es.open(s_rank);
    const auto t_s = game::end_screen_text(es);
    CHECK(t_s[0] == "VICTORIA: REINO DESPIERTO");
    CHECK(t_s[1] == "NIVEL 3: LA GUARIDA DEL WYRM");
    CHECK(t_s[5] == "TIEMPO:  2:05   RANGO: S");

    game::EndStats a_rank{"NIVEL 3: LA GUARIDA DEL WYRM", 2, 2, 2,
                          game::kAbilityDoubleJump | game::kAbilityDash, false, "", 62.f, true};
    CHECK(game::end_rank(a_rank) == 'A');

    game::EndStats b_rank{"NIVEL 3: LA GUARIDA DEL WYRM", 1, 2, 4,
                          game::kAbilityDoubleJump | game::kAbilityDash, false, "", 10.f, true};
    CHECK(game::end_rank(b_rank) == 'B');
  }

  std::puts("stage9_test OK");
  return 0;
}
