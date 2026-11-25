// Test integral E2E sin GL (Etapa 18): verifica en world.ewmap (8x5) el flujo completo de
// héroes, Tres Sellos, huevos por zona (8/8), Viaje Rápido (kSfxWarp), avisos [E], Diario (J),
// Mapa Expandido (M), cronómetro speedrun, save.ewsv v5 (visited_chunks + elapsed_sec + hard_mode),
// Epílogo del Nido Primordial y EndScreen (FINAL VERDADERO + MAPA: 100 + DESAFIO).
#include "audio/sfx.hpp"
#include "game/end_screen.hpp"
#include "game/frame_view.hpp"
#include "game/hud.hpp"
#include "game/minimap.hpp"
#include "game/save.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#define CHECK(c)                                                              \
  do {                                                                        \
    if (!(c)) {                                                               \
      std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__);            \
      return 1;                                                               \
    }                                                                         \
  } while (0)

using namespace ew::game;

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string err;
  Scene scene;
  CHECK(scene.load(argv[1], err));
  Sim sim(scene);

  // 1) Estructura base de world.ewmap (8x5 = 40 salas, 8 huevos, 4 bosses, 3 Sellos, 5 Santuarios).
  const auto mm0 = compute_minimap(scene);
  CHECK(mm0.map_w == 8 && mm0.map_h == 5 && mm0.total_chunks == 40);
  CHECK(mm0.seals_total == 3 && mm0.seals_mask == 0u);
  CHECK(sim.collectible_total() == 8 && sim.boss_count() == 4);
  CHECK(list_fast_travel_nodes(scene).size() == 5u);

  // 2) Ventajas pasivas de los 3 héroes.
  sim.set_hero(HeroKind::Lantern);
  CHECK(sim.hero_lantern_radius() > 140.f && sim.hero_reveal_reach() > 120.f);
  sim.set_hero(HeroKind::Opossum);
  CHECK(sim.hero_walk_speed() > 95.f && sim.hero_coyote_frames() == 8);
  sim.set_hero(HeroKind::Beaver);
  CHECK(sim.hero_water_jump_factor() > 0.90f && sim.hero_wall_slide_speed() < 30.f);

  // 3) Recorrer las 40 salas desbloquea 100% de exploración y los 5 Santuarios de Viaje Rápido.
  for (int cy = 0; cy < 5; ++cy) {
    for (int cx = 0; cx < 8; ++cx) {
      scene.follow(float(cx) * 320.f + 160.f, float(cy) * 184.f + 92.f);
    }
  }
  const auto mm_full = compute_minimap(scene);
  CHECK(mm_full.explored_pct == 100);
  const auto nodes = list_fast_travel_nodes(scene);
  for (const auto& nd : nodes) CHECK(nd.unlocked);

  // 4) Viaje Rápido entre Santuarios emite kSfxWarp y centra cámara en destino.
  (void)sim.take_sfx();
  CHECK(sim.teleport_to_sanctuary(nodes[2].x, nodes[2].y, nodes[2].ent_idx));
  CHECK((sim.take_sfx() & ew::audio::kSfxWarp) != 0u);
  CHECK(scene.chunk_x() == nodes[2].cx && scene.chunk_y() == nodes[2].cy);

  // 5) Recolectar los 8 huevos (2/2/2/1/1), abrir los 3 Sellos y otorgar las 4 reliquias.
  sim.grant_ability(kAbilityAll);
  for (std::size_t i = 0; i < scene.entities().size(); ++i) {
    const auto& e = scene.entities()[i];
    if (e.type == "collectible" || e.name == "door_seal_pozo" || e.name == "door_seal_cat" ||
        e.name == "door_seal_archivo" || e.type == "boss" || e.type == "boss_dragon" || e.type == "boss_sentinel") {
      scene.set_active(i, true);
    }
  }
  CHECK(sim.collected() == 8 && sim.all_eggs_collected() && sim.max_hp() == 3 && sim.dash_upgraded());
  const auto zones = compute_zone_eggs(scene);
  CHECK(zones.size() == 5u);
  int sum_found = 0, sum_total = 0;
  for (const auto& z : zones) {
    CHECK(z.found == z.total);
    sum_found += z.found;
    sum_total += z.total;
  }
  CHECK(sum_found == 8 && sum_total == 8);
  CHECK(compute_minimap(scene).seals_mask == 7u);

  // 6) Modo Desafío (--hard) + save.ewsv v5 round-trip (preserva visited_chunks, elapsed_sec y hard_mode).
  sim.set_hard_mode(true);
  sim.set_elapsed_sec(345.f);
  CHECK(sim.max_hp() == 1 && sim.hp() == 1);
  const SaveData saved = capture_save(sim);
  const auto bytes = encode_save(saved);
  SaveData decoded;
  CHECK(decode_save(bytes.data(), bytes.size(), decoded) == SaveError::None);
  CHECK(decoded.hard_mode && decoded.visited_chunks == scene.visited_chunks());
  CHECK(std::fabs(decoded.elapsed_sec - 345.f) < 1e-3f);

  Scene scene2;
  CHECK(scene2.load(argv[1], err));
  Sim sim2(scene2);
  CHECK(apply_save(sim2, decoded) == SaveError::None);
  CHECK(sim2.hard_mode() && sim2.max_hp() == 1 && sim2.hero() == HeroKind::Beaver);
  CHECK(compute_minimap(scene2).explored_pct == 100);
  CHECK(std::fabs(sim2.elapsed_sec() - 345.f) < 1e-3f);

  // 7) HUD: Diario (map_open==2), Mapa Expandido (map_open==1), cronómetro y EndScreen FINAL VERDADERO + DESAFIO.
  PauseMenu menu;
  EndScreen end_screen;
  DialogueBox dialogue;
  HudTracker hud;
  FrameData fv;
  populate_hud_and_overlays(fv, scene2, sim2, menu, end_screen, dialogue, hud, "0.1.0", false, -1, true, true);
  CHECK(fv.map_open == 2 && fv.hard_mode == 1 && fv.show_timer == 1 && fv.timer_secs == 345);
  populate_hud_and_overlays(fv, scene2, sim2, menu, end_screen, dialogue, hud, "0.1.0", true, 3, true, false);
  CHECK(fv.map_open == 1 && fv.seals_mask == 7 && fv.seals_total == 3);

  EndStats st;
  st.title = "EL CALABOZO DEL WYRM";
  st.items = 8;
  st.items_total = 8;
  st.deaths = 0;
  st.abilities = kAbilityAll;
  st.elapsed_sec = 345.f;
  st.campaign_done = true;
  st.true_ending = true;
  st.max_hp = 1;
  st.dash_upgraded = true;
  st.explored_pct = 100;
  st.hard_mode = true;
  end_screen.open(st);
  const auto lines = end_screen_text(end_screen);
  CHECK(lines[0] == std::string(kEndTitleTrue));
  CHECK(lines[2] == "HUEVOS: 8/8   MAPA: 100");
  CHECK(lines[3] == "MUERTES: 0   DESAFIO");
  CHECK(lines[4].find("FAROL") != std::string::npos);

  std::puts("campaign_e2e_test OK");
  return 0;
}
