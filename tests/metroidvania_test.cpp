// Test integral de campaña Metroidvania (Etapa 10, sin GL):
// 1. Progresión de campaña slice -> level2 -> level3 y resolución de mapas (campaign.hpp).
// 2. Chequeo anti-softlock de spawn y de todos los checkpoints en los 3 niveles (sin META 4 ni aplastamiento).
// 3. Transferencia de habilidades entre niveles, round-trip save.ewsv v3 y SFX de Dash / Break / Hit.
// 4. Cierre de campaña en level3 con EndScreen ("VICTORIA: REINO DESPIERTO" y rango S).
#include "audio/sfx.hpp"
#include "game/campaign.hpp"
#include "game/end_screen.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"

#include <cstdio>
#include <string>

#define CHECK(c)                                                      \
  do {                                                                \
    if (!(c)) {                                                       \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);        \
      return 1;                                                       \
    }                                                                 \
  } while (0)

namespace {
using namespace ew::game;

void move_to(Sim& sim, float x, float y) {
  auto& p = sim.world().points()[sim.player_index()];
  p.pos = p.prev = {x, y};
  sim.scene().follow(x, y);
}

int find_ent(const Scene& s, std::string_view name) {
  const auto& ents = s.entities();
  for (std::size_t i = 0; i < ents.size(); ++i)
    if (ents[i].name == name) return int(i);
  return -1;
}

bool verify_checkpoints_safe(const char* map_path) {
  Scene s;
  std::string err;
  if (!s.load(map_path, err)) {
    std::printf("load error %s: %s\n", map_path, err.c_str());
    return false;
  }
  if (!s.find_type("player_spawn") || !s.find_type("goal")) return false;
  const auto& ents = s.entities();
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].type != "checkpoint" && ents[i].type != "player_spawn") continue;
    Sim sim(s);
    move_to(sim, ents[i].x, ents[i].y);
    const int cx = int(ents[i].x) / (ew::map::kChunkW * ew::map::kTileSize);
    const int cy = int(ents[i].y) / (ew::map::kChunkH * ew::map::kTileSize);
    if (!sim.scene().chunk_visited(cx, cy)) return false;
    const int d0 = sim.deaths();
    for (int f = 0; f < 5; ++f) sim.step({});
    if (sim.deaths() != d0) {
      std::printf("unsafe checkpoint/spawn %s in %s\n", ents[i].name.c_str(), map_path);
      return false;
    }
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  CHECK(argc >= 4);
  const std::string slice_path = argv[1];
  const std::string level2_path = argv[2];
  const std::string level3_path = argv[3];

  // 1) Tabla de campaña y resolución de mapas.
  CHECK(next_campaign_map("slice") == "level2");
  CHECK(next_campaign_map("level2") == "level3");
  CHECK(next_campaign_map("level3").empty());
  CHECK(level_title("slice") == "NIVEL 1: EL POZO");
  CHECK(level_title("level2") == "NIVEL 2: LAS CATACUMBAS");
  CHECK(level_title("level3") == "NIVEL 3: LA GUARIDA DEL WYRM");
  CHECK(resolve_map_file("level2", slice_path, "") == level2_path);
  CHECK(resolve_map_file("level3", level2_path, "") == level3_path);

  // 2) Anti-softlock en spawn y todos los checkpoints de la campaña.
  CHECK(verify_checkpoints_safe(slice_path.c_str()));
  CHECK(verify_checkpoints_safe(level2_path.c_str()));
  CHECK(verify_checkpoints_safe(level3_path.c_str()));

  // 3) Nivel 2 -> desbloqueo de habilidades, rotura de muro y round-trip save.ewsv v3.
  uint32_t carried_abilities = 0;
  {
    Scene s2;
    std::string err;
    CHECK(s2.load(level2_path, err));
    Sim sim2(s2);
    CHECK(sim2.boss_count() == 1 && sim2.boss(0).guardian2 && !sim2.boss(0).dragon);
    CHECK(sim2.boss(0).speed() > Sim::kBossSpeed);
    CHECK(s2.dialogs()[0].text.find("IMPEDIMENTO") != std::string::npos);
    CHECK(s2.dialogs()[0].text.find("CORAZON PURO") != std::string::npos);
    CHECK(s2.dialogs()[2].text.find("DE ALLI UNA VEZ VINISTE") != std::string::npos);
    CHECK(s2.dialogs()[2].text.find("PUREZA") != std::string::npos);
    sim2.set_abilities(kAbilityDoubleJump | kAbilityDash);
    const int wall_i = find_ent(s2, "wall_guardian");
    const int egg1_i = find_ent(s2, "egg_catacomb_1");
    const int ck_i = find_ent(s2, "ck_boss2");
    CHECK(wall_i >= 0 && egg1_i >= 0 && ck_i >= 0);

    // Toma checkpoint y huevo, rompe wall_guardian con Dash.
    move_to(sim2, s2.entities()[std::size_t(ck_i)].x, s2.entities()[std::size_t(ck_i)].y);
    sim2.step({});
    CHECK(sim2.checkpoint_index() == ck_i);
    move_to(sim2, s2.entities()[std::size_t(egg1_i)].x, s2.entities()[std::size_t(egg1_i)].y);
    sim2.step({});
    CHECK(sim2.collected() == 1);

    move_to(sim2, s2.entities()[std::size_t(wall_i)].x - 10.f, s2.entities()[std::size_t(wall_i)].y + 12.f);
    (void)sim2.take_sfx();
    Input in;
    in.move_x = 1.f;
    in.dash = true;
    sim2.step(in);
    for (int f = 0; f < 8; ++f) sim2.step({});
    const uint32_t bits = sim2.take_sfx();
    CHECK((bits & ew::audio::kSfxDash) != 0u);
    CHECK((bits & ew::audio::kSfxBreak) != 0u);
    CHECK(s2.entities()[std::size_t(wall_i)].active);

    // Guardado y restauración en instancia limpia de level2.
    const SaveData sv = capture_save(sim2);
    const auto raw = encode_save(sv);
    SaveData decoded;
    CHECK(decode_save(raw.data(), raw.size(), decoded) == SaveError::None);
    Scene s2_reload;
    CHECK(s2_reload.load(level2_path, err));
    Sim sim2_reload(s2_reload);
    CHECK(apply_save(sim2_reload, decoded) == SaveError::None);
    CHECK(sim2_reload.abilities() == (kAbilityDoubleJump | kAbilityDash));
    CHECK(sim2_reload.collected() == 1);
    CHECK(s2_reload.entities()[std::size_t(wall_i)].active);
    carried_abilities = sim2_reload.abilities();
  }

  // 4) Nivel 3 -> recibe habilidades de level2, derrota imp con SFX Hit y cierra campaña con rango S.
  {
    Scene s3;
    std::string err;
    CHECK(s3.load(level3_path, err));
    Sim sim3(s3);
    CHECK(s3.dialogs()[0].text.find("DE ALLI UNA VEZ VINISTE") != std::string::npos);
    CHECK(s3.dialogs()[0].text.find("PUREZA") != std::string::npos);
    sim3.set_abilities(carried_abilities);
    CHECK(sim3.has_ability(kAbilityDoubleJump));
    CHECK(sim3.has_ability(kAbilityDash));
    CHECK(sim3.imp_count() == 1);

    const auto& im0 = sim3.imp(0);
    move_to(sim3, im0.home_x - 18.f, im0.y - 4.f);
    (void)sim3.take_sfx();
    Input dash_in;
    dash_in.move_x = 1.f;
    dash_in.dash = true;
    sim3.step(dash_in);
    for (int f = 0; f < 10 && !sim3.imp(0).defeated; ++f) sim3.step({});
    CHECK(sim3.imp(0).defeated);
    CHECK((sim3.take_sfx() & ew::audio::kSfxHit) != 0u);

    // Derrotar al Wyrm activa el estallido visual y sonoro de rayos catódicos (kSfxBossCrt + boss_burst).
    for (int hit = 0; hit < Sim::kDragonHits && !sim3.bosses_defeated(); ++hit) {
      for (int f = 0; f < 120 && (sim3.boss(0).hurt > 0.f || sim3.boss(0).phase == 2 || sim3.dash_ticks() > 0); ++f) {
        move_to(sim3, 648.f, 88.f);
        sim3.step({});
      }
      const auto& b = sim3.boss(0);
      auto& pl = sim3.world().points()[sim3.player_index()];
      pl.pos = {b.x, b.y - Sim::kDragonH - Sim::kPlayerRadius + 1.5f};
      pl.prev = {pl.pos.x, pl.pos.y - 2.f};
      sim3.step({});
    }
    CHECK(sim3.bosses_defeated());
    CHECK(sim3.boss_burst() > 0);
    CHECK((sim3.take_sfx() & ew::audio::kSfxBossCrt) != 0u);

    EndScreen es;
    EndStats st;
    st.title = level_title(s3.map().string_at(1));
    st.items = 2;
    st.items_total = 2;
    st.deaths = 0;
    st.abilities = sim3.abilities();
    st.has_next = !next_campaign_map(s3.map().string_at(1)).empty();
    st.elapsed_sec = sim3.elapsed_sec();
    st.campaign_done = !st.has_next;
    es.open(st);
    const auto txt = end_screen_text(es);
    CHECK(txt[0] == "VICTORIA: REINO DESPIERTO");
    CHECK(txt[5].find("RANGO: S") != std::string::npos);
  }

  std::puts("metroidvania_test ok");
  return 0;
}
