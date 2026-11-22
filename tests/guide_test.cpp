// Test sin GL: flecha guía de navegación (engine/game/guide.hpp + sección GUIDE = 11)
// sobre slice.ewmap, level2.ewmap, level3.ewmap y fallback por links en hello.ewmap.
#include "game/guide.hpp"
#include "game/sim.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)

using namespace ew;
using game::GuideArrow;
using game::Input;
using game::Scene;
using game::Sim;

static void place(Sim& s, float x, float y) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = p.prev = {x, y};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

static const game::Entity* find_name(const Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

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

static int test_slice(const char* path) {
  Scene scene;
  std::string err;
  if (!scene.load(path, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  CHECK(scene.map().guides().size() == 2);

  Sim sim(scene);
  CHECK(std::fabs(sim.player().pos.x - 32.f) < 1e-3f && std::fabs(sim.player().pos.y - 152.f) < 1e-3f);

  // En el spawn (32, 152), apunta a switch_start (96, 152), style == 0.
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid);
    CHECK(ga.target_x == 96.f && ga.target_y == 152.f);
    CHECK(ga.style == 0);
    CHECK(ga.ux > 0.7f);
  }

  // A <= 24 px de switch_start, devuelve !valid.
  place(sim, 96.f - 24.f, 152.f);
  CHECK(!game::compute_guide_arrow(scene, sim).valid);
  place(sim, 96.f - 10.f, 152.f);
  CHECK(!game::compute_guide_arrow(scene, sim).valid);

  // Al activar switch_start, apunta a flood (776, 56), style == 0, y NO a switch_well.
  place(sim, 96.f, 152.f);
  { Input in; in.interact = true; sim.step(in); }
  const auto* sw_start = find_name(scene, "switch_start");
  const auto* door_start = find_name(scene, "door_start");
  const auto* sw_well = find_name(scene, "switch_well");
  const auto* flood = find_name(scene, "flood");
  CHECK(sw_start && sw_start->active && door_start && door_start->active);
  CHECK(sw_well && !sw_well->active && flood && !flood->active);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid);
    CHECK(ga.target_x == 776.f && ga.target_y == 56.f);
    CHECK(ga.style == 0);
    CHECK(ga.target_x != sw_well->x);
  }

  // Al avanzar más allá de x > 1130, devuelve !valid.
  place(sim, 1135.f, 56.f);
  CHECK(!game::compute_guide_arrow(scene, sim).valid);

  // Al volver antes de 1130 aún apunta a flood; al activar flood, devuelve !valid.
  place(sim, 700.f, 56.f);
  CHECK(game::compute_guide_arrow(scene, sim).valid);
  place(sim, 776.f, 56.f);
  { Input in; in.interact = true; sim.step(in); }
  CHECK(flood->active);
  place(sim, 700.f, 56.f);
  CHECK(!game::compute_guide_arrow(scene, sim).valid);

  // Cuando sim.finished(), devuelve !valid aunque quede un objetivo pendiente.
  {
    Scene s_fin;
    if (!s_fin.load(path, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    Sim sim_fin(s_fin);
    CHECK(game::compute_guide_arrow(s_fin, sim_fin).valid);
    defeat_boss(sim_fin);
    CHECK(sim_fin.bosses_defeated());
    place(sim_fin, 2520.f, 152.f);
    sim_fin.step({});
    CHECK(sim_fin.finished());
    CHECK(!game::compute_guide_arrow(s_fin, sim_fin).valid);
    place(sim_fin, 32.f, 152.f);
    CHECK(!game::compute_guide_arrow(s_fin, sim_fin).valid);
  }

  return 0;
}

static int test_level2(const char* path) {
  Scene scene;
  std::string err;
  if (!scene.load(path, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  CHECK(scene.map().guides().size() == 5);

  Sim sim(scene);

  // 1) rel_double_jump (style == 1, (80, 336)):
  //    arriba en (32, 152) rutea hacia el pozo vertical (ux > 0.7, hacia x = 448).
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 1);
    CHECK(ga.target_x == 80.f && ga.target_y == 336.f);
    CHECK(ga.ux > 0.7f);
  }
  //    sobre la boca del pozo (448, 152) apunta hacia abajo (uy > 0.7).
  place(sim, 448.f, 152.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 1);
    CHECK(ga.target_x == 80.f && ga.target_y == 336.f);
    CHECK(ga.uy > 0.7f);
  }
  //    ya abajo (380, 336) apunta directo a (80, 336) (ux < -0.7).
  place(sim, 380.f, 336.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 1);
    CHECK(ga.target_x == 80.f && ga.target_y == 336.f);
    CHECK(ga.ux < -0.7f);
  }

  // 2) Tras tomar rel_double_jump, apunta a sw_sanctum (1048, 336) (style == 0).
  place(sim, 80.f, 336.f);
  sim.step({});
  CHECK(sim.has_ability(game::kAbilityDoubleJump));
  CHECK(find_name(scene, "rel_double_jump")->active);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 1048.f && ga.target_y == 336.f);
  }

  // 3) Tras activar sw_sanctum (abre door_hub_gallery), apunta a rel_dash (1208, 272) (style == 1).
  place(sim, 1048.f, 336.f);
  { Input in; in.interact = true; sim.step(in); }
  CHECK(find_name(scene, "sw_sanctum")->active && find_name(scene, "door_hub_gallery")->active);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 1);
    CHECK(ga.target_x == 1208.f && ga.target_y == 272.f);
  }

  // 4) Tras tomar rel_dash:
  place(sim, 1208.f, 272.f);
  sim.step({});
  CHECK(sim.has_ability(game::kAbilityDash));
  CHECK(find_name(scene, "rel_dash")->active);

  //    en planta baja (1160, 272) apunta hacia door_hub_gallery (632, 84) ruteando hacia el pozo 472 (ux < -0.7).
  place(sim, 1160.f, 272.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 632.f && ga.target_y == 84.f);
    CHECK(ga.ux < -0.7f);
  }
  //    dentro del pozo (472, 270) apunta hacia arriba (uy < -0.7).
  place(sim, 472.f, 270.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 632.f && ga.target_y == 84.f);
    CHECK(ga.uy < -0.7f);
  }
  //    ya arriba antes de x = 640 sigue válida hacia la puerta, y pasando x > 640 devuelve !valid.
  place(sim, 500.f, 152.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.target_x == 632.f && ga.target_y == 84.f);
  }
  place(sim, 650.f, 152.f);
  CHECK(!game::compute_guide_arrow(scene, sim).valid);

  return 0;
}

static int test_level3(const char* path) {
  Scene scene;
  std::string err;
  if (!scene.load(path, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  CHECK(scene.map().guides().size() == 3);

  Sim sim(scene);

  // Apunta primero a sw_seal3 (224, 336) (style == 0) ruteado por el pozo 568 desde planta alta.
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 224.f && ga.target_y == 336.f);
    CHECK(ga.ux > 0.7f);
  }
  place(sim, 568.f, 152.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 224.f && ga.target_y == 336.f);
    CHECK(ga.uy > 0.7f);
  }
  place(sim, 500.f, 336.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 224.f && ga.target_y == 336.f);
    CHECK(ga.ux < -0.7f);
  }

  // Tras activar sw_seal3, en planta baja apunta a door_wyrm_seal (632, 84) ruteando hacia arriba por el pozo 568.
  place(sim, 224.f, 336.f);
  { Input in; in.interact = true; sim.step(in); }
  CHECK(sim.deaths() == 0);
  CHECK(find_name(scene, "sw_seal3")->active && find_name(scene, "door_wyrm_seal")->active);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 632.f && ga.target_y == 84.f);
    CHECK(ga.ux > 0.7f);
  }
  place(sim, 568.f, 280.f);
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 632.f && ga.target_y == 84.f);
    CHECK(ga.uy < -0.7f);
  }

  // Al subir a planta alta (y < 184), devuelve !valid.
  place(sim, 568.f, 152.f);
  CHECK(!game::compute_guide_arrow(scene, sim).valid);

  return 0;
}

static int test_hello_fallback(const char* path) {
  Scene scene;
  std::string err;
  if (!scene.load(path, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  CHECK(scene.map().guides().empty());

  Sim sim(scene);

  // Fallback por links: apunta a switch_a (96, 152) mientras door_a está cerrada.
  {
    const GuideArrow ga = game::compute_guide_arrow(scene, sim);
    CHECK(ga.valid && ga.style == 0);
    CHECK(ga.target_x == 96.f && ga.target_y == 152.f);
    CHECK(ga.ux > 0.7f);
  }

  // Al abrir door_a activando switch_a, devuelve !valid.
  place(sim, 96.f, 152.f);
  { Input in; in.interact = true; sim.step(in); }
  CHECK(find_name(scene, "switch_a")->active && find_name(scene, "door_a")->active);
  place(sim, 32.f, 152.f);
  CHECK(!game::compute_guide_arrow(scene, sim).valid);

  return 0;
}

int main(int argc, char** argv) {
  if (argc < 5) return 2;
  if (test_slice(argv[1]) != 0) return 1;
  if (test_level2(argv[2]) != 0) return 1;
  if (test_level3(argv[3]) != 0) return 1;
  if (test_hello_fallback(argv[4]) != 0) return 1;
  std::printf("guide_test: slice + level2 + level3 + hello fallback OK\n");
  return 0;
}
