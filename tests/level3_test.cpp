// Test sin GL: Nivel 3 "La Guarida del Wyrm" (level3.ewmap, 3×2 chunks = 960×368 px) +
// jefe dragón multipatrón (boss_dragon: patrulla, vuelo con bolas de fuego, embestida rasa,
// 4 pisotones, i-frames del Dash contra fuego) y campaña slice -> level2 -> level3.
#include "game/campaign.hpp"
#include "game/end_screen.hpp"
#include "game/save.hpp"
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
  s.scene().follow(x, y);
}

static const game::Entity* find_name(const game::Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

static void perform_double_jump_to_ledge(Sim& sim, float dir, int first_frames = 16,
                                         int pre_steer_frames = 10, int steer_frames = 35) {
  { Input j; j.jump = true; sim.step(j); }
  for (int i = 0; i < first_frames; ++i) sim.step({});
  { Input j; j.jump = true; sim.step(j); }
  for (int i = 0; i < pre_steer_frames; ++i) sim.step({});
  for (int i = 0; i < steer_frames; ++i) {
    Input in;
    in.move_x = dir;
    sim.step(in);
  }
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string err;
  game::Scene scene;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  // --- 1) Geometría 3×2, entidades y jefe boss_dragon ---
  CHECK(scene.width_px() == 960 && scene.height_px() == 368);
  CHECK(scene.map().header().map_w_chunks == 3 && scene.map().header().map_h_chunks == 2);
  CHECK(scene.map().backdrops().size() == 6);
  CHECK(scene.dialogs().size() == 4);

  Sim sim(scene);
  CHECK(sim.collectible_total() == 2 && sim.boss_count() == 1);
  CHECK(sim.boss(0).dragon && sim.boss(0).max_hits() == Sim::kDragonHits);
  CHECK(!sim.bosses_defeated());

  const auto* wall_entry = find_name(scene, "wall_entry");
  const auto* wall_sec = find_name(scene, "wall_secret3");
  const auto* egg1 = find_name(scene, "egg_wyrm_1");
  const auto* egg2 = find_name(scene, "egg_wyrm_2");
  const auto* sw_seal = find_name(scene, "sw_seal3");
  const auto* door_seal = find_name(scene, "door_wyrm_seal");
  const auto* door_exit = find_name(scene, "door_wyrm_exit");
  CHECK(wall_entry && wall_sec && egg1 && egg2 && sw_seal && door_seal && door_exit);

  // --- 2) Patrón multipatrón del Wyrm (vuelo + bolas de fuego + embestida + i-frames de Dash) ---
  {
    game::Scene sb;
    CHECK(sb.load(argv[1], err));
    Sim s(sb);
    s.grant_ability(game::kAbilityDoubleJump | game::kAbilityDash);
    place(s, 664.f, 152.f);
    s.step({});
    CHECK(s.checkpoint() && s.checkpoint()->name == "ck_wyrm_arena");

    // Observar desde la repisa de entrada un ciclo completo de salto + fuego + embestida.
    place(s, 648.f, 88.f);
    bool saw_jump = false, saw_fireball = false, saw_charge = false;
    for (int i = 0; i < 420; ++i) {
      s.step({});
      const auto& b = s.boss(0);
      saw_jump = saw_jump || (b.phase == 2);
      saw_charge = saw_charge || (b.phase == 3);
      saw_fireball = saw_fireball || !s.fireballs().empty();
      if (saw_jump && saw_fireball && saw_charge) break;
    }
    std::printf("wyrm patrón: jump=%d fireball=%d charge=%d\n", int(saw_jump), int(saw_fireball), int(saw_charge));
    CHECK(saw_jump && saw_fireball && saw_charge);

    // Esperar a que una bola de fuego se aleje del cuerpo del Wyrm y probar i-frames de Dash vs daño sin Dash.
    auto find_clear_fb = [&](game::Sim::Fireball& out) {
      for (int i = 0; i < 400; ++i) {
        s.step({});
        const auto& b = s.boss(0);
        for (const auto& f : s.fireballs()) {
          if (f.life > 0.25f && f.y < 155.f && std::fabs(f.x - b.x) > Sim::kDragonW * 0.5f + 12.f) {
            out = f;
            return true;
          }
        }
      }
      return false;
    };
    game::Sim::Fireball fb{};
    CHECK(find_clear_fb(fb));
    // Con Dash activo: atraviesa la brasa sin morir.
    place(s, fb.x, fb.y);
    { Input d; d.move_x = 1.f; d.dash = true; s.step(d); }
    CHECK(s.dash_ticks() > 0 && s.deaths() == 0);
    // Terminar el dash en curso antes de estacionar al player arriba de la repisa de entrada.
    for (int i = 0; i < Sim::kDashFrames + 2; ++i) {
      place(s, 648.f, 88.f);
      s.step({});
    }
    for (int i = 0; i < 40; ++i) s.step({});
    CHECK(s.deaths() == 0 && s.invuln() == 0);
    game::Sim::Fireball fb2{};
    CHECK(find_clear_fb(fb2));
    place(s, fb2.x, fb2.y);
    s.step({});
    CHECK(s.deaths() == 1);
  }

  // --- 3) Camino jugable completo de level3: (0,0) -> (1,0) -> (1,1) -> (0,1) -> (1,0) -> (2,0) ---
  for (int i = 0; i < 20; ++i) sim.step({});
  for (int i = 0; i < 120 && !sim.has_ability(game::kAbilityDash); ++i) sim.step({1.f});
  CHECK(sim.has_ability(game::kAbilityDoubleJump | game::kAbilityDash));

  // Romper wall_entry (x=200) embistiendo desde x≈178, y luego cruzar (tx=28..33) con Salto + Dash aéreo hacia ck_abismo3.
  for (int i = 0; i < 80 && sim.player().pos.x < 178.f; ++i) sim.step({1.f});
  CHECK(!wall_entry->active && scene.door_blocks(25, 17));
  { Input d; d.move_x = 1.f; d.dash = true; sim.step(d); }
  for (int i = 0; i < Sim::kDashFrames + 2; ++i) sim.step({});
  CHECK(wall_entry->active && !scene.door_blocks(25, 17) && sim.deaths() == 0);
  for (int i = 0; i < 30 && (!sim.player().grounded || !sim.dash_ready()); ++i) sim.step({});
  for (int i = 0; i < 40 && sim.player().pos.x < 216.f; ++i) sim.step({1.f});
  { Input j; j.move_x = 1.f; j.jump = true; sim.step(j); }
  for (int i = 0; i < 12; ++i) sim.step({1.f});
  { Input d; d.move_x = 1.f; d.dash = true; sim.step(d); }
  for (int i = 0; i < 160 && !(sim.checkpoint() && sim.checkpoint()->name == "ck_abismo3"); ++i) sim.step({1.f});
  CHECK(scene.chunk_x() == 1 && scene.chunk_y() == 0);
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_abismo3");

  // Tomar egg_wyrm_1 en la repisa de (1,0) saltando desde la izquierda (x=316, fuera de x=328..384).
  place(sim, 316.f, 152.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  perform_double_jump_to_ledge(sim, 1.f);
  for (int i = 0; i < 80 && !egg1->active; ++i) sim.step({1.f});
  CHECK(egg1->active && sim.collected() == 1);

  // Descender por la boca libre del pozo vertical (tx=70..71 -> x=568) hasta el agua de (1,1) y tomar ck_invertido3.
  place(sim, 568.f, 152.f);
  for (int i = 0; i < 150 && !sim.player_in_water(); ++i) sim.step({});
  CHECK(scene.chunk_x() == 1 && scene.chunk_y() == 1 && sim.player_in_water());
  for (int i = 0; i < 140 && !(sim.checkpoint() && sim.checkpoint()->name == "ck_invertido3"); ++i) sim.step({-1.f});
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_invertido3");

  // Corredor de gravedad invertida (flip_up3 -> techo -> flip_down3) y romper wall_secret3 hacia (0,1).
  for (int i = 0; i < 80 && !sim.gravity_flipped(); ++i) sim.step({-1.f});
  CHECK(sim.gravity_flipped());
  for (int i = 0; i < 40 && !sim.player().grounded; ++i) sim.step({});
  for (int i = 0; i < 350 && sim.gravity_flipped(); ++i) sim.step({-1.f});
  CHECK(!sim.gravity_flipped());
  sim.step({});  // despegar del techo tras des-invertir la gravedad
  for (int i = 0; i < 80 && !sim.player().grounded; ++i) sim.step({});
  CHECK(sim.player().grounded && !wall_sec->active && scene.door_blocks(39, 40));
  for (int i = 0; i < 40 && sim.player().pos.x > 332.f; ++i) sim.step({-1.f});
  { Input d; d.move_x = -1.f; d.dash = true; sim.step(d); }
  for (int i = 0; i < Sim::kDashFrames + 5; ++i) sim.step({-1.f});
  CHECK(wall_sec->active && !scene.door_blocks(39, 40));
  CHECK(scene.chunk_x() == 0 && scene.chunk_y() == 1);

  // Activar sw_seal3 (abre door_wyrm_seal) y subir al altar de egg_wyrm_2 (2/2 huevos).
  for (int i = 0; i < 120 && sim.player().pos.x > sw_seal->x + 2.f; ++i) sim.step({-1.f});
  { Input act; act.interact = true; sim.step(act); }
  CHECK(sw_seal->active && door_seal->active);
  place(sim, 168.f, 340.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  perform_double_jump_to_ledge(sim, -1.f);
  for (int i = 0; i < 80 && !egg2->active; ++i) sim.step({-1.f});
  CHECK(egg2->active && sim.collected() == 2);

  // Entrar a (2,0) por door_wyrm_seal abierta, bajar a ck_wyrm_arena, vencer al Wyrm y tocar goal.
  place(sim, 616.f, 88.f);
  for (int i = 0; i < 60 && sim.player().pos.x < 658.f; ++i) sim.step({1.f});
  for (int i = 0; i < 90 && !(sim.checkpoint() && sim.checkpoint()->name == "ck_wyrm_arena"); ++i) sim.step({});
  CHECK(scene.chunk_x() == 2 && scene.chunk_y() == 0);
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_wyrm_arena");
  CHECK(!door_exit->active);

  for (int k = 0; k < Sim::kDragonHits && !sim.bosses_defeated(); ++k) {
    for (int i = 0; i < 120 && (sim.boss(0).hurt > 0.f || sim.boss(0).phase == 2); ++i) {
      place(sim, 648.f, 88.f);
      sim.step({});
    }
    const auto& b = sim.boss(0);
    place(sim, b.x, b.y - Sim::kDragonH - Sim::kPlayerRadius + 1.5f, 2.f);
    sim.step({});
    CHECK(sim.boss(0).hits == k + 1);
  }
  place(sim, 880.f, 152.f);
  CHECK(sim.boss(0).defeated && sim.bosses_defeated() && door_exit->active && sim.fireballs().empty());

  for (int i = 0; i < 240 && !sim.finished(); ++i) sim.step({1.f});
  std::printf("level3 meta: pos=(%.1f, %.1f) finished=%d huevos=%d/%d muertes=%d\n",
              sim.player().pos.x, sim.player().pos.y, int(sim.finished()),
              sim.collected(), sim.collectible_total(), sim.deaths());
  CHECK(sim.finished() && sim.collected() == 2 && sim.deaths() == 0);

  // --- 4) Progresión de campaña slice -> level2 -> level3 y guardado v3 ---
  CHECK(game::next_campaign_map("slice") == "level2");
  CHECK(game::next_campaign_map("level2") == "level3");
  CHECK(game::next_campaign_map("level3").empty());
  CHECK(game::level_title("level3") == "NIVEL 3: LA GUARIDA DEL WYRM");
  if (argc >= 3) {
    CHECK(!game::resolve_map_file("level3", argv[2], "").empty());
  }
  {
    const auto bytes = game::encode_save(game::capture_save(sim));
    game::SaveData d;
    CHECK(game::decode_save(bytes.data(), bytes.size(), d) == game::SaveError::None);
    CHECK(d.map == "level3" && d.abilities == (game::kAbilityDoubleJump | game::kAbilityDash));
    game::Scene s2;
    CHECK(s2.load(argv[1], err));
    Sim sim2(s2);
    CHECK(game::apply_save(sim2, d) == game::SaveError::None);
    CHECK(sim2.collected() == 2 && sim2.bosses_defeated());
    CHECK(find_name(s2, "wall_entry")->active && find_name(s2, "wall_secret3")->active);
  }

  std::puts("level3_test OK");
  return 0;
}
