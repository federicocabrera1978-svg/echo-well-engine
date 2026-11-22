// Test sin GL: Etapa 12 (Ítem 13) — Game feel (Coyote Time, Jump Buffer, Squash & Stretch, Dust y estela de Dash).
#include "game/frame_view.hpp"
#include "game/sim.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)

using namespace ew;
using game::FrameData;
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
  if (!scene.load(argv[1], err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }

  // --- 1) Coyote Time: 3 frames fuera del borde usa salto normal; 7+ frames usa Doble Salto ---
  {
    Sim sim(scene);
    sim.grant_ability(game::kAbilityDoubleJump);
    // Repisa alta en ability.yaml: x=192..312, techo en y=96 -> centro apoyado en y=92.
    place(sim, 200.f, 92.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    CHECK(sim.player().grounded);
    CHECK(sim.air_jumps() == 1);
    CHECK(sim.coyote_ticks() == Sim::kCoyoteFrames);

    // Caminar a la izquierda hasta salir del borde (x < 192).
    for (int i = 0; i < 30 && sim.player().grounded; ++i) sim.step({-1.f});
    CHECK(!sim.player().grounded);
    // Avanzar 2 frames adicionales en el aire (3 frames totales fuera del borde).
    for (int i = 0; i < 2; ++i) sim.step({-1.f});
    CHECK(!sim.player().grounded && sim.coyote_ticks() > 0);
    {
      Input j;
      j.jump = true;
      sim.step(j);
    }
    const float vy_coyote = sim.player().velocity().y;
    CHECK(vy_coyote < -200.f * Sim::kDt);
    CHECK(sim.air_jumps() == 1);  // no gastó el doble salto
    CHECK(sim.coyote_ticks() == 0);

    // Ahora dejar expirar el coyote time (7+ frames fuera del borde) -> dispara doble salto.
    place(sim, 200.f, 92.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    CHECK(sim.player().grounded && sim.air_jumps() == 1);
    for (int i = 0; i < 30 && sim.player().grounded; ++i) sim.step({-1.f});
    CHECK(!sim.player().grounded);
    for (int i = 0; i < 7; ++i) sim.step({-1.f});
    CHECK(!sim.player().grounded && sim.coyote_ticks() == 0);
    {
      Input j;
      j.jump = true;
      sim.step(j);
    }
    CHECK(sim.air_jumps() == 0);  // usó el doble salto
    CHECK(sim.double_jump_flash() == Sim::kDoubleJumpFlashFrames);
  }

  // --- 2) Jump Buffer: apretar salto 3 frames antes de tocar el piso dispara el salto al aterrizar ---
  {
    Sim sim(scene);
    sim.set_abilities(0);
    // Piso en y=160 -> centro apoyado en y=156. Caer desde x=160 (lejos de rel_dj en x=120), y=135 hasta ~3 frames sobre el piso.
    place(sim, 160.f, 135.f);
    for (int i = 0; i < 15 && !sim.player().grounded && sim.player().pos.y < 151.f; ++i) sim.step({});
    CHECK(!sim.player().grounded && sim.coyote_ticks() == 0 && sim.air_jumps() == 0);
    {
      Input j;
      j.jump = true;
      sim.step(j);
    }
    CHECK(!sim.player().grounded);
    CHECK(sim.jump_buffer_ticks() == Sim::kJumpBufferFrames);

    bool jumped_on_land = false;
    for (int i = 0; i < 6; ++i) {
      sim.step({});
      if (sim.player().velocity().y < -200.f * Sim::kDt) {
        jumped_on_land = true;
        break;
      }
    }
    CHECK(jumped_on_land);
    CHECK(sim.jump_buffer_ticks() == 0);
  }

  // --- 3) Squash & Stretch + Dust en salto y aterrizaje ---
  {
    Sim sim(scene);
    place(sim, 120.f, 156.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    CHECK(sim.player().grounded);
    CHECK(sim.squash_x() == 1.f && sim.squash_y() == 1.f);

    // Salto: estira verticalmente (squash_x < 1, squash_y > 1) y genera partículas de despegue.
    {
      Input j;
      j.jump = true;
      sim.step(j);
    }
    CHECK(sim.squash_x() < 1.f && sim.squash_y() > 1.f);
    CHECK(sim.dust().size() >= 3);

    // Caer hasta aterrizar (sin re-apretar salto).
    bool landed_squash = false;
    bool prev_ground = sim.player().grounded;
    for (int i = 0; i < 90; ++i) {
      sim.step({});
      if (!prev_ground && sim.player().grounded) {
        landed_squash = (sim.squash_x() > 1.f && sim.squash_y() < 1.f && sim.dust().size() >= 4);
        break;
      }
      prev_ground = sim.player().grounded;
    }
    CHECK(landed_squash);

    // Decaimiento limpio a (1, 1) y dust() vacío tras ~30 frames.
    for (int i = 0; i < 30; ++i) sim.step({});
    CHECK(sim.squash_x() == 1.f && sim.squash_y() == 1.f);
    CHECK(sim.dust().empty());
  }

  // --- 4) Dash dust trail + FrameData (player_sprite_idx, squash y estela residual) ---
  {
    Sim sim(scene);
    sim.grant_ability(game::kAbilityDash);
    place(sim, 120.f, 156.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    CHECK(sim.player().grounded);

    {
      Input d;
      d.move_x = 1.f;
      d.dash = true;
      sim.step(d);
    }
    CHECK(sim.dash_ticks() == Sim::kDashFrames);
    CHECK(sim.squash_x() > 1.f && sim.squash_y() < 1.f);
    CHECK(!sim.dust().empty());

    FrameData fd{};
    build_world_frame(fd, scene, sim, 0.f, false, false);
    CHECK(fd.player_sprite_idx >= 0);
    CHECK(std::fabs(fd.player_squash_x - sim.squash_x()) < 1e-5f);
    CHECK(std::fabs(fd.player_squash_y - sim.squash_y()) < 1e-5f);
    CHECK(fd.sprite_count >= 3);  // 2 estelas de dash + sprite principal
    CHECK(fd.marker_count > 0);   // partículas de polvo

    // Al salir del dash con dash_flash() > 0 sigue dibujando 1 afterimage residual.
    for (int i = 0; i < Sim::kDashFrames; ++i) sim.step({});
    CHECK(sim.dash_ticks() == 0 && sim.dash_flash() > 0);
    build_world_frame(fd, scene, sim, 0.f, false, false);
    CHECK(fd.player_sprite_idx >= 0);
    CHECK(fd.sprite_count >= 2);  // afterimage residual + sprite principal

    // respawn y reset limpian dust, timers y squash.
    { Input r; r.restart = true; sim.step(r); }
    CHECK(sim.coyote_ticks() == 0 && sim.jump_buffer_ticks() == 0);
    CHECK(sim.squash_x() == 1.f && sim.squash_y() == 1.f);
    CHECK(sim.dust().empty());
  }

  std::puts("game_feel_test: OK");
  return 0;
}
