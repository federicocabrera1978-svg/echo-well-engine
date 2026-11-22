// Test sin GL (Etapa 9 Fase B): mundo Metroidvania unificado 8x3 (`world.ewmap`).
// Verifica estructura 8x3 (24 chunks), 5 huevos, 3 bosses, contención de agua,
// atajos unidireccionales (owd_wyrm_cat_l, owd_wyrm_pozo), ascensores
// (elev_cat_pozo, elev_wyrm_pozo), caída tras Boss 1, derrota del Wyrm -> goal
// y persistencia en save.ewsv.
#include "audio/sfx.hpp"
#include "game/campaign.hpp"
#include "game/end_screen.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"

#include <algorithm>
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

static int find_mover_idx(const Sim& sim, const Scene& s, const char* name) {
  for (int i = 0; i < sim.mover_count(); ++i) {
    const std::size_t ei = sim.mover(i).ent;
    if (ei < s.entities().size() && s.entities()[ei].name == name) return i;
  }
  return -1;
}

static void defeat_boss_at(Sim& sim, int boss_idx, float safe_x, float safe_y) {
  const int max_hits = sim.boss(boss_idx).max_hits();
  for (int k = 0; k < max_hits && !sim.boss(boss_idx).defeated; ++k) {
    for (int i = 0; i < 300 && (sim.boss(boss_idx).hurt > 0.f || sim.boss(boss_idx).phase == 2); ++i) {
      place(sim, safe_x, safe_y);
      sim.step({});
    }
    const auto& b = sim.boss(boss_idx);
    place(sim, b.x, b.y - b.height() - Sim::kPlayerRadius + 1.5f, 2.f);
    sim.step({});
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: world_test <world.ewmap>\n");
    return 2;
  }

  Scene scene;
  std::string err;
  if (!scene.load(argv[1], err)) {
    std::fprintf(stderr, "load error: %s\n", err.c_str());
    return 1;
  }

  // --- 1) Invariantes estructurales 8x3 (24 chunks), 5 huevos, 3 bosses, contención de agua ---
  CHECK(scene.width_px() == 2560 && scene.height_px() == 552);
  CHECK(scene.map().header().map_w_chunks == 8 && scene.map().header().map_h_chunks == 3);
  CHECK(scene.map().chunk_count() == 24);
  CHECK(scene.map().backdrops().size() == 24);

  Sim sim(scene);
  CHECK(sim.collectible_total() == 5 && sim.collected() == 0);
  CHECK(sim.boss_count() == 3 && !sim.bosses_defeated());
  CHECK(sim.rope_count() == 4 && sim.water().size() == 6);
  CHECK(sim.mover_count() == 7);

  // Contención de las 6 piletas (misma invariante que water_test).
  for (const auto& b : sim.water()) {
    const int l = int(b.x()) / 8 - 1;
    const int r = int(b.x() + b.width()) / 8;
    for (int ty = int(b.rest_y()) / 8; ty < int(b.bottom()) / 8; ++ty)
      CHECK(scene.solid_tile(l, ty) && scene.solid_tile(r, ty));
    for (int tx = l + 1; tx < r; ++tx)
      CHECK(scene.solid_tile(tx, int(b.bottom()) / 8));
  }

  // Anti-softlock en spawn y todos los checkpoints de world.ewmap.
  for (const auto& e : scene.entities()) {
    if (e.type != "player_spawn" && e.type != "checkpoint") continue;
    Scene s_chk;
    CHECK(s_chk.load(argv[1], err));
    Sim sim_chk(s_chk);
    place(sim_chk, e.x, e.y);
    const int cx = int(e.x) / (ew::map::kChunkW * ew::map::kTileSize);
    const int cy = int(e.y) / (ew::map::kChunkH * ew::map::kTileSize);
    CHECK(s_chk.chunk_visited(cx, cy));
    for (int f = 0; f < 5; ++f) sim_chk.step({});
    CHECK(sim_chk.deaths() == 0);
  }

  const auto* boss1 = find_name(scene, "boss_pozo");
  const auto* door_b1 = find_name(scene, "door_boss1");
  const auto* ck_cat = find_name(scene, "ck_cat_drop");
  const auto* sw_elev_cat = find_name(scene, "sw_elev_cat");
  const auto* elev_cat = find_name(scene, "elev_cat_pozo");
  const auto* owd_cat = find_name(scene, "owd_wyrm_cat_l");
  const auto* boss2 = find_name(scene, "boss_catacomb");
  const auto* door_b2 = find_name(scene, "door_boss2");
  const auto* elev_wyrm = find_name(scene, "elev_wyrm_pozo");
  const auto* owd_pozo = find_name(scene, "owd_wyrm_pozo");
  const auto* wyrm = find_name(scene, "wyrm");
  const auto* door_exit = find_name(scene, "door_wyrm_exit");
  const auto* goal = find_name(scene, "goal");
  CHECK(boss1 && door_b1 && ck_cat && sw_elev_cat && elev_cat && owd_cat);
  CHECK(boss2 && door_b2 && elev_wyrm && owd_pozo && wyrm && door_exit && goal);

  // --- 2) flood_switch en (2,0) solo inunda la cisterna de (3,0), no piletas de Zona 2/3 ---
  {
    const float y_cat_drop0 = sim.water()[2].rest_y();
    const float y_pit2_0 = sim.water()[3].rest_y();
    const auto* flood = find_name(scene, "flood");
    CHECK(flood);
    place(sim, flood->x, flood->y);
    { Input act; act.interact = true; sim.step(act); }
    CHECK(flood->active);
    for (int i = 0; i < 120; ++i) sim.step({});
    CHECK(sim.water()[1].rest_y() < 100.f);  // cisterna de (3,0) sí sube
    CHECK(std::fabs(sim.water()[2].rest_y() - y_cat_drop0) < 1e-4f);
    CHECK(std::fabs(sim.water()[3].rest_y() - y_pit2_0) < 1e-4f);
    sim.reset();
  }

  // --- 3) Boss 1 en (7,0) -> door_boss1 -> caída segura al agua de (7,1) y ck_cat_drop ---
  place(sim, 2264.f, 152.f);
  sim.step({});
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_boss");
  defeat_boss_at(sim, 0, 2264.f, 152.f);
  CHECK(sim.boss(0).defeated && door_b1->active);
  CHECK(!sim.bosses_defeated());  // aún viven Boss 2 y Wyrm

  bool wet_after_drop = false;
  for (int i = 0; i < 240 && sim.player().pos.y < 320.f; ++i) {
    sim.step({1.f});
    wet_after_drop = wet_after_drop || sim.player_in_water();
  }
  for (int i = 0; i < 30; ++i) {
    sim.step({});
    wet_after_drop = wet_after_drop || sim.player_in_water();
  }
  CHECK(wet_after_drop);
  CHECK(scene.chunk_x() == 7 && scene.chunk_y() == 1);
  CHECK(sim.deaths() == 0);
  // Salir de la pileta hacia la izquierda hasta activar ck_cat_drop.
  for (int i = 0; i < 200 && !(sim.checkpoint() && sim.checkpoint()->name == "ck_cat_drop"); ++i) {
    Input in;
    in.move_x = -1.f;
    if (sim.player_in_water() || (sim.player().grounded && sim.player().pos.x > 2468.f)) in.jump = true;
    sim.step(in);
  }
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_cat_drop");
  CHECK(sim.deaths() == 0);

  // --- 4) elev_cat_pozo: bloqueado hasta sw_elev_cat, luego sube de (6,1) a (6,0) ---
  const int elev_cat_idx = find_mover_idx(sim, scene, "elev_cat_pozo");
  const int elev_wyrm_idx = find_mover_idx(sim, scene, "elev_wyrm_pozo");
  CHECK(elev_cat_idx >= 0 && elev_wyrm_idx >= 0);
  CHECK(!elev_cat->active);
  CHECK(std::fabs(sim.mover(elev_cat_idx).y - 160.f) < 1e-4f);

  // En (6,0) antes de desbloquearlo, sella el piso y no cae a Zona 2.
  place(sim, 1924.f, 156.f);
  for (int i = 0; i < 35; ++i) sim.step({1.f});
  CHECK(scene.chunk_x() == 6 && scene.chunk_y() == 0);
  CHECK(sim.player().pos.y < 162.f && !sim.gravity_flipped());

  // En (6,1), intentar llamarlo sin activar sw_elev_cat no lo mueve.
  place(sim, 1956.f, 336.f);
  sim.step({});
  { Input act; act.interact = true; sim.step(act); }  // 1956 está cerca de sw_elev_cat (1964) -> espera, probemos lejos del switch
  // Probemos explícitamente desde x=1936 (distancia 28 a sw_elev_cat=1964 > kSwitchReach=14).
  sim.reset();
  place(sim, 1936.f, 340.f);
  sim.step({});
  { Input act; act.interact = true; sim.step(act); }
  CHECK(!elev_cat->active);
  CHECK(std::fabs(sim.mover(elev_cat_idx).y - 160.f) < 1e-4f);

  // Activar sw_elev_cat (1964, 336), luego llamar elev_cat_pozo y subir a (6,0).
  place(sim, 1964.f, 336.f);
  sim.step({});
  { Input act; act.interact = true; sim.step(act); }
  CHECK(sw_elev_cat->active && elev_cat->active);
  place(sim, 1940.f, 340.f);
  { Input act; act.interact = true; sim.step(act); }  // llama al ascensor bajo el eje (x=1940)
  bool elev_cat_down = false;
  for (int i = 0; i < 240; ++i) {
    sim.step({});
    if (std::fabs(sim.mover(elev_cat_idx).y - 328.f) < 0.05f) { elev_cat_down = true; break; }
  }
  CHECK(elev_cat_down);

  place(sim, 1940.f, 324.f);
  sim.step({});
  CHECK(sim.ridden() == elev_cat_idx);
  bool elev_cat_up = false;
  for (int i = 0; i < 260; ++i) {
    sim.step({});
    if (std::fabs(sim.mover(elev_cat_idx).y - 160.f) < 0.05f) { elev_cat_up = true; break; }
  }
  CHECK(elev_cat_up);
  CHECK(sim.player().pos.y < 162.f && scene.chunk_x() == 6 && scene.chunk_y() == 0);
  CHECK(!sim.gravity_flipped() && sim.deaths() == 0);

  // --- 5) one_way_door owd_wyrm_cat_l: bloqueada desde Zona 2 (derecha), abre desde Zona 3 (izquierda) ---
  CHECK(!owd_cat->active && scene.door_blocks(160, 63));
  place(sim, 1296.f, 524.f);
  for (int i = 0; i < 30; ++i) sim.step({-1.f});
  CHECK(sim.player().pos.x >= 1288.f - Sim::kPlayerRadius - 0.6f);
  CHECK(!owd_cat->active);
  (void)sim.take_sfx();
  { Input act; act.interact = true; sim.step(act); }
  CHECK((sim.take_sfx() & ew::audio::kSfxHit) != 0u);
  CHECK(!owd_cat->active);

  place(sim, 1268.f, 524.f);
  (void)sim.take_sfx();
  bool cat_opened = false;
  for (int i = 0; i < 40; ++i) {
    sim.step({1.f});
    if (sim.take_sfx() & ew::audio::kSfxSwitch) cat_opened = true;
  }
  CHECK(cat_opened && owd_cat->active);
  CHECK(sim.player().pos.x > 1292.f);

  // --- 6) elev_wyrm_pozo + owd_wyrm_pozo en (1,1) <-> (1,0) ---
  CHECK(elev_wyrm->active);  // sin link -> arranca desbloqueado en (480, 328)
  CHECK(!owd_pozo->active && scene.door_blocks(51, 17));
  // Bloqueada desde (1,0) lado izquierdo.
  place(sim, 396.f, 156.f);
  for (int i = 0; i < 30; ++i) sim.step({1.f});
  CHECK(sim.player().pos.x <= 408.f - Sim::kPlayerRadius + 0.6f);
  CHECK(!owd_pozo->active);

  // Subir en elev_wyrm_pozo desde (1,1) hasta la alcoba de (1,0) y abrir owd_wyrm_pozo desde la derecha.
  place(sim, 492.f, 324.f);
  sim.step({});
  CHECK(sim.ridden() == elev_wyrm_idx);
  bool elev_wyrm_up = false;
  for (int i = 0; i < 260; ++i) {
    sim.step({});
    if (std::fabs(sim.mover(elev_wyrm_idx).y - 160.f) < 0.05f) { elev_wyrm_up = true; break; }
  }
  CHECK(elev_wyrm_up);
  CHECK(scene.chunk_x() == 1 && scene.chunk_y() == 0);
  CHECK(sim.player().pos.y < 162.f);

  bool pozo_opened = false;
  for (int i = 0; i < 160 && sim.player().pos.x > 392.f; ++i) {
    sim.step({-1.f});
    if (owd_pozo->active) pozo_opened = true;
  }
  CHECK(pozo_opened && owd_pozo->active && !scene.door_blocks(51, 17));
  CHECK(sim.player().pos.x < 404.f && sim.deaths() == 0);

  // --- 7) Derrotar Boss 1, Boss 2 y Wyrm -> door_wyrm_exit -> goal -> EndScreen + save.ewsv ---
  sim.reset();
  sim.grant_ability(game::kAbilityDoubleJump | game::kAbilityDash);

  // Recoger los 5 huevos y activar checkpoints/puertas clave.
  for (const char* egg_name : {"egg_well", "egg_cat_1", "egg_cat_2", "egg_wyrm_1", "egg_wyrm_2"}) {
    const auto* eg = find_name(scene, egg_name);
    CHECK(eg);
    place(sim, eg->x, eg->y);
    sim.step({});
    CHECK(eg->active);
  }
  CHECK(sim.collected() == 5);

  // Derrotar Boss 1 y Boss 2.
  defeat_boss_at(sim, 0, 2264.f, 152.f);
  CHECK(sim.boss(0).defeated && door_b1->active);
  defeat_boss_at(sim, 1, 1560.f, 336.f);
  CHECK(sim.boss(1).defeated && door_b2->active);
  CHECK(!sim.bosses_defeated());

  // Tocar goal antes de vencer al Wyrm no completa el mapa.
  place(sim, goal->x, goal->y);
  sim.step({});
  CHECK(!sim.finished());

  // Activar ck_wyrm_arena y vencer al Wyrm (4 pisotones).
  place(sim, 300.f, 336.f);
  sim.step({});
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_wyrm_arena");
  defeat_boss_at(sim, 2, 296.f, 280.f);
  CHECK(sim.boss(2).defeated && sim.bosses_defeated() && door_exit->active);

  // Activar atajos y ascensor antes de cruzar a goal para validar save.ewsv completo.
  place(sim, 1964.f, 336.f);
  sim.step({});
  { Input act; act.interact = true; sim.step(act); }
  place(sim, 1268.f, 524.f);
  for (int i = 0; i < 30; ++i) sim.step({1.f});
  place(sim, 424.f, 156.f);
  for (int i = 0; i < 30; ++i) sim.step({-1.f});
  CHECK(sw_elev_cat->active && elev_cat->active && owd_cat->active && owd_pozo->active);

  place(sim, 72.f, 336.f);
  for (int i = 0; i < 120 && !sim.finished(); ++i) sim.step({-1.f});
  CHECK(sim.finished());
  CHECK(sim.deaths() == 0);

  // Campaña y EndScreen para "world".
  CHECK(game::find_campaign_level("world") != nullptr);
  CHECK(game::next_campaign_map("world").empty());
  CHECK(game::level_title("world") == "EL REINO DEL POZO");
  {
    game::EndStats st;
    st.title = game::level_title("world");
    st.items = sim.collected();
    st.items_total = sim.collectible_total();
    st.deaths = sim.deaths();
    st.abilities = sim.abilities();
    st.has_next = !game::next_campaign_map("world").empty();
    st.campaign_done = !st.has_next;
    game::EndScreen es;
    es.open(st);
    const auto txt = game::end_screen_text(es);
    CHECK(txt[0] == "VICTORIA: REINO DESPIERTO");
    CHECK(txt[2] == "HUEVOS:  5/5");
    CHECK(txt[5].find("RANGO: S") != std::string::npos);
  }

  // Persistencia save.ewsv sobre world.ewmap (incluyendo atajos y ascensor).
  const auto bytes = game::encode_save(game::capture_save(sim));
  game::SaveData decoded;
  CHECK(game::decode_save(bytes.data(), bytes.size(), decoded) == game::SaveError::None);
  CHECK(decoded.map == "world");

  Scene s2;
  CHECK(s2.load(argv[1], err));
  Sim sim2(s2);
  CHECK(game::apply_save(sim2, decoded) == game::SaveError::None);
  CHECK(sim2.collected() == 5);
  CHECK(sim2.bosses_defeated());
  CHECK(sim2.has_ability(game::kAbilityDoubleJump | game::kAbilityDash));
  CHECK(find_name(s2, "elev_cat_pozo")->active);
  CHECK(find_name(s2, "owd_wyrm_cat_l")->active && !s2.door_blocks(160, 63));
  CHECK(find_name(s2, "owd_wyrm_pozo")->active && !s2.door_blocks(51, 17));

  std::puts("world_test: OK");
  return 0;
}
