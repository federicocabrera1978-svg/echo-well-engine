// Test sin GL (Etapa 4): mecanismos ambientales nuevos.
//   - `crystal`: sólido hasta que lo rompe un Dash o un pisotón; suelta escombros, persiste en
//     respawn y save.ewsv, y vuelve con R largo.
//   - `timed_switch`: abre sus puertas linkeadas durante Sim::kTimedSwitchSec y las cierra solo,
//     de forma determinista; el estado transitorio no se restaura desde save.ewsv.
#include "audio/sfx.hpp"
#include "game/frame_view.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#define CHECK(c)                                                                \
  do {                                                                          \
    if (!(c)) {                                                                 \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
      return 1;                                                                 \
    }                                                                           \
  } while (0)

using namespace ew;
using game::Input;
using game::Scene;
using game::Sim;

static void place(Sim& s, float x, float y, float vy = 0.f) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = {x, y};
  p.prev = {x, y - vy};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

static const game::Entity* find_name(const Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

static int find_idx(const Scene& s, const char* name) {
  const auto& ents = s.entities();
  for (std::size_t i = 0; i < ents.size(); ++i)
    if (ents[i].name == name) return int(i);
  return -1;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: mechanisms_env_test <mechanisms.ewmap>\n");
    return 2;
  }

  Scene scene;
  std::string err;
  CHECK(scene.load(argv[1], err));

  const auto* crystal = find_name(scene, "crystal_a");
  const auto* sw = find_name(scene, "sw_timed");
  const auto* door = find_name(scene, "door_timed");
  CHECK(crystal && crystal->type == "crystal");
  CHECK(sw && sw->type == "timed_switch");
  CHECK(door && door->type == "door");
  const int idx_cr = find_idx(scene, "crystal_a");
  const int idx_sw = find_idx(scene, "sw_timed");
  CHECK(idx_cr >= 0 && idx_sw >= 0);

  // --- 1) crystal: sólido de entrada y bloquea el paso sin habilidad ---
  CHECK(scene.door_blocks(12, 18) && scene.door_blocks(12, 19));
  CHECK(scene.solid_tile(12, 18) && scene.solid_tile(12, 19));
  {
    Sim sim(scene);
    place(sim, 64.f, 152.f);
    for (int i = 0; i < 40; ++i) sim.step({1.f});
    CHECK(!crystal->active);
    CHECK(sim.player().pos.x <= 96.f - Sim::kPlayerRadius + 0.7f);
  }

  // --- 2) Dash rompe el cristal, suelta escombros y deja pasar ---
  {
    Sim sim(scene);
    sim.grant_ability(game::kAbilityDash);
    place(sim, 64.f, 152.f);
    sim.step({});
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
    for (int i = 0; i < 12 && !crystal->active; ++i) sim.step({1.f});
    CHECK(crystal->active);
    CHECK(!scene.door_blocks(12, 18) && !scene.door_blocks(12, 19));
    CHECK(sim.crystal_break_flash() > 0);
    CHECK(!sim.dust().empty());
    CHECK((sim.take_sfx() & ew::audio::kSfxBreak) != 0u);

    // Persiste tras morir / R corto.
    { Input rst; rst.restart = true; sim.step(rst); }
    CHECK(crystal->active);
    CHECK(!scene.solid_tile(12, 18));

    // save.ewsv conserva el cristal roto y lo restaura en otra escena.
    const auto bytes = game::encode_save(game::capture_save(sim));
    game::SaveData decoded;
    CHECK(game::decode_save(bytes.data(), bytes.size(), decoded) == game::SaveError::None);
    CHECK(decoded.active[std::size_t(idx_cr)] == 1);

    Scene s2;
    CHECK(s2.load(argv[1], err));
    Sim sim2(s2);
    CHECK(game::apply_save(sim2, decoded) == game::SaveError::None);
    CHECK(find_name(s2, "crystal_a")->active && !s2.solid_tile(12, 18));

    // R largo restaura el cristal y vuelve a ser sólido.
    { Input full; full.restart_full = true; sim.step(full); }
    CHECK(!crystal->active);
    CHECK(scene.solid_tile(12, 18) && scene.solid_tile(12, 19));
  }

  // --- 3) Pisotón: caer encima del cristal también lo rompe ---
  {
    Scene s_st;
    CHECK(s_st.load(argv[1], err));
    Sim sim(s_st);
    place(sim, 100.f, 128.f, 10.f);  // centrado sobre el cristal (96..104 px) para caerle encima
    for (int i = 0; i < 8 && !find_name(s_st, "crystal_a")->active; ++i) sim.step({});
    CHECK(find_name(s_st, "crystal_a")->active);
    CHECK(!s_st.solid_tile(12, 18));
    CHECK(sim.deaths() == 0);
  }

  // --- 4) timed_switch: abre la puerta y la cierra sola a los kTimedSwitchSec ---
  {
    Scene s_sw;
    CHECK(s_sw.load(argv[1], err));
    Sim sim(s_sw);
    const auto* sw1 = find_name(s_sw, "sw_timed");
    const auto* door1 = find_name(s_sw, "door_timed");
    CHECK(sw1 && door1);
    CHECK(!sw1->active && !door1->active);
    CHECK(s_sw.door_blocks(32, 17) && s_sw.door_blocks(32, 19));

    place(sim, 200.f, 152.f);
    sim.step({});
    { Input act; act.interact = true; sim.step(act); }
    CHECK(sw1->active && door1->active);
    CHECK(sim.timed_switch_left(idx_sw) > Sim::kTimedSwitchSec - 0.1f);

    // La puerta se abre visualmente (anim >= kDoorSolidUntil) y deja pasar.
    for (int i = 0; i < 20; ++i) sim.step({});
    CHECK(!s_sw.door_blocks(32, 17) && !s_sw.door_blocks(32, 19));

    // A mitad de la cuenta sigue abierta; al vencer se cierra y vuelve a colisionar.
    for (int i = 0; i < 120; ++i) sim.step({});
    CHECK(door1->active && sim.timed_switch_left(idx_sw) > 0.f);

    // Re-activarla reinicia la cuenta atrás (no se cierra antes de tiempo).
    place(sim, 200.f, 152.f);
    { Input act; act.interact = true; sim.step(act); }
    CHECK(sim.timed_switch_left(idx_sw) > Sim::kTimedSwitchSec - 0.1f);
    const int frames = int(Sim::kTimedSwitchSec * 60.f) + 40;
    for (int i = 0; i < frames; ++i) sim.step({});
    CHECK(!sw1->active && !door1->active);
    CHECK(std::fabs(sim.timed_switch_left(idx_sw)) < 1e-4f);
    CHECK(s_sw.door_blocks(32, 17) && s_sw.door_blocks(32, 19));

    // Determinismo: dos simulaciones con el mismo input tienen la misma cuenta atrás.
    Scene sa;
    Scene sb;
    CHECK(sa.load(argv[1], err));
    CHECK(sb.load(argv[1], err));
    Sim sim_a(sa);
    Sim sim_b(sb);
    place(sim_a, 200.f, 152.f);
    place(sim_b, 200.f, 152.f);
    sim_a.step({});
    sim_b.step({});
    { Input act; act.interact = true; sim_a.step(act); sim_b.step(act); }
    for (int i = 0; i < 100; ++i) { sim_a.step({}); sim_b.step({}); }
    CHECK(std::fabs(sim_a.timed_switch_left(idx_sw) - sim_b.timed_switch_left(idx_sw)) < 1e-5f);

    // El estado transitorio no se restaura desde save.ewsv: una palanca a medio abrir vuelve cerrada.
    const auto bytes = game::encode_save(game::capture_save(sim_a));
    game::SaveData decoded;
    CHECK(game::decode_save(bytes.data(), bytes.size(), decoded) == game::SaveError::None);
    Scene sc;
    CHECK(sc.load(argv[1], err));
    Sim sim_c(sc);
    CHECK(game::apply_save(sim_c, decoded) == game::SaveError::None);
    CHECK(!find_name(sc, "sw_timed")->active && !find_name(sc, "door_timed")->active);
    CHECK(sc.door_blocks(32, 17));
  }

  // --- 5) Render y luz sin GL: hay marcadores para cristal/palanca y luces puntuales ---
  {
    Scene s_r;
    CHECK(s_r.load(argv[1], err));
    Sim sim(s_r);
    place(sim, 96.f, 152.f);
    sim.step({});
    game::FrameData fv;
    game::build_world_frame(fv, s_r, sim, 0.5f, false, false);
    CHECK(fv.marker_count > 0);
    CHECK(fv.point_light_count > 0);

    place(sim, 200.f, 152.f);
    { Input act; act.interact = true; sim.step(act); }
    game::build_world_frame(fv, s_r, sim, 0.6f, false, false);
    CHECK(fv.marker_count > 0);
  }

  std::puts("mechanisms_env_test OK");
  return 0;
}