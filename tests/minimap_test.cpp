// Test sin GL (Etapa 8): estado derivado del mini-mapa Metroidvania (minimap.hpp)
// y volcado de máscaras + porcentaje explorado en FrameData vía populate_hud_and_overlays.
#include "audio/sfx.hpp"
#include "game/hud.hpp"
#include "game/minimap.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#define CHECK(c)                                                              \
  do {                                                                        \
    if (!(c)) {                                                               \
      std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__);            \
      return 1;                                                               \
    }                                                                         \
  } while (0)

using namespace ew;
using game::DialogueBox;
using game::EndScreen;
using game::FrameData;
using game::HudTracker;
using game::MinimapState;
using game::PauseMenu;
using game::Scene;
using game::Sim;

static constexpr uint64_t bit_at(int cx, int cy, int map_w) {
  return uint64_t(1) << unsigned(cy * map_w + cx);
}

static int find_entity_index(const Scene& sc, std::string_view name) {
  const auto& ents = sc.entities();
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].name == name) return int(i);
  }
  return -1;
}

int main(int argc, char** argv) {
  if (argc < 4) return 2;
  std::string err;

  // --- 1) slice.ewmap: estado inicial, puertas/huevos, limpieza y 100% exploración ---
  {
    Scene sc;
    CHECK(sc.load(argv[1], err));

    const MinimapState m0 = game::compute_minimap(sc);
    CHECK(m0.map_w == 8 && m0.map_h == 1);
    CHECK(m0.total_chunks == 8);
    CHECK(m0.cur_cx == 0 && m0.cur_cy == 0);
    CHECK(m0.visited_mask == bit_at(0, 0, 8));
    for (int b = 1; b < 8; ++b) CHECK(((m0.visited_mask >> unsigned(b)) & 1ull) == 0);
    CHECK(m0.visited_count == 1);
    CHECK(m0.explored_pct == 12);
    CHECK((m0.blocked_doors_mask & bit_at(0, 0, 8)) != 0);
    CHECK((m0.missing_eggs_mask & bit_at(4, 0, 8)) != 0);
    CHECK(m0.missing_eggs_mask == bit_at(4, 0, 8));

    // Abrir door_start limpia el bit de chunk (0,0) en blocked_doors_mask.
    const int door_start_idx = find_entity_index(sc, "door_start");
    CHECK(door_start_idx >= 0);
    sc.set_active(std::size_t(door_start_idx), true);
    const MinimapState m_open = game::compute_minimap(sc);
    CHECK((m_open.blocked_doors_mask & bit_at(0, 0, 8)) == 0);

    // Recoger egg_well limpia el bit de chunk (4,0) en missing_eggs_mask.
    const int egg_well_idx = find_entity_index(sc, "egg_well");
    CHECK(egg_well_idx >= 0);
    sc.set_active(std::size_t(egg_well_idx), true);
    const MinimapState m_egg = game::compute_minimap(sc);
    CHECK((m_egg.missing_eggs_mask & bit_at(4, 0, 8)) == 0);
    CHECK(m_egg.missing_eggs_mask == 0);

    // Visitar los 8 chunks da visited_count == 8 y explored_pct == 100.
    for (int cx = 0; cx < 8; ++cx) sc.follow(float(cx) * 320.f + 160.f, 152.f);
    const MinimapState m_all = game::compute_minimap(sc);
    CHECK(m_all.visited_count == 8);
    CHECK(m_all.explored_pct == 100);
    CHECK(m_all.visited_mask == uint64_t(0xFF));
  }

  // --- 2) level2.ewmap: grilla 4x2, huevos en (0,0) y (3,1), wall_guardian en (2,0), door_boss2 en (3,0) ---
  {
    Scene sc2;
    CHECK(sc2.load(argv[2], err));

    const MinimapState m2 = game::compute_minimap(sc2);
    CHECK(m2.map_w == 4 && m2.map_h == 2);
    CHECK(m2.total_chunks == 8);
    CHECK(m2.cur_cx == 0 && m2.cur_cy == 0);
    CHECK(m2.visited_count == 1 && m2.explored_pct == 12);
    CHECK((m2.missing_eggs_mask & bit_at(0, 0, 4)) != 0);
    CHECK((m2.missing_eggs_mask & bit_at(3, 1, 4)) != 0);
    CHECK(m2.missing_eggs_mask == (bit_at(0, 0, 4) | bit_at(3, 1, 4)));
    CHECK((m2.blocked_doors_mask & bit_at(2, 0, 4)) != 0);
    CHECK((m2.blocked_doors_mask & bit_at(3, 0, 4)) != 0);

    // Destruir wall_guardian libera chunk (2,0) de blocked_doors_mask.
    const int wall_idx = find_entity_index(sc2, "wall_guardian");
    CHECK(wall_idx >= 0);
    sc2.set_active(std::size_t(wall_idx), true);
    const MinimapState m2_after = game::compute_minimap(sc2);
    CHECK((m2_after.blocked_doors_mask & bit_at(2, 0, 4)) == 0);
    CHECK((m2_after.blocked_doors_mask & bit_at(3, 0, 4)) != 0);
  }

  // --- 3) level3.ewmap: grilla 3x2 y populate_hud_and_overlays copia todas las máscaras ---
  {
    Scene sc3;
    CHECK(sc3.load(argv[3], err));
    Sim sim(sc3);
    PauseMenu menu;
    EndScreen end_screen;
    DialogueBox dialogue;
    HudTracker hud;
    FrameData v;

    sc3.follow(400.f, 250.f);  // visita también chunk (1,1)
    const MinimapState mm = game::compute_minimap(sc3);
    CHECK(mm.map_w == 3 && mm.map_h == 2 && mm.total_chunks == 6);
    CHECK(mm.cur_cx == 1 && mm.cur_cy == 1);
    CHECK(mm.visited_count == 2 && mm.explored_pct == 33);
    CHECK((mm.blocked_doors_mask & bit_at(0, 0, 3)) != 0);
    CHECK((mm.blocked_doors_mask & bit_at(1, 0, 3)) != 0);
    CHECK((mm.blocked_doors_mask & bit_at(2, 0, 3)) != 0);
    CHECK((mm.blocked_doors_mask & bit_at(0, 1, 3)) != 0);
    CHECK((mm.missing_eggs_mask & bit_at(1, 0, 3)) != 0);
    CHECK((mm.missing_eggs_mask & bit_at(0, 1, 3)) != 0);

    game::populate_hud_and_overlays(v, sc3, sim, menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(v.map_w == mm.map_w && v.map_h == mm.map_h);
    CHECK(v.cur_cx == mm.cur_cx && v.cur_cy == mm.cur_cy);
    CHECK(v.visited_mask == uint32_t(mm.visited_mask & 0xFFFFFFFFu));
    CHECK(v.visited_mask_hi == uint32_t((mm.visited_mask >> 32) & 0xFFFFFFFFu));
    CHECK(v.blocked_doors_lo == uint32_t(mm.blocked_doors_mask & 0xFFFFFFFFu));
    CHECK(v.blocked_doors_hi == uint32_t((mm.blocked_doors_mask >> 32) & 0xFFFFFFFFu));
    CHECK(v.missing_eggs_lo == uint32_t(mm.missing_eggs_mask & 0xFFFFFFFFu));
    CHECK(v.missing_eggs_hi == uint32_t((mm.missing_eggs_mask >> 32) & 0xFFFFFFFFu));
    CHECK(v.explored_pct == mm.explored_pct);
    CHECK(v.boss_hp == 0 && v.boss_max_hp == 0);

    // Acercarse al Wyrm activa el HUD de vida de Boss (4 segmentos, kind=2) y registra cada impacto.
    CHECK(sim.boss_count() == 1);
    {
      const auto& b0 = sim.boss(0);
      auto& p = sim.world().points()[sim.player_index()];
      p.pos = p.prev = {b0.x - 40.f, b0.y};
      auto ev0 = game::populate_hud_and_overlays(v, sc3, sim, menu, end_screen, dialogue, hud, "0.1.0");
      CHECK(v.boss_max_hp == 4 && v.boss_hp == 4 && v.boss_kind == 2);
      CHECK(v.boss_hurt_flash == 0.f && !ev0.boss_hit);

      // Golpear al Wyrm por encima reduce boss_hp a 3, activa boss_hurt_flash > 0 y dispara ev.boss_hit.
      p.pos = {b0.x, b0.y - b0.height() - Sim::kPlayerRadius + 1.5f};
      p.prev = {b0.x, p.pos.y - 2.f};
      sim.step({});
      auto ev1 = game::populate_hud_and_overlays(v, sc3, sim, menu, end_screen, dialogue, hud, "0.1.0");
      CHECK(v.boss_max_hp == 4 && v.boss_hp == 3 && v.boss_kind == 2);
      CHECK(v.boss_hurt_flash > 0.f && ev1.boss_hit);
    }
  }

  // --- 4) world.ewmap (opcional argv[4]): cristales, Tres Sellos, zonas y Mapa Expandido ---
  if (argc >= 5) {
    Scene scw;
    CHECK(scw.load(argv[4], err));
    const MinimapState mw0 = game::compute_minimap(scw);
    CHECK(mw0.map_w == 8 && mw0.map_h == 5);
    CHECK(mw0.seals_total == 3 && mw0.seals_mask == 0u);
    CHECK(game::zone_name_for(scw) == "EL POZO");
    // Sala (2,3) sólo tiene 'cr_archivo' como bloqueo: debe encender el bit (2,3) hasta romperlo.
    CHECK((mw0.blocked_doors_mask & bit_at(2, 3, 8)) != 0);
    const int cry_idx = find_entity_index(scw, "cr_archivo");
    CHECK(cry_idx >= 0);
    scw.set_active(std::size_t(cry_idx), true);
    const MinimapState mw1 = game::compute_minimap(scw);
    CHECK((mw1.blocked_doors_mask & bit_at(2, 3, 8)) == 0);

    // Abrir cada sello elemental enciende su bit correspondiente en seals_mask.
    const int sp = find_entity_index(scw, "door_seal_pozo");
    const int sc_cat = find_entity_index(scw, "door_seal_cat");
    const int sa = find_entity_index(scw, "door_seal_archivo");
    CHECK(sp >= 0 && sc_cat >= 0 && sa >= 0);
    scw.set_active(std::size_t(sp), true);
    CHECK(game::compute_minimap(scw).seals_mask == 1u);
    scw.set_active(std::size_t(sc_cat), true);
    CHECK(game::compute_minimap(scw).seals_mask == 3u);
    scw.set_active(std::size_t(sa), true);
    CHECK(game::compute_minimap(scw).seals_mask == 7u);

    // Zonas de world.ewmap, overlay de Mapa Expandido y Viaje Rápido entre Santuarios descubiertos.
    const auto ft0 = game::list_fast_travel_nodes(scw);
    CHECK(ft0.size() == 5u);
    CHECK(ft0[0].label == "EL POZO" && ft0[0].unlocked);
    CHECK(ft0[1].label == "LAS CATACUMBAS" && !ft0[1].unlocked);
    CHECK(ft0[2].label == "GUARIDA DEL WYRM" && !ft0[2].unlocked);
    CHECK(ft0[3].label == "ARCHIVO SUMERGIDO" && !ft0[3].unlocked);
    CHECK(ft0[4].label == "CAMPANARIO HUNDIDO" && !ft0[4].unlocked);

    scw.follow(ft0[1].x, ft0[1].y);  // visita (6,1) Las Catacumbas
    scw.follow(160.f, 4.f * 184.f + 92.f);  // (0,4) Campanario Hundido
    CHECK(game::zone_name_for(scw) == "CAMPANARIO HUNDIDO");
    const auto ft1 = game::list_fast_travel_nodes(scw);
    CHECK(ft1[0].unlocked && ft1[1].unlocked && !ft1[4].unlocked);

    Sim simw(scw);
    PauseMenu menu;
    EndScreen end_screen;
    DialogueBox dialogue;
    HudTracker hud;
    FrameData vw;
    game::populate_hud_and_overlays(vw, scw, simw, menu, end_screen, dialogue, hud, "0.1.0", true, 1);
    CHECK(vw.map_open == 1 && vw.seals_total == 3 && vw.seals_mask == 7);
    CHECK(vw.map_target_cx == ft1[1].cx && vw.map_target_cy == ft1[1].cy);
    const auto mwf = game::compute_minimap(scw);
    CHECK(vw.ctl_txt == game::pack_text(game::world_map_text("CAMPANARIO HUNDIDO", mwf.explored_pct,
                                                             vw.items, vw.items_total, mwf.seals_mask,
                                                             mwf.seals_total, ft1[1].label, true)));

    // Teletransporte al Santuario de Las Catacumbas: mueve jugador, normaliza gravedad y actualiza cámara/checkpoint.
    simw.set_gravity_flipped(true);
    (void)simw.take_sfx();
    CHECK(simw.teleport_to_sanctuary(ft1[1].x, ft1[1].y, ft1[1].ent_idx));
    CHECK((simw.take_sfx() & ew::audio::kSfxWarp) != 0u);
    CHECK(!simw.gravity_flipped());
    CHECK(scw.chunk_x() == ft1[1].cx && scw.chunk_y() == ft1[1].cy);
    CHECK(simw.checkpoint_index() == ft1[1].ent_idx);
    CHECK(simw.hp() == simw.max_hp());

    // Al cerrar el mapa tras cambiar de zona (Campanario -> Las Catacumbas), dispara banner de zona (toast_kind=0).
    simw.set_elapsed_sec(125.4f);
    game::populate_hud_and_overlays(vw, scw, simw, menu, end_screen, dialogue, hud, "0.1.0", false, -1, true);
    CHECK(vw.toast_alpha > 0.9f && vw.toast_kind == 0);
    CHECK(vw.show_timer == 1 && vw.timer_secs == 125);
    for (int f = 0; f < 130; ++f)
      game::populate_hud_and_overlays(vw, scw, simw, menu, end_screen, dialogue, hud, "0.1.0", false, -1, false);
    CHECK(vw.toast_alpha == 0.f && vw.show_timer == 0);

    // EndScreen muestra porcentaje de mapa explorado junto a los huevos cuando explored_pct > 0.
    game::EndStats est;
    est.items = 8; est.items_total = 8; est.explored_pct = 100;
    end_screen.open(est);
    CHECK(game::end_screen_text(end_screen)[2] == "HUEVOS: 8/8   MAPA: 100");
    end_screen.close();

    // Diario de la Vigía (tecla J): desglose de los 8 huevos en 5 zonas (2, 2, 2, 1, 1) y map_open == 2.
    const auto z0 = game::compute_zone_eggs(scw);
    CHECK(z0.size() == 5u);
    CHECK(z0[0].zone == "EL POZO" && z0[0].total == 2 && z0[0].found == 0);
    CHECK(z0[1].zone == "LAS CATACUMBAS" && z0[1].total == 2 && z0[1].found == 0);
    CHECK(z0[2].zone == "GUARIDA DEL WYRM" && z0[2].total == 2 && z0[2].found == 0);
    CHECK(z0[3].zone == "ARCHIVO SUMERGIDO" && z0[3].total == 1 && z0[3].found == 0);
    CHECK(z0[4].zone == "CAMPANARIO HUNDIDO" && z0[4].total == 1 && z0[4].found == 0);
    const int egg_arch_idx = find_entity_index(scw, "egg_archivo");
    CHECK(egg_arch_idx >= 0);
    scw.set_active(std::size_t(egg_arch_idx), true);
    CHECK(game::compute_zone_eggs(scw)[3].found == 1);
    game::populate_hud_and_overlays(vw, scw, simw, menu, end_screen, dialogue, hud, "0.1.0", false, -1, false, true);
    CHECK(vw.map_open == 2);
  }

  std::puts("minimap_test OK");
  return 0;
}
