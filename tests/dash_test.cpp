// Test sin GL: habilidad de Embestida (Dash), recarga, muros rompibles (break_wall),
// gravedad invertida, prioridad de interacción y guardado v3 sobre dash.yaml.
#include "audio/sfx.hpp"
#include "game/end_screen.hpp"
#include "game/gamepad.hpp"
#include "game/save.hpp"
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

static const game::Entity* find_name(const game::Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string err;
  game::Scene scene;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  // --- 1) Sin Dash: wall_gate bloquea el paso y apretar dash no hace nada ---
  {
    Sim sim(scene);
    CHECK(sim.abilities() == 0 && !sim.has_ability(game::kAbilityDash) && !sim.dash_ready());
    const auto* wg = find_name(scene, "wall_gate");
    CHECK(wg && !wg->active);
    CHECK(scene.door_blocks(20, 17) && scene.door_blocks(20, 19));
    place(sim, 140.f, 152.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    Input d_in;
    d_in.move_x = 1.f;
    d_in.dash = true;
    sim.step(d_in);
    CHECK(sim.dash_ticks() == 0);
    for (int i = 0; i < 60; ++i) sim.step({1.f});
    CHECK(sim.player().pos.x < 160.f && !wg->active);
  }

  // --- 2) Tomar rel_dash, embestir wall_gate, llegar a meta, respawn vs reset ---
  {
    game::Scene s;
    CHECK(s.load(argv[1], err));
    Sim sim(s);
    for (int i = 0; i < 15; ++i) sim.step({});
    (void)sim.take_sfx();
    for (int i = 0; i < 100 && !sim.has_ability(game::kAbilityDash); ++i) sim.step({1.f});
    CHECK(sim.has_ability(game::kAbilityDash) && sim.dash_ready());
    CHECK(sim.ability_flash() > 0);
    CHECK(sim.take_sfx() & audio::kSfxAbility);
    CHECK(find_name(s, "rel_dash")->active);

    // Esperar frente a wall_gate (x ≈ 148) y lanzar Dash hacia la derecha.
    for (int i = 0; i < 60 && sim.player().pos.x < 146.f; ++i) sim.step({1.f});
    CHECK(sim.player().pos.x < 160.f && !find_name(s, "wall_gate")->active);
    (void)sim.take_sfx();
    { Input d; d.move_x = 1.f; d.dash = true; sim.step(d); }
    CHECK(sim.dash_ticks() == Sim::kDashFrames);
    for (int i = 0; i < Sim::kDashFrames; ++i) sim.step({});
    std::printf("tras dash contra wall_gate: pos=(%.1f, %.1f) roto=%d\n",
                sim.player().pos.x, sim.player().pos.y, int(find_name(s, "wall_gate")->active));
    CHECK(find_name(s, "wall_gate")->active);
    CHECK(!s.door_blocks(20, 17) && !s.door_blocks(20, 19));
    CHECK(sim.player().pos.x > 164.f);
    CHECK(sim.wall_break_flash() > 0 || sim.player().pos.x > 168.f);

    // Prioridad de interacción: junto a sw_demo, (interact + dash) activa el switch y NO dispara Dash.
    place(sim, 208.f, 152.f);
    for (int i = 0; i < 30; ++i) sim.step({});
    CHECK(sim.dash_ready() && !find_name(s, "sw_demo")->active);
    {
      Input both;
      both.move_x = 1.f;
      both.interact = true;
      both.dash = true;
      sim.step(both);
    }
    CHECK(find_name(s, "sw_demo")->active);
    CHECK(sim.dash_ticks() == 0 && sim.dash_ready());

    // Avanzar al checkpoint y la meta.
    for (int i = 0; i < 120 && !sim.finished(); ++i) sim.step({1.f});
    CHECK(sim.finished() && sim.checkpoint() && sim.checkpoint()->name == "ck_beyond");

    // R corto conserva la habilidad y el muro roto; R largo limpia ambos.
    { Input r; r.restart = true; sim.step(r); }
    CHECK(sim.has_ability(game::kAbilityDash) && find_name(s, "wall_gate")->active);
    { Input r; r.restart_full = true; sim.step(r); }
    CHECK(!sim.has_ability(game::kAbilityDash) && !find_name(s, "wall_gate")->active);
    CHECK(s.door_blocks(20, 17));
  }

  // --- 3) Un solo Dash por vuelo + gravedad invertida horizontal sin caer + recarga al aterrizar ---
  {
    game::Scene s;
    CHECK(s.load(argv[1], err));
    Sim sim(s);
    sim.grant_ability(game::kAbilityDash);
    sim.set_gravity_flipped(true);
    place(sim, 40.f, 110.f);
    sim.step({});
    const float y0 = sim.player().pos.y;
    { Input d; d.move_x = 1.f; d.dash = true; sim.step(d); }
    CHECK(sim.dash_ticks() > 0 && !sim.dash_ready());
    for (int i = 0; i < Sim::kDashFrames; ++i) sim.step({});
    CHECK(sim.player().pos.x > 70.f);
    // Durante el dash horizontal, Y se mantiene casi plano (sin caer por gravedad).
    CHECK(std::fabs(sim.player().pos.y - y0) < 2.f);
    // Segundo dash en el aire antes de tocar superficie sólida no arranca.
    for (int i = 0; i < 20; ++i) sim.step({});
    { Input d; d.move_x = 1.f; d.dash = true; sim.step(d); }
    CHECK(sim.dash_ticks() == 0);
    // Al tocar el techo (gravedad invertida) se recarga el Dash.
    for (int i = 0; i < 60 && !sim.player().grounded; ++i) sim.step({});
    CHECK(sim.player().grounded && sim.dash_ready());
  }

  // --- 4) Save v3 + EndScreen + Gamepad reflejan kAbilityDash ---
  {
    game::Scene s;
    CHECK(s.load(argv[1], err));
    Sim sim(s);
    sim.grant_ability(game::kAbilityDoubleJump | game::kAbilityDash);
    s.set_active(2, true);  // wall_gate roto
    const auto bytes = game::encode_save(game::capture_save(sim));
    game::SaveData d;
    CHECK(game::decode_save(bytes.data(), bytes.size(), d) == game::SaveError::None);
    CHECK(d.abilities == (game::kAbilityDoubleJump | game::kAbilityDash));
    game::Scene s2;
    CHECK(s2.load(argv[1], err));
    Sim sim2(s2);
    CHECK(game::apply_save(sim2, d) == game::SaveError::None);
    CHECK(sim2.has_ability(game::kAbilityDash) && find_name(s2, "wall_gate")->active);
    CHECK(!s2.door_blocks(20, 17));

    game::EndScreen es;
    game::EndStats st;
    st.abilities = sim2.abilities();
    es.open(st);
    const auto txt = game::end_screen_text(es);
    CHECK(txt[4].find("DOBLE SALTO + DASH") != std::string::npos);

    game::PadRaw pr;
    game::PadEdges pe;
    Input pi;
    pr.x = true;
    pr.buttons = 1u << game::pad::kX;
    const auto def = game::Bindings::defaults();
    game::apply_pad(pr, pi, false, pe, 0.0, Sim::kRestartFullHold, &def);
    CHECK(pi.interact && pi.dash);
  }

  std::puts("dash_test OK");
  return 0;
}
