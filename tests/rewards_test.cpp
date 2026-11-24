// Test sin GL (Etapa 11 Ítem 12): recompensas por hitos de huevos (+1 HP a 2/6,
// Dash II a 4/6 con 2 cargas aéreas, 14f duración y 10f enfriamiento) y Final Verdadero (6/6).
#include "audio/sfx.hpp"
#include "game/campaign.hpp"
#include "game/end_screen.hpp"
#include "game/frame_view.hpp"
#include "game/hud.hpp"
#include "game/level_flow.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"
#include "game/story.hpp"

#include <cstdio>
#include <string>
#include <vector>

#define CHECK(c)                                                                \
  do {                                                                          \
    if (!(c)) {                                                                 \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
      return 1;                                                                 \
    }                                                                           \
  } while (0)

using namespace ew;
using game::DialogueBox;
using game::EndScreen;
using game::EndStats;
using game::FrameData;
using game::HudEvents;
using game::HudTracker;
using game::Input;
using game::LevelFlow;
using game::LevelFlowConfig;
using game::PauseMenu;
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

static void collect_egg(Sim& sim, const Scene& s, const char* name) {
  const auto* eg = find_name(s, name);
  place(sim, eg->x, eg->y);
  sim.step({});
}

static void wait_invuln(Sim& sim, float safe_x, float safe_y) {
  for (int i = 0; i < 80 && sim.invuln() > 0; ++i) {
    place(sim, safe_x, safe_y);
    sim.step({});
  }
}

static void defeat_all_bosses(Sim& sim, float safe_x, float safe_y) {
  for (int bi = 0; bi < sim.boss_count(); ++bi) {
    const int max_hits = sim.boss(bi).max_hits();
    for (int k = 0; k < max_hits && !sim.boss(bi).defeated; ++k) {
      for (int i = 0; i < 300 && (sim.boss(bi).hurt > 0.f || sim.boss(bi).phase == 2); ++i) {
        place(sim, safe_x, safe_y);
        sim.step({});
      }
      const auto& b = sim.boss(bi);
      auto& pl = sim.world().points()[sim.player_index()];
      pl.pos = {b.x, b.y - b.height() - Sim::kPlayerRadius + 1.5f};
      pl.prev = {pl.pos.x, pl.pos.y - 2.f};
      pl.force = {};
      sim.scene().follow(pl.pos.x, pl.pos.y);
      sim.step({});
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: rewards_test <world.ewmap>\n");
    return 2;
  }

  static_assert(Sim::kEggsForExtraHp == 2);
  static_assert(Sim::kEggsForDashUpgrade == 4);
  static_assert(Sim::kEggsForThirdHp == 6);
  static_assert(Sim::kDashUpgradedFrames == 14);
  static_assert(Sim::kDashUpgradedCooldown == 10);
  static_assert(Sim::kDashUpgradedCharges == 2);
  static_assert(Sim::kHitInvulnFrames == 60);

  Scene scene;
  std::string err;
  if (!scene.load(argv[1], err)) {
    std::fprintf(stderr, "load error: %s\n", err.c_str());
    return 1;
  }

  Sim sim(scene);
  CHECK(sim.collectible_total() == 8);
  CHECK(scene.tile(80, 19, ew::map::Layer::META) == Sim::kMetaHazard);

  PauseMenu menu;
  EndScreen end_screen;
  DialogueBox dialogue;
  HudTracker hud;
  FrameData fv;

  // --- 1) 0/6 huevos: max_hp == 1, !dash_upgraded, tocar pinchos mata ---
  CHECK(sim.collected() == 0 && sim.max_hp() == 1 && sim.hp() == 1);
  CHECK(!sim.dash_upgraded() && !sim.all_eggs_collected());
  {
    const HudEvents hev = game::populate_hud_and_overlays(fv, scene, sim, menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(!hev.new_extra_hp && !hev.new_dash_upgrade);
    CHECK(fv.hp == 1 && fv.max_hp == 1 && fv.dash_upgraded == 0 && fv.end_true == 0);
  }
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  (void)sim.take_sfx();
  sim.step({});
  CHECK(sim.deaths() == 1);
  CHECK((sim.take_sfx() & ew::audio::kSfxDie) != 0u);
  CHECK(sim.hp() == 1 && sim.max_hp() == 1);

  // --- 2) 2/6 huevos: +1 HP, evento HUD, sobrevivir 1er golpe (pinchos e imp), morir al 2do, curar en respawn/checkpoint ---
  wait_invuln(sim, 536.f, 56.f);
  collect_egg(sim, scene, "egg_well");
  CHECK(sim.collected() == 1 && sim.max_hp() == 1 && sim.hp() == 1);
  (void)sim.take_sfx();
  collect_egg(sim, scene, "egg_cat_1");
  CHECK(sim.collected() == 2 && sim.max_hp() == 2 && sim.hp() == 2);
  CHECK((sim.take_sfx() & ew::audio::kSfxAbility) != 0u);
  {
    const HudEvents hev = game::populate_hud_and_overlays(fv, scene, sim, menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(hev.new_extra_hp && !hev.new_dash_upgrade);
    CHECK(fv.hp == 2 && fv.max_hp == 2 && fv.dash_upgraded == 0);
    CHECK(fv.milestone_flash >= 0.99f && fv.dash_charges_max == 0);
  }

  // Primer golpe en pinchos: sobrevive con hp == 1, invuln == 60, deaths sigue en 1 y hay knockback.
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  (void)sim.take_sfx();
  sim.step({});
  CHECK(sim.hp() == 1 && sim.max_hp() == 2 && sim.deaths() == 1);
  CHECK(sim.invuln() == Sim::kHitInvulnFrames);
  CHECK((sim.take_sfx() & ew::audio::kSfxHit) != 0u);
  CHECK(sim.player().velocity().y < 0.f);
  {
    const HudEvents hev_hit = game::populate_hud_and_overlays(fv, scene, sim, menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(hev_hit.took_damage && fv.damage_flash >= 0.99f);
    const HudEvents hev_decay = game::populate_hud_and_overlays(fv, scene, sim, menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(!hev_decay.took_damage && fv.damage_flash < 0.99f && fv.damage_flash > 0.f);
  }

  // Tocar checkpoint actual ya activado cura silenciosamente a hp == 2 (sin re-emitir kSfxCheckpoint).
  const auto* ck_ledge = find_name(scene, "ck_ledge");
  CHECK(ck_ledge);
  place(sim, ck_ledge->x, ck_ledge->y);
  (void)sim.take_sfx();
  sim.step({});
  CHECK(sim.hp() == 2 && sim.checkpoint() == ck_ledge);
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  wait_invuln(sim, 536.f, 56.f);
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  sim.step({});
  CHECK(sim.hp() == 1);
  place(sim, ck_ledge->x, ck_ledge->y);
  (void)sim.take_sfx();
  sim.step({});
  CHECK(sim.hp() == 2);
  CHECK((sim.take_sfx() & ew::audio::kSfxCheckpoint) == 0u);

  // Segundo golpe sin curarse mata (deaths == 2) y respawn() restaura hp == 2.
  wait_invuln(sim, 600.f, 56.f);
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  sim.step({});
  CHECK(sim.hp() == 1 && sim.deaths() == 1 && sim.invuln() > 0);
  wait_invuln(sim, 600.f, 56.f);
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  sim.step({});
  CHECK(sim.deaths() == 2 && sim.hp() == 2 && sim.max_hp() == 2);

  // Golpe de imp también permite sobrevivir 1er golpe con hp == 2 -> hp == 1.
  wait_invuln(sim, 600.f, 56.f);
  CHECK(sim.imp_count() > 0 && !sim.imp(0).defeated);
  place(sim, sim.imp(0).x, sim.imp(0).y - Sim::kImpH * 0.5f);
  sim.step({});
  CHECK(sim.hp() == 1 && sim.deaths() == 2 && sim.invuln() > 0);

  // --- 3) 4/6 huevos: Dash II (2 cargas aéreas, 14f duración, 10f cooldown) + evento HUD ---
  sim.grant_ability(game::kAbilityDash);
  CHECK(!sim.dash_upgraded() && sim.max_dash_charges() == 1);
  collect_egg(sim, scene, "egg_cat_2");
  CHECK(sim.collected() == 3 && !sim.dash_upgraded());
  (void)sim.take_sfx();
  collect_egg(sim, scene, "egg_wyrm_1");
  CHECK(sim.collected() == 4 && sim.dash_upgraded());
  CHECK(sim.max_dash_charges() == 2 && sim.dash_charges() == 2);
  CHECK((sim.take_sfx() & ew::audio::kSfxAbility) != 0u);
  {
    const HudEvents hev = game::populate_hud_and_overlays(fv, scene, sim, menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(!hev.new_extra_hp && hev.new_dash_upgrade);
    CHECK(fv.dash_upgraded == 1 && fv.hp == 1 && fv.max_hp == 2);
    CHECK(fv.dash_charges == 2 && fv.dash_charges_max == 2 && fv.milestone_flash >= 0.99f);
  }
  sim.respawn();
  CHECK(sim.hp() == 2 && sim.max_hp() == 2 && sim.dash_charges() == 2);

  // En el aire durante un solo salto: 2 dashes seguidos de 14 frames cada uno, sin recargar en el aire.
  place(sim, 536.f, 56.f);
  for (int i = 0; i < 15; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.dash_charges() == 2);
  { Input j; j.jump = true; sim.step(j); }
  CHECK(!sim.player().grounded && sim.dash_charges() == 2);

  // Primer dash aéreo (14 frames de duración).
  { Input d; d.dash = true; d.move_x = 1.f; sim.step(d); }
  CHECK(sim.dash_ticks() == Sim::kDashUpgradedFrames);
  CHECK(sim.dash_charges() == 1);
  for (int i = 0; i < Sim::kDashUpgradedFrames; ++i) {
    CHECK(sim.dash_ticks() == Sim::kDashUpgradedFrames - i);
    sim.step({});
  }
  CHECK(sim.dash_ticks() == 0 && !sim.player().grounded && sim.dash_charges() == 1);

  // Segundo dash aéreo en el mismo salto (cooldown 10 <= 14 ya expiró).
  { Input d; d.dash = true; d.move_x = 1.f; sim.step(d); }
  CHECK(sim.dash_ticks() == Sim::kDashUpgradedFrames);
  CHECK(sim.dash_charges() == 0);
  for (int i = 0; i < Sim::kDashUpgradedFrames; ++i) sim.step({});
  CHECK(sim.dash_ticks() == 0 && !sim.player().grounded);

  // Tercer dash en el mismo salto NO dispara (sin cargas).
  { Input d; d.dash = true; d.move_x = 1.f; sim.step(d); }
  CHECK(sim.dash_ticks() == 0 && sim.dash_charges() == 0);

  // Al aterrizar recarga ambas cargas.
  for (int i = 0; i < 60 && !sim.player().grounded; ++i) sim.step({});
  CHECK(sim.player().grounded && sim.dash_charges() == 2);

  // --- 3b) 6/8 huevos: +2 HP (max_hp == 3, 3 corazones) + evento HUD new_extra_hp ---
  collect_egg(sim, scene, "egg_wyrm_2");
  CHECK(sim.collected() == 5 && sim.max_hp() == 2 && sim.hp() == 2);
  (void)sim.take_sfx();
  collect_egg(sim, scene, "egg_pozo_secreto");
  CHECK(sim.collected() == 6 && sim.max_hp() == 3 && sim.hp() == 3);
  CHECK((sim.take_sfx() & ew::audio::kSfxAbility) != 0u);
  {
    const HudEvents hev = game::populate_hud_and_overlays(fv, scene, sim, menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(hev.new_extra_hp && !hev.new_dash_upgrade);
    CHECK(fv.hp == 3 && fv.max_hp == 3 && fv.dash_upgraded == 1);
  }
  // Con 3 corazones sobrevive dos golpes seguidos y muere al tercero.
  wait_invuln(sim, 600.f, 56.f);
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  sim.step({});
  CHECK(sim.hp() == 2 && sim.max_hp() == 3 && sim.deaths() == 2);
  wait_invuln(sim, 600.f, 56.f);
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  sim.step({});
  CHECK(sim.hp() == 1 && sim.max_hp() == 3 && sim.deaths() == 2);
  wait_invuln(sim, 600.f, 56.f);
  place(sim, 80.f * 8.f + 4.f, 19.f * 8.f + 4.f);
  sim.step({});
  CHECK(sim.deaths() == 3 && sim.hp() == 3 && sim.max_hp() == 3);

  // --- 3c) Ventajas pasivas reales de los 3 héroes (Lantern, Opossum, Beaver) ---
  {
    // 1. Sr. del Farol: mayor radio de iluminación (150 vs 125) y mayor alcance del Farol Revelador (128 vs 96).
    sim.set_hero(game::HeroKind::Lantern);
    CHECK(sim.hero_lantern_radius() == Sim::kLanternLightRadius);
    CHECK(sim.hero_reveal_reach() == Sim::kLanternRevealReach);
    CHECK(sim.hero_walk_speed() == Sim::kWalkSpeed);
    CHECK(sim.hero_coyote_frames() == Sim::kCoyoteFrames);
    CHECK(sim.hero_water_jump_factor() == Sim::kBaseWaterJumpFactor);
    CHECK(sim.hero_wall_slide_speed() == Sim::kWallSlideSpeed);
    FrameData fv_l;
    game::build_world_frame(fv_l, scene, sim, 0.f, false, false);
    CHECK(fv_l.lantern_radius == Sim::kLanternLightRadius);

    // 2. La Zarigüeya: paso más veloz (98 vs 90) y coyote time extendido (8 vs 6).
    sim.set_hero(game::HeroKind::Opossum);
    CHECK(sim.hero_walk_speed() == Sim::kOpossumWalkSpeed);
    CHECK(sim.hero_coyote_frames() == Sim::kOpossumCoyoteFrames);
    CHECK(sim.hero_lantern_radius() == Sim::kBaseLightRadius);
    CHECK(sim.hero_reveal_reach() == Sim::kRevealReach);
    FrameData fv_o;
    game::build_world_frame(fv_o, scene, sim, 0.f, false, false);
    CHECK(fv_o.lantern_radius == Sim::kBaseLightRadius);
    place(sim, 536.f, 56.f);
    for (int i = 0; i < 10; ++i) sim.step({});
    CHECK(sim.player().grounded && sim.coyote_ticks() == Sim::kOpossumCoyoteFrames);

    // 3. El Castor: impulso de salto acuático más potente (0.94 vs 0.80) y agarre más firme en pared (26 vs 36).
    sim.set_hero(game::HeroKind::Beaver);
    CHECK(sim.hero_water_jump_factor() == Sim::kBeaverWaterJumpFactor);
    CHECK(sim.hero_wall_slide_speed() == Sim::kBeaverWallSlideSpeed);
    CHECK(sim.hero_walk_speed() == Sim::kWalkSpeed);
    sim.set_hero(game::HeroKind::Lantern);
  }

  // --- 4) EndStats::true_ending y LevelFlow::post_step (8/8 vs 7/8) ---
  // 4a) 8/8 en world -> true_ending = true ("FINAL VERDADERO", max_hp == 3, "+2HP")
  {
    LevelFlow flow;
    LevelFlowConfig cfg;
    cfg.map_path = argv[1];
    CHECK(flow.init(cfg, err));
    PauseMenu m;
    EndScreen es;
    DialogueBox dlg;
    HudTracker ht;
    flow.sim().grant_ability(game::kAbilityAll);
    for (const char* egg_name : {"egg_well", "egg_cat_1", "egg_cat_2", "egg_wyrm_1", "egg_wyrm_2", "egg_pozo_secreto", "egg_archivo", "egg_torre"}) {
      collect_egg(flow.sim(), flow.scene(), egg_name);
    }
    CHECK(flow.sim().all_eggs_collected());
    defeat_all_bosses(flow.sim(), 296.f, 280.f);
    CHECK(flow.sim().bosses_defeated());
    const auto* goal = find_name(flow.scene(), "goal");
    CHECK(goal);
    place(flow.sim(), goal->x, goal->y);
    flow.sim().step({});
    CHECK(flow.sim().finished());
    const auto post = flow.post_step(m, es, dlg, false);
    CHECK(post.opened_end && es.open());
    CHECK(es.stats().campaign_done && es.stats().true_ending);
    CHECK(es.stats().max_hp == 3 && es.stats().dash_upgraded);
    const auto txt = game::end_screen_text(es);
    CHECK(txt[0] == game::kEndTitleTrue);
    CHECK(txt[1] == game::kEndSubtitleTrue);
    CHECK(txt[4].size() <= std::size_t(game::kTxtCols));
    CHECK(txt[4].find("DASH II") != std::string::npos);
    CHECK(txt[4].find("+2HP") != std::string::npos);
    FrameData fv_end;
    (void)game::populate_hud_and_overlays(fv_end, flow.scene(), flow.sim(), m, es, dlg, ht, "0.1.0");
    CHECK(fv_end.end_true == 1);
  }

  // 4b) 7/8 en world -> campaign_done = true pero true_ending = false (kEndTitleVictory)
  {
    LevelFlow flow;
    LevelFlowConfig cfg;
    cfg.map_path = argv[1];
    CHECK(flow.init(cfg, err));
    PauseMenu m;
    EndScreen es;
    DialogueBox dlg;
    for (const char* egg_name : {"egg_well", "egg_cat_1", "egg_cat_2", "egg_wyrm_1", "egg_wyrm_2", "egg_pozo_secreto", "egg_archivo"}) {
      collect_egg(flow.sim(), flow.scene(), egg_name);
    }
    CHECK(flow.sim().collected() == 7 && !flow.sim().all_eggs_collected());
    defeat_all_bosses(flow.sim(), 296.f, 280.f);
    const auto* goal = find_name(flow.scene(), "goal");
    CHECK(goal);
    place(flow.sim(), goal->x, goal->y);
    flow.sim().step({});
    CHECK(flow.sim().finished());
    const auto post = flow.post_step(m, es, dlg, false);
    CHECK(post.opened_end && es.open());
    CHECK(es.stats().campaign_done && !es.stats().true_ending);
    const auto txt = game::end_screen_text(es);
    CHECK(txt[0] == game::kEndTitleVictory);
    CHECK(txt[1] == game::level_title("world"));
    FrameData fv_end;
    HudTracker ht;
    (void)game::populate_hud_and_overlays(fv_end, flow.scene(), flow.sim(), m, es, dlg, ht, "0.1.0");
    CHECK(fv_end.end_true == 0);
  }

  // 4c) Sin habilidades ni mejoras -> "HABILIDADES: NINGUNA"
  {
    EndScreen es;
    EndStats st;
    st.abilities = 0;
    st.dash_upgraded = false;
    st.max_hp = 1;
    es.open(st);
    const auto txt = game::end_screen_text(es);
    CHECK(txt[4] == "HABILIDADES: NINGUNA");
  }

  std::puts("rewards_test: OK");
  return 0;
}
