// Test sin GL: primera habilidad metroidvania (doble salto), recarga en suelo/cuerda,
// gravedad invertida, respawn/reset y guardado v3 sobre assets/maps/src/ability.yaml.
#include "audio/sfx.hpp"
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

  // --- 1) Sin habilidad: un salto normal no alcanza la repisa alta (y=96) ---
  {
    Sim sim(scene);
    CHECK(sim.abilities() == 0 && !sim.has_ability(game::kAbilityDoubleJump) && sim.air_jumps() == 0);
    for (int i = 0; i < 30; ++i) sim.step({});
    CHECK(sim.player().grounded);
    place(sim, 168.f, 156.f);
    for (int i = 0; i < 10; ++i) sim.step({});
    float min_y = 999.f;
    Input j; j.jump = true; sim.step(j);
    min_y = std::fmin(min_y, sim.player().pos.y);
    for (int i = 0; i < 45; ++i) {
      Input air;
      air.move_x = 1.f;
      air.jump = (i == 16);  // intenta saltar otra vez en el aire sin tener la habilidad
      sim.step(air);
      min_y = std::fmin(min_y, sim.player().pos.y);
    }
    std::printf("sin habilidad: min_y=%.2f (repisa en y=96)\n", min_y);
    CHECK(min_y > 105.f);
    CHECK(sim.double_jump_flash() == 0);
  }

  // --- 2, 3, 4 y 5) Agarrar reliquia, 1 doble salto por vuelo, subir repisa, respawn y reset ---
  {
    game::Scene s;
    CHECK(s.load(argv[1], err));
    Sim sim(s);
    for (int i = 0; i < 20; ++i) sim.step({});
    (void)sim.take_sfx();
    for (int i = 0; i < 100 && !sim.has_ability(game::kAbilityDoubleJump); ++i) sim.step({1.f});
    CHECK(sim.has_ability(game::kAbilityDoubleJump));
    CHECK(sim.air_jumps() == 1);
    CHECK(sim.ability_flash() > 0);
    CHECK(sim.take_sfx() & audio::kSfxAbility);
    const auto* rel = find_name(s, "rel_dj");
    CHECK(rel && rel->active);

    // Esperar a asentarse en el piso y probar límite de 1 doble salto por vuelo.
    place(sim, 150.f, 156.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    CHECK(sim.player().grounded && sim.air_jumps() == 1);
    { Input j; j.jump = true; sim.step(j); }
    CHECK(sim.air_jumps() == 1);
    for (int i = 0; i < 16; ++i) sim.step({});
    (void)sim.take_sfx();
    { Input j; j.jump = true; sim.step(j); }
    CHECK(sim.air_jumps() == 0);
    CHECK(sim.double_jump_flash() == Sim::kDoubleJumpFlashFrames);
    CHECK(sim.take_sfx() & audio::kSfxJump);
    // Un tercer salto en el aire no tiene efecto.
    const float vy_before = sim.player().velocity().y;
    for (int i = 0; i < 5; ++i) sim.step({});
    const int flash_before = sim.double_jump_flash();
    { Input j; j.jump = true; sim.step(j); }
    CHECK(sim.air_jumps() == 0);
    CHECK(sim.double_jump_flash() < flash_before);
    CHECK(sim.player().velocity().y > vy_before);  // sigue cayendo por gravedad
    // Al aterrizar se recarga.
    for (int i = 0; i < 60 && !sim.player().grounded; ++i) sim.step({});
    CHECK(sim.player().grounded && sim.air_jumps() == 1);

    // Subir a la repisa alta con Doble Salto, activar ck_top y llegar a la meta.
    place(sim, 176.f, 156.f);
    for (int i = 0; i < 10; ++i) sim.step({});
    { Input j; j.jump = true; sim.step(j); }
    for (int i = 0; i < 16; ++i) sim.step({});
    { Input j; j.jump = true; sim.step(j); }
    for (int i = 0; i < 10; ++i) sim.step({});
    for (int i = 0; i < 120 && !sim.finished(); ++i) sim.step({1.f});
    std::printf("con doble salto: pos=(%.1f, %.1f) finished=%d\n",
                sim.player().pos.x, sim.player().pos.y, int(sim.finished()));
    CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_top");
    CHECK(sim.finished());

    // R corto (respawn) conserva la habilidad; R largo (reset) la limpia y reactiva la reliquia.
    { Input r; r.restart = true; sim.step(r); }
    CHECK(sim.has_ability(game::kAbilityDoubleJump) && sim.air_jumps() == 1);
    CHECK(find_name(s, "rel_dj")->active);
    { Input r; r.restart_full = true; sim.step(r); }
    CHECK(sim.abilities() == 0 && sim.air_jumps() == 0);
    CHECK(!find_name(s, "rel_dj")->active);
  }

  // --- 6) Gravedad invertida + Doble Salto desde el techo ---
  {
    game::Scene s;
    CHECK(s.load(argv[1], err));
    Sim sim(s);
    sim.set_gravity_flipped(true);
    sim.grant_ability(game::kAbilityDoubleJump);
    place(sim, 120.f, 30.f);
    for (int i = 0; i < 30; ++i) sim.step({});
    CHECK(sim.player().grounded && std::fabs(sim.player().pos.y - 28.f) < 1.f);
    { Input j; j.jump = true; sim.step(j); }
    for (int i = 0; i < 16; ++i) sim.step({});
    { Input j; j.jump = true; sim.step(j); }
    float max_y = 0.f;
    for (int i = 0; i < 60; ++i) { sim.step({}); max_y = std::fmax(max_y, sim.player().pos.y); }
    std::printf("invertido + doble salto: max_y=%.2f final_y=%.2f\n", max_y, sim.player().pos.y);
    CHECK(max_y > 95.f);
    CHECK(sim.player().grounded && sim.air_jumps() == 1);
  }

  // --- 7) Cuerda + Doble Salto: saltar de la cuerda NO gasta el doble salto en el mismo frame ---
  {
    game::Scene s;
    CHECK(s.load(argv[1], err));
    Sim sim(s);
    sim.grant_ability(game::kAbilityDoubleJump);
    for (int i = 0; i < 60; ++i) sim.step({});
    const auto tip = sim.world().points()[sim.rope_first() + uint32_t(sim.rope_points() - 1)].pos;
    place(sim, tip.x, tip.y);
    { Input g; g.grab = true; sim.step(g); }
    CHECK(sim.on_rope() && sim.air_jumps() == 1);
    // Soltar con salto: debe soltar la cuerda y mantener air_jumps() == 1.
    { Input j; j.jump = true; sim.step(j); }
    CHECK(!sim.on_rope());
    CHECK(sim.air_jumps() == 1);
    CHECK(sim.double_jump_flash() == 0);
    // Un frame después en el aire, el doble salto sí funciona.
    { Input j; j.jump = true; sim.step(j); }
    CHECK(sim.air_jumps() == 0);
    CHECK(sim.double_jump_flash() == Sim::kDoubleJumpFlashFrames);
  }

  // --- 8) Guardado v3 sobre ability.ewmap: preserva habilidad y reliquia recogida ---
  {
    game::Scene s1, s2;
    CHECK(s1.load(argv[1], err) && s2.load(argv[1], err));
    Sim a(s1), b(s2);
    for (int i = 0; i < 100 && !a.has_ability(game::kAbilityDoubleJump); ++i) a.step({1.f});
    CHECK(a.has_ability(game::kAbilityDoubleJump));
    const auto* ck = find_name(s1, "ck_top");
    CHECK(ck);
    place(a, ck->x, ck->y);
    a.step({});
    CHECK(a.checkpoint() && a.checkpoint()->name == "ck_top");
    const auto bytes = game::encode_save(game::capture_save(a));
    game::SaveData d;
    CHECK(game::decode_save(bytes.data(), bytes.size(), d) == game::SaveError::None);
    CHECK(d.abilities == game::kAbilityDoubleJump);
    CHECK(game::apply_save(b, d) == game::SaveError::None);
    CHECK(b.has_ability(game::kAbilityDoubleJump) && b.air_jumps() == 1);
    CHECK(find_name(s2, "rel_dj")->active);
    CHECK(b.checkpoint() && b.checkpoint()->name == "ck_top");
  }

  std::puts("ability_test: OK");
  return 0;
}
