// Test sin GL: habilidad kAbilityWallClimb (deslizamiento y salto en pared),
// chimenea de 136 px inalcanzable con Doble Salto, preservación de air_jumps_,
// FrameData/HUD/EndScreen y persistencia save.ewsv sobre wallclimb.yaml.
#include "audio/sfx.hpp"
#include "game/end_screen.hpp"
#include "game/frame_view.hpp"
#include "game/hud.hpp"
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

  // --- 1) Con Doble Salto (sin WallClimb) NO se puede subir la chimenea de 136 px ---
  {
    Sim sim(scene);
    CHECK(sim.abilities() == 0 && !sim.has_ability(game::kAbilityWallClimb));
    sim.grant_ability(game::kAbilityDoubleJump);
    place(sim, 172.f, 160.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    CHECK(sim.player().grounded);
    float min_y = 999.f;
    { Input j; j.jump = true; j.move_x = 1.f; sim.step(j); }
    for (int i = 0; i < 60; ++i) {
      Input in;
      in.move_x = (i % 20 < 10) ? 1.f : -1.f;
      in.jump = (i == 18);
      sim.step(in);
      min_y = std::fmin(min_y, sim.player().pos.y);
    }
    std::printf("solo doble salto en chimenea 136px: min_y=%.2f (meta en y=32)\n", min_y);
    CHECK(min_y > 48.f);
    CHECK(!sim.finished());
  }

  // --- 2) Recoger rel_wall_climb, deslizar con límite de velocidad, trepar chimenea -> goal ---
  {
    game::Scene s;
    CHECK(s.load(argv[1], err));
    Sim sim(s);
    game::HudTracker hud;
    game::PauseMenu menu;
    game::EndScreen end_screen;
    game::DialogueBox dlg;
    game::FrameData fv;

    for (int i = 0; i < 15; ++i) sim.step({});
    (void)sim.take_sfx();
    for (int i = 0; i < 120 && !sim.has_ability(game::kAbilityWallClimb); ++i) sim.step({1.f});
    CHECK(sim.has_ability(game::kAbilityWallClimb));
    CHECK(sim.ability_flash() > 0);
    CHECK((sim.take_sfx() & audio::kSfxAbility) != 0u);
    CHECK(find_name(s, "rel_wall_climb")->active);

    const auto hev = game::populate_hud_and_overlays(fv, s, sim, menu, end_screen, dlg, hud, "0.1.0");
    CHECK(hev.new_wall_climb);
    CHECK(fv.abilities == int(game::kAbilityWallClimb));

    // EndScreen refleja ESCALADA cuando tiene kAbilityWallClimb.
    {
      game::EndScreen es;
      game::EndStats st;
      st.abilities = sim.abilities();
      es.open(st);
      const auto txt = game::end_screen_text(es);
      CHECK(txt[4].find("ESCALADA") != std::string::npos);
    }

    // Probar deslizamiento controlado en pared derecha de la chimenea (x=180, cayendo).
    place(sim, 180.f, 70.f);
    bool saw_slide = false;
    float max_slide_vy = 0.f;
    for (int i = 0; i < 30; ++i) {
      sim.step({1.f});
      if (sim.wall_sliding()) {
        saw_slide = true;
        CHECK(sim.wall_dir() == 1);
        const float vy_px_s = sim.player().velocity().y / Sim::kDt;
        max_slide_vy = std::fmax(max_slide_vy, vy_px_s);
        CHECK(vy_px_s <= Sim::kWallSlideSpeed + 1.f);
        game::build_world_frame(fv, s, sim, float(i) * Sim::kDt, false, false);
        CHECK(fv.marker_count > 0);
      }
    }
    std::printf("wall-slide: saw=%d max_vy=%.2f px/s (limite %.1f)\n",
                int(saw_slide), max_slide_vy, Sim::kWallSlideSpeed);
    CHECK(saw_slide);

    // Wall-jump NO consume air_jumps_ si también tiene Doble Salto.
    sim.grant_ability(game::kAbilityDoubleJump);
    place(sim, 180.f, 110.f);
    sim.step({1.f});
    { Input j; j.jump = true; j.move_x = 1.f; sim.step(j); }
    CHECK(sim.air_jumps() == 1);
    CHECK(sim.player().velocity().x < 0.f);

    // Escalar la chimenea de 136 px desde el piso (x=172, y=160) alternando paredes hasta goal.
    place(sim, 172.f, 160.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    CHECK(sim.player().grounded);
    // Primer salto hacia la pared derecha.
    { Input j; j.jump = true; j.move_x = 1.f; sim.step(j); }
    int push_dir = 1;
    bool climbed_top = false;
    for (int i = 0; i < 240 && !sim.finished(); ++i) {
      Input in;
      if (sim.player().pos.y < 30.f) {
        climbed_top = true;
        in.move_x = 1.f;
      } else {
        in.move_x = float(push_dir);
        const bool touch_r = (push_dir > 0) && (sim.player().pos.x >= 179.f);
        const bool touch_l = (push_dir < 0) && (sim.player().pos.x <= 165.f);
        if ((touch_r || touch_l) && (i % 9 == 0)) {
          in.jump = true;
          push_dir = -push_dir;
        }
      }
      sim.step(in);
    }
    std::printf("con wall-climb: pos=(%.1f, %.1f) top=%d finished=%d\n",
                sim.player().pos.x, sim.player().pos.y, int(climbed_top), int(sim.finished()));
    CHECK(climbed_top && sim.finished());

    // Save v4 round-trip preserva kAbilityWallClimb; restart (R corto) lo conserva, reset() (R largo) lo limpia.
    const auto bytes = game::encode_save(game::capture_save(sim));
    game::SaveData d;
    CHECK(game::decode_save(bytes.data(), bytes.size(), d) == game::SaveError::None);
    CHECK((d.abilities & game::kAbilityWallClimb) != 0u);

    game::Scene s2;
    CHECK(s2.load(argv[1], err));
    Sim sim2(s2);
    CHECK(game::apply_save(sim2, d) == game::SaveError::None);
    CHECK(sim2.has_ability(game::kAbilityWallClimb));
    CHECK(find_name(s2, "rel_wall_climb")->active);

    { Input r; r.restart = true; sim2.step(r); }
    CHECK(sim2.has_ability(game::kAbilityWallClimb));
    CHECK(find_name(s2, "rel_wall_climb")->active);

    { Input r; r.restart_full = true; sim2.step(r); }
    CHECK(!sim2.has_ability(game::kAbilityWallClimb));
    CHECK(!find_name(s2, "rel_wall_climb")->active);
  }

  std::puts("wallclimb_test OK");
  return 0;
}
