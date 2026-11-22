// Test sin GL: Etapas 12 y 13 — Game feel (Coyote Time, Jump Buffer, Squash & Stretch, Dust,
// estela de Dash, hitstop y sacudida de cámara en impactos).
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

static void place(Sim& s, float x, float y, float vy = 0.f) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = {x, y};
  p.prev = {x, y - vy};
  p.force = {};
  p.grounded = false;
}

static const game::Entity* find_name(const game::Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

int main(int argc, char** argv) {
  if (argc < 4) return 2;
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

  // --- 5) Hitstop + camera shake en pisotón y derrota de boss + build_world_frame + toggle off ---
  {
    game::Scene boss_scene;
    if (!boss_scene.load(argv[2], err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    Sim sim(boss_scene);
    CHECK(sim.impact_fx_enabled() && sim.hitstop_ticks() == 0 && sim.shake_x() == 0.f && sim.shake_y() == 0.f);
    for (int i = 0; i < 15; ++i) sim.step({});

    // Primer pisotón: dispara kHitstopEnemyFrames y sacudida; congela boss y jugador.
    {
      const auto& b = sim.boss(0);
      place(sim, b.x, b.y - Sim::kBossH - Sim::kPlayerRadius + 1.5f, 2.f);
      sim.step({});
    }
    CHECK(sim.boss(0).hits == 1);
    CHECK(sim.hitstop_ticks() == Sim::kHitstopEnemyFrames);
    const float hit_sx = sim.shake_x(), hit_sy = sim.shake_y();
    CHECK(std::fabs(hit_sx) > 0.f || std::fabs(hit_sy) > 0.f);

    // build_world_frame aplica shake sobre cam_x/y y deja intacto chunk_x_px/chunk_y_px.
    {
      FrameData fd{};
      build_world_frame(fd, boss_scene, sim, 0.f, false, false);
      CHECK(std::fabs(fd.cam_x - std::round(boss_scene.cam_x() + hit_sx)) < 1e-4f);
      CHECK(std::fabs(fd.cam_y - std::round(boss_scene.cam_y() + hit_sy)) < 1e-4f);
      CHECK(fd.chunk_x_px == boss_scene.nb_origin_x() && fd.chunk_y_px == boss_scene.nb_origin_y());
    }

    // Durante los kHitstopEnemyFrames pasos siguientes, boss y player quedan congelados.
    const float bx0 = sim.boss(0).x, by0 = sim.boss(0).y, hurt0 = sim.boss(0).hurt;
    const float px0 = sim.player().pos.x, py0 = sim.player().pos.y;
    for (int i = 0; i < Sim::kHitstopEnemyFrames; ++i) {
      Input j;
      if (i == Sim::kHitstopEnemyFrames - 1) j.jump = true;  // el buffer conserva salto durante hitstop
      sim.step(j);
      CHECK(sim.boss(0).x == bx0 && sim.boss(0).y == by0 && sim.boss(0).hurt == hurt0);
      CHECK(sim.player().pos.x == px0 && sim.player().pos.y == py0);
      CHECK(sim.hitstop_ticks() == Sim::kHitstopEnemyFrames - 1 - i);
    }
    CHECK(sim.hitstop_ticks() == 0);
    CHECK(sim.jump_buffer_ticks() == Sim::kJumpBufferFrames);

    // Al salir del hitstop, la simulación retoma limpiamente.
    sim.step({});
    CHECK(sim.boss(0).hurt < hurt0);
    CHECK(sim.player().pos.y != py0 || sim.boss(0).x != bx0);

    // Segundo pisotón y apagado inmediato con set_impact_fx_enabled(false).
    for (int i = 0; i < 70; ++i) sim.step({});
    {
      const auto& b = sim.boss(0);
      place(sim, b.x, b.y - Sim::kBossH - Sim::kPlayerRadius + 1.5f, 2.f);
      sim.step({});
    }
    CHECK(sim.boss(0).hits == 2 && sim.hitstop_ticks() == Sim::kHitstopEnemyFrames);
    sim.set_impact_fx_enabled(false);
    CHECK(!sim.impact_fx_enabled() && sim.hitstop_ticks() == 0 && sim.shake_x() == 0.f && sim.shake_y() == 0.f);
    {
      FrameData fd{};
      build_world_frame(fd, boss_scene, sim, 0.f, false, false);
      CHECK(std::fabs(fd.cam_x - std::round(boss_scene.cam_x())) < 1e-4f);
      CHECK(std::fabs(fd.cam_y - std::round(boss_scene.cam_y())) < 1e-4f);
    }

    // Re-activar para verificar derrota del boss: kHitstopDefeatFrames y mayor amplitud de shake.
    sim.set_impact_fx_enabled(true);
    for (int i = 0; i < 70; ++i) sim.step({});
    {
      const auto& b = sim.boss(0);
      place(sim, b.x, b.y - Sim::kBossH - Sim::kPlayerRadius + 1.5f, 2.f);
      sim.step({});
    }
    CHECK(sim.boss(0).defeated);
    CHECK(sim.hitstop_ticks() == Sim::kHitstopDefeatFrames);
    const float defeat_mag = std::hypot(sim.shake_x(), sim.shake_y());
    const float hit_mag = std::hypot(hit_sx, hit_sy);
    CHECK(defeat_mag >= hit_mag && (std::fabs(sim.shake_x()) >= 3.f || std::fabs(sim.shake_y()) >= 2.f));

    // Durante hitstop de derrota, victory_seq y boss_burst siguen avanzando mientras player queda quieto.
    const float dpx = sim.player().pos.x, dpy = sim.player().pos.y;
    const int burst0 = sim.boss_burst();
    for (int i = 0; i < Sim::kHitstopDefeatFrames; ++i) {
      sim.step({});
      CHECK(sim.player().pos.x == dpx && sim.player().pos.y == dpy);
      CHECK(sim.victory_seq().in_defeat_anim());
    }
    CHECK(sim.hitstop_ticks() == 0);
    CHECK(sim.boss_burst() < burst0);

    // Con impact_fx_enabled(false), reset() conserva la preferencia y el pisotón no dispara hitstop ni shake.
    sim.set_impact_fx_enabled(false);
    sim.reset();
    CHECK(!sim.impact_fx_enabled());
    for (int i = 0; i < 15; ++i) sim.step({});
    {
      const auto& b = sim.boss(0);
      place(sim, b.x, b.y - Sim::kBossH - Sim::kPlayerRadius + 1.5f, 2.f);
      sim.step({});
    }
    CHECK(sim.boss(0).hits == 1);
    CHECK(sim.hitstop_ticks() == 0 && sim.shake_x() == 0.f && sim.shake_y() == 0.f);
  }

  // --- 6) Romper break_wall con Dash dispara hitstop + shake; toggle off los suprime ---
  {
    game::Scene dash_scene;
    if (!dash_scene.load(argv[3], err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    Sim sim(dash_scene);
    sim.grant_ability(game::kAbilityDash);
    place(sim, 146.f, 156.f);
    for (int i = 0; i < 15; ++i) sim.step({});

    {
      Input d;
      d.move_x = 1.f;
      d.dash = true;
      sim.step(d);
    }
    bool broke_with_impact = false;
    for (int i = 0; i < Sim::kDashFrames + 6; ++i) {
      if (find_name(dash_scene, "wall_gate")->active) {
        broke_with_impact = (sim.hitstop_ticks() == Sim::kHitstopEnemyFrames) &&
                            (std::fabs(sim.shake_x()) > 0.f || std::fabs(sim.shake_y()) > 0.f);
        break;
      }
      sim.step({});
    }
    CHECK(broke_with_impact);

    // Con impact_fx desactivado, romper wall_gate no genera hitstop ni shake.
    sim.set_impact_fx_enabled(false);
    sim.reset();
    CHECK(!sim.impact_fx_enabled());
    sim.grant_ability(game::kAbilityDash);
    place(sim, 146.f, 156.f);
    for (int i = 0; i < 15; ++i) sim.step({});
    {
      Input d;
      d.move_x = 1.f;
      d.dash = true;
      sim.step(d);
    }
    for (int i = 0; i < Sim::kDashFrames + 2; ++i) {
      CHECK(sim.hitstop_ticks() == 0 && sim.shake_x() == 0.f && sim.shake_y() == 0.f);
      if (find_name(dash_scene, "wall_gate")->active) break;
      sim.step({});
    }
    CHECK(find_name(dash_scene, "wall_gate")->active);
    CHECK(sim.hitstop_ticks() == 0 && sim.shake_x() == 0.f && sim.shake_y() == 0.f);
  }

  std::puts("game_feel_test: OK");
  return 0;
}
