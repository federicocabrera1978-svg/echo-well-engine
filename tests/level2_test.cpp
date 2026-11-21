// Test sin GL: Nivel 2 "Las Catacumbas" (level2.ewmap, 4×2 chunks = 1280×368 px).
// Verifica la estructura metroidvania:
//   A) Zonas visibles pero bloqueadas al inicio sin Doble Salto (repisa de egg_catacomb_1 en (0,0)
//      y Galería Alta + door_hub_gallery cerrada en (1,0)).
//   B) Descenso vertical por el pozo (1,0) -> (1,1) al agua, cruce a (0,1) y obtención de
//      la reliquia ability_double_jump.
//   C) Ala subterránea este (1,1) -> (2,1) -> (3,1): Doble Salto al Escalón 1, plataforma móvil
//      sobre pinchos, switch sw_sanctum que abre door_hub_gallery y altar alto con egg_catacomb_2.
//   D) Backtracking vertical (1,1) -> (1,0) por los Escalones 1 y 2 y regreso a (0,0) para
//      recoger egg_catacomb_1 con Doble Salto (2/2 huevos).
//   E) Ascenso a la Galería Alta de (1,0), cruce por door_hub_gallery a (2,0) (gravedad invertida)
//      y (3,0) (boss_catacomb -> door_boss2 -> goal).
//   F) Progresión de campaña slice -> level2 y pantalla de final.
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

static void place(Sim& s, float x, float y) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = p.prev = {x, y};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

static const game::Entity* find_name(const game::Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

static float steer(float x, float target) {
  return std::clamp((target - x) / 6.f, -1.f, 1.f);
}

// Ejecuta un doble salto desde el suelo: salto vertical durante first_frames, segundo salto
// en el aire con empuje vertical durante pre_steer_frames y luego avance horizontal dir (-1 o +1).
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

// Vence al boss con 3 pisotones limpios sobre su cabeza.
static void defeat_boss(Sim& sim) {
  for (int k = 0; k < Sim::kBossHits && !sim.bosses_defeated(); ++k) {
    for (int i = 0; i < 70; ++i) sim.step({});
    const auto& b = sim.boss(0);
    auto& p = sim.world().points()[sim.player_index()];
    p.pos = {b.x, b.y - Sim::kBossH - Sim::kPlayerRadius + 1.5f};
    p.prev = {p.pos.x, p.pos.y - 2.f};
    sim.step({});
  }
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string err;
  game::Scene scene;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }

  // --- Invariantes del loader y geometría 4×2 ---
  CHECK(scene.width_px() == 1280 && scene.height_px() == 368);
  CHECK(scene.map().header().map_w_chunks == 4 && scene.map().header().map_h_chunks == 2);
  CHECK(scene.map().backdrops().size() == 8);
  const auto* spawn = find_name(scene, "spawn");
  const auto* egg1 = find_name(scene, "egg_catacomb_1");
  const auto* egg2 = find_name(scene, "egg_catacomb_2");
  const auto* rel_dj = find_name(scene, "rel_double_jump");
  const auto* sw_sanctum = find_name(scene, "sw_sanctum");
  const auto* door_hub = find_name(scene, "door_hub_gallery");
  const auto* door_boss = find_name(scene, "door_boss2");
  const auto* goal = find_name(scene, "goal");
  CHECK(spawn && spawn->x == 32.f && spawn->y == 152.f);
  CHECK(egg1 && egg1->x == 96.f && egg1->y == 88.f && !egg1->active);
  CHECK(egg2 && egg2->x == 1160.f && egg2->y == 272.f && !egg2->active);
  CHECK(rel_dj && rel_dj->x == 80.f && rel_dj->y == 336.f && !rel_dj->active);
  CHECK(sw_sanctum && door_hub && door_boss && goal);
  CHECK(!sw_sanctum->active && !door_hub->active && !door_boss->active);

  Sim sim(scene);
  CHECK(sim.collectible_total() == 2 && sim.collected() == 0);
  CHECK(sim.abilities() == 0 && !sim.has_ability(game::kAbilityDoubleJump));
  CHECK(sim.rope_count() == 1 && sim.water().size() == 1);
  CHECK(sim.mover_count() == 1 && sim.boss_count() == 1);

  // --- Paso A: Zonas teaseadas pero bloqueadas al inicio sin Doble Salto ---
  for (int i = 0; i < 20; ++i) sim.step({});
  CHECK(sim.player().grounded);
  {
    // Intento de subir a la repisa de egg_catacomb_1 en (0,0) (tope y = 96 px) sin Doble Salto.
    place(sim, 160.f, 156.f);
    for (int i = 0; i < 10; ++i) sim.step({});
    float min_y = 999.f;
    { Input j; j.jump = true; sim.step(j); }
    for (int i = 0; i < 45; ++i) {
      Input air;
      air.move_x = -1.f;
      air.jump = (i == 15);
      sim.step(air);
      min_y = std::fmin(min_y, sim.player().pos.y);
    }
    std::printf("paso A (0,0) sin doble salto: min_y=%.2f (repisa en y=96)\n", min_y);
    CHECK(min_y > 105.f && sim.collected() == 0);
  }
  {
    // Intento de subir a la Galería Alta en (1,0) (tope y = 96 px) sin Doble Salto.
    place(sim, 496.f, 156.f);
    for (int i = 0; i < 10; ++i) sim.step({});
    float min_y = 999.f;
    { Input j; j.jump = true; sim.step(j); }
    for (int i = 0; i < 45; ++i) {
      Input air;
      air.move_x = 1.f;
      air.jump = (i == 15);
      sim.step(air);
      min_y = std::fmin(min_y, sim.player().pos.y);
    }
    std::printf("paso A (1,0) sin doble salto: min_y=%.2f (galería en y=96)\n", min_y);
    CHECK(min_y > 105.f);
    CHECK(!door_hub->active && scene.door_blocks(79, 9));
  }

  // --- Paso B: Descenso vertical (1,0) -> (1,1) al agua y desbloqueo de Doble Salto en (0,1) ---
  place(sim, 340.f, 156.f);
  for (int i = 0; i < 10; ++i) sim.step({});
  bool wet = false;
  for (int i = 0; i < 300 && sim.player().pos.y < 310.f; ++i) {
    Input in;
    if (sim.player().pos.x < 418.f) in.move_x = 1.f;
    sim.step(in);
    wet = wet || sim.player_in_water();
  }
  for (int i = 0; i < 40; ++i) {
    sim.step({});
    wet = wet || sim.player_in_water();
  }
  std::printf("paso B pozo vertical: pos=(%.1f, %.1f) chunk=(%d,%d) cam_y=%.1f agua=%d\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), scene.chunk_y(),
              scene.cam_y(), int(wet));
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_hub");
  CHECK(scene.chunk_x() == 1 && scene.chunk_y() == 1);
  CHECK(wet && sim.deaths() == 0);
  CHECK(scene.cam_y() >= 170.f);

  // Salir de la pileta hacia la izquierda, activar ck_pit (368,336), cruzar a (0,1) y tomar rel_double_jump.
  for (int i = 0; i < 500 && !sim.has_ability(game::kAbilityDoubleJump); ++i) {
    Input in;
    in.move_x = -1.f;
    const float x = sim.player().pos.x;
    if (sim.player_in_water()) {
      in.jump = true;
    } else if (sim.player().grounded && ((x > 395.f && x < 416.f) || (x > 200.f && x < 214.f))) {
      in.jump = true;  // saltar el borde izquierdo de la pileta y el foso de 3 tiles en (0,1)
    }
    sim.step(in);
  }
  std::printf("paso B altar (0,1): pos=(%.1f, %.1f) chunk=(%d,%d) dj=%d\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), scene.chunk_y(),
              int(sim.has_ability(game::kAbilityDoubleJump)));
  CHECK(scene.chunk_x() == 0 && scene.chunk_y() == 1);
  CHECK(sim.has_ability(game::kAbilityDoubleJump) && sim.air_jumps() == 1);
  CHECK(rel_dj->active);
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_altar");
  CHECK(sim.deaths() == 0);

  // --- Paso C: Ala subterránea este (1,1) -> (2,1) -> (3,1) ---
  // Situarse al pie del Escalón 1 en (1,1) (x = 484, piso y = 340) y subir con Doble Salto a ty = 35 (y = 280).
  place(sim, 484.f, 340.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.air_jumps() == 1);
  perform_double_jump_to_ledge(sim, 1.f);
  for (int i = 0; i < 180 && sim.player().pos.x < 680.f; ++i) sim.step({1.f});
  std::printf("paso C entrada (2,1): pos=(%.1f, %.1f) chunk=(%d,%d)\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), scene.chunk_y());
  CHECK(sim.player().pos.y < 280.f);
  CHECK(scene.chunk_x() == 2 && scene.chunk_y() == 1);
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_east_low");

  // Viajar en plat_crypt (mover 0: x = 696..864, y = 280) sobre el foso de pinchos hasta tx = 112..119.
  {
    const auto& plat = sim.mover(0);
    int phase = 0;
    for (int steps = 0; steps < 2500 && sim.player().pos.x < 920.f; ++steps) {
      const float x = sim.player().pos.x;
      Input in;
      switch (phase) {
        case 0:  // esperar en el borde de la repisa izquierda (x = 684)
          in.move_x = steer(x, 684.f);
          if (std::fabs(x - 684.f) < 1.5f && plat.x <= 696.5f && sim.player().grounded) phase = 1;
          break;
        case 1:  // subir a plat_crypt y mantenerse centrado
          in.move_x = steer(x, plat.x + 16.f);
          if (plat.x >= 863.5f && sim.ridden() == 0) phase = 2;
          break;
        default:  // bajar a la repisa derecha y entrar a (3,1)
          in.move_x = 1.f;
          break;
      }
      sim.step(in);
    }
  }
  // Caminar en (3,1) y bajar a la fosa del santuario hasta sw_sanctum (1048, 336) y activarlo.
  for (int i = 0; i < 220 && sim.player().pos.x < sw_sanctum->x - 2.f; ++i) sim.step({1.f});
  for (int i = 0; i < 40 && !sim.player().grounded; ++i) sim.step({});
  { Input act; act.interact = true; sim.step(act); }
  for (int i = 0; i < 30; ++i) sim.step({});
  std::printf("paso C santuario (3,1): pos=(%.1f, %.1f) chunk=(%d,%d) sw=%d door_hub=%d\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), scene.chunk_y(),
              int(sw_sanctum->active), int(door_hub->active));
  CHECK(scene.chunk_x() == 3 && scene.chunk_y() == 1);
  CHECK(sw_sanctum->active && door_hub->active);
  CHECK(!scene.door_blocks(79, 9));

  // Subir con Doble Salto al altar alto de (3,1) (tx = 138..154, ty = 35, tope y = 280) y tomar egg_catacomb_2.
  place(sim, 1088.f, 340.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.air_jumps() == 1);
  perform_double_jump_to_ledge(sim, 1.f);
  for (int i = 0; i < 120 && sim.collected() < 1; ++i) sim.step({1.f});
  std::printf("paso C altar huevo 2: pos=(%.1f, %.1f) huevos=%d/%d\n",
              sim.player().pos.x, sim.player().pos.y, sim.collected(), sim.collectible_total());
  CHECK(sim.collected() == 1 && egg2->active);

  // --- Paso D: Backtracking vertical (1,1) -> (1,0) y regreso a (0,0) por egg_catacomb_1 ---
  // Desde el Escalón 1 en (1,1) (x = 496, y = 276), Doble Salto hacia la izquierda al Escalón 2 (tx = 55..60, y = 224).
  place(sim, 496.f, 276.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.air_jumps() == 1);
  perform_double_jump_to_ledge(sim, -1.f);
  for (int i = 0; i < 20 && !sim.player().grounded; ++i) sim.step({});
  std::printf("paso D escalón 2: pos=(%.1f, %.1f) grounded=%d\n",
              sim.player().pos.x, sim.player().pos.y, int(sim.player().grounded));
  CHECK(sim.player().grounded && std::fabs(sim.player().pos.y - 220.f) < 2.f);

  // Desde el Escalón 2 (x = 472, y = 220), Doble Salto hacia la derecha para salir por la boca del pozo al Hub (1,0).
  place(sim, 472.f, 220.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.air_jumps() == 1);
  perform_double_jump_to_ledge(sim, 1.f);
  for (int i = 0; i < 30 && !sim.player().grounded; ++i) sim.step({1.f});
  std::printf("paso D salida al Hub (1,0): pos=(%.1f, %.1f) chunk=(%d,%d)\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), scene.chunk_y());
  CHECK(scene.chunk_y() == 0 && sim.player().pos.y < 162.f);

  // Backtracking a (0,0): subir con Doble Salto a la repisa de entrada (tx = 6..18, ty = 12, tope y = 96) y tomar egg_catacomb_1.
  place(sim, 168.f, 156.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.air_jumps() == 1);
  perform_double_jump_to_ledge(sim, -1.f);
  for (int i = 0; i < 120 && sim.collected() < 2; ++i) sim.step({-1.f});
  std::printf("paso D repisa (0,0) huevo 1: pos=(%.1f, %.1f) huevos=%d/%d\n",
              sim.player().pos.x, sim.player().pos.y, sim.collected(), sim.collectible_total());
  CHECK(scene.chunk_x() == 0 && scene.chunk_y() == 0);
  CHECK(sim.collected() == 2 && egg1->active);

  // --- Paso E: Galería Alta (1,0) -> Galería Invertida (2,0) -> Cámara del Guardián (3,0) ---
  place(sim, 496.f, 156.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.air_jumps() == 1);
  perform_double_jump_to_ledge(sim, 1.f);
  // Caminar por la Galería Alta y cruzar door_hub_gallery abierta hacia (2,0) hasta ck_grav2 y flip_up2.
  for (int i = 0; i < 350 && !sim.gravity_flipped(); ++i) sim.step({1.f});
  CHECK(scene.chunk_x() == 2 && scene.chunk_y() == 0);
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_grav2");
  CHECK(sim.gravity_flipped());
  // Esperar a posarse en el techo (y ≈ 28) y caminar sobre el foso de pinchos hasta flip_down2 (904, 28) y (3,0).
  for (int i = 0; i < 40 && !sim.player().grounded; ++i) sim.step({});
  for (int i = 0; i < 400 && !(sim.checkpoint() && sim.checkpoint()->name == "ck_boss2"); ++i)
    sim.step({1.f});
  std::printf("paso E entrada boss (3,0): pos=(%.1f, %.1f) chunk=(%d,%d) flipped=%d\n",
              sim.player().pos.x, sim.player().pos.y, scene.chunk_x(), scene.chunk_y(),
              int(sim.gravity_flipped()));
  CHECK(scene.chunk_x() == 3 && scene.chunk_y() == 0);
  CHECK(!sim.gravity_flipped());
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_boss2");

  // Derrotar a boss_catacomb (3 pisotones), cruzar door_boss2 y tocar goal.
  defeat_boss(sim);
  CHECK(sim.bosses_defeated() && door_boss->active);
  for (int i = 0; i < 350 && !sim.finished(); ++i) sim.step({1.f});
  std::printf("paso E meta: pos=(%.1f, %.1f) finished=%d huevos=%d/%d muertes=%d\n",
              sim.player().pos.x, sim.player().pos.y, int(sim.finished()),
              sim.collected(), sim.collectible_total(), sim.deaths());
  CHECK(sim.finished());
  CHECK(sim.collected() == 2 && sim.collectible_total() == 2);
  CHECK(sim.has_ability(game::kAbilityDoubleJump));
  CHECK(sim.deaths() == 0);

  // --- Paso F: Vínculo de campaña slice -> level2 y guardado v3 de level2 ---
  CHECK(game::next_campaign_map("slice") == "level2");
  CHECK(game::next_campaign_map("level2").empty());
  if (argc >= 3) {
    const std::string resolved = game::resolve_map_file(game::next_campaign_map("slice"), argv[2], "");
    CHECK(!resolved.empty());
    game::EndScreen end;
    game::EndStats st;
    st.title = game::level_title("slice");
    st.has_next = true;
    end.open(st);
    CHECK(end.stats().has_next && end.cursor() == game::EndScreen::kNext);
    st.title = game::level_title("level2");
    st.has_next = false;
    end.open(st);
    CHECK(!end.stats().has_next && end.cursor() == game::EndScreen::kRestart);
  }
  {
    const auto bytes = game::encode_save(game::capture_save(sim));
    game::SaveData d;
    CHECK(game::decode_save(bytes.data(), bytes.size(), d) == game::SaveError::None);
    CHECK(d.map == "level2");
    CHECK(d.abilities == game::kAbilityDoubleJump);
    game::Scene s2;
    CHECK(s2.load(argv[1], err));
    Sim sim2(s2);
    CHECK(game::apply_save(sim2, d) == game::SaveError::None);
    CHECK(sim2.collected() == 2 && sim2.has_ability(game::kAbilityDoubleJump));
    CHECK(sim2.bosses_defeated());
  }

  std::puts("level2_test: OK");
  return 0;
}
