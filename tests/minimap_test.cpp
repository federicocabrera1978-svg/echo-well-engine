// Test sin GL (Etapa 8): estado derivado del mini-mapa Metroidvania (minimap.hpp)
// y volcado de máscaras + porcentaje explorado en FrameData vía populate_hud_and_overlays.
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
  }

  std::puts("minimap_test OK");
  return 0;
}
