#pragma once
// Poblado de HUD y overlays (pausa, controles, pantalla final, diálogo, minimapa) sin GL/GLFW.
#include "game/dialogue.hpp"
#include "game/end_screen.hpp"
#include "game/frame_view.hpp"
#include "game/menu.hpp"
#include "game/menu_text.hpp"
#include "game/minimap.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace ew::game {

struct HudTracker {
  int last_items = 0;
  uint32_t last_abilities = 0;
  int last_max_hp = 1;
  bool last_dash_up = false;
  int last_boss_hp = 0;
  int last_hp = 1;
  int last_deaths = 0;
  float damage_flash = 0.f;
  float milestone_flash = 0.f;
  std::string last_zone;
  uint32_t last_seals = 0;
  std::string toast_text;
  int toast_ticks = 0;
  int toast_kind = 0;
};

struct HudEvents {
  bool new_item = false;
  bool new_double_jump = false;
  bool new_dash = false;
  bool new_wall_climb = false;
  bool new_reveal = false;
  bool new_extra_hp = false;
  bool new_dash_upgrade = false;
  bool boss_hit = false;
  bool took_damage = false;
};

inline HudEvents populate_hud_and_overlays(FrameData& v, const Scene& scene, const Sim& sim, const PauseMenu& menu,
                                           const EndScreen& end_screen, const DialogueBox& dialogue,
                                           HudTracker& tracker, std::string_view version_str,
                                           bool map_open = false, int map_travel_cursor = -1) {
  v.pause = menu.open() ? 1 : 0;
  v.pause_sel = menu.cursor();
  v.pause_vol = menu.volume_setting();
  v.pause_mute = menu.muted() ? 1 : 0;
  v.pause_has_save = menu.has_save() ? 1 : 0;
  v.ctl = 0;
  v.ctl_sel = 0;
  v.ctl_wait = 0;
  v.hero_select = menu.in_hero_select() ? 1 : 0;
  v.hero_cursor = menu.hero_cursor();
  v.end_screen = 0;
  v.end_sel = 0;
  v.end_has_next = 0;
  v.end_true = 0;
  v.dlg = 0;
  v.dlg_more = 0;
  v.ctl_txt = {};

  if (menu.in_hero_select()) {
    v.ctl_txt = pack_text(hero_select_text(menu));
  } else if (menu.in_controls()) {
    v.ctl = 1;
    v.ctl_sel = menu.ctl_cursor() + 1;
    v.ctl_wait = menu.waiting() ? 1 : 0;
    v.ctl_txt = pack_text(controls_text(menu));
  } else if (menu.open()) {
    v.ctl_txt = pack_text(pause_text(menu, version_str));
  }
  if (!menu.open() && end_screen.open()) {
    v.end_screen = 1;
    v.end_sel = end_screen.cursor();
    v.end_has_next = end_screen.stats().has_next ? 1 : 0;
    v.end_true = end_screen.stats().true_ending ? 1 : 0;
    v.ctl_txt = pack_text(end_screen_text(end_screen));
  } else if (!menu.open() && dialogue.open()) {
    v.dlg = 1;
    v.dlg_more = dialogue.has_more() ? 1 : 0;
    v.ctl_txt = pack_text(dialogue_text(dialogue));
  }

  v.items = sim.collected();
  v.items_total = sim.collectible_total();
  v.items_flash = float(sim.pickup_flash()) / float(Sim::kPickupFlashFrames);
  HudEvents ev;
  if (v.items != tracker.last_items) {
    if (v.items > tracker.last_items) ev.new_item = true;
    tracker.last_items = v.items;
  }
  const uint32_t ab = sim.abilities();
  if (ab != tracker.last_abilities) {
    if ((ab & kAbilityDoubleJump) && !(tracker.last_abilities & kAbilityDoubleJump)) ev.new_double_jump = true;
    if ((ab & kAbilityDash) && !(tracker.last_abilities & kAbilityDash)) ev.new_dash = true;
    if ((ab & kAbilityWallClimb) && !(tracker.last_abilities & kAbilityWallClimb)) ev.new_wall_climb = true;
    if ((ab & kAbilityReveal) && !(tracker.last_abilities & kAbilityReveal)) ev.new_reveal = true;
    tracker.last_abilities = ab;
  }
  v.abilities = int(ab);
  v.ability_flash = float(sim.ability_flash()) / float(Sim::kAbilityFlashFrames);
  v.hp = sim.hp();
  v.max_hp = sim.max_hp();
  v.dash_upgraded = sim.dash_upgraded() ? 1 : 0;
  v.dash_charges = sim.dash_charges();
  v.dash_charges_max = sim.max_dash_charges();
  if (v.max_hp != tracker.last_max_hp) {
    if (v.max_hp > tracker.last_max_hp) ev.new_extra_hp = true;
    tracker.last_max_hp = v.max_hp;
  }
  const bool dash_up = sim.dash_upgraded();
  if (dash_up != tracker.last_dash_up) {
    if (dash_up && !tracker.last_dash_up) ev.new_dash_upgrade = true;
    tracker.last_dash_up = dash_up;
  }
  if (v.hp < tracker.last_hp || sim.deaths() > tracker.last_deaths) {
    ev.took_damage = true;
    tracker.damage_flash = 1.f;
  }
  tracker.last_hp = v.hp;
  tracker.last_deaths = sim.deaths();
  if (ev.new_extra_hp || ev.new_dash_upgrade) {
    tracker.milestone_flash = 1.f;
  }
  v.damage_flash = tracker.damage_flash;
  v.milestone_flash = tracker.milestone_flash;
  tracker.damage_flash = std::max(0.f, tracker.damage_flash - 1.f / 22.f);
  tracker.milestone_flash = std::max(0.f, tracker.milestone_flash - 1.f / 45.f);
  v.boss_hp = 0;
  v.boss_max_hp = 0;
  v.boss_kind = 0;
  v.boss_hurt_flash = 0.f;
  const auto& ppos = sim.player().pos;
  float best_d2 = 1e30f;
  int best_bi = -1;
  for (int bi = 0; bi < sim.boss_count(); ++bi) {
    const auto& b = sim.boss(bi);
    if (b.defeated) continue;
    const float dx = std::fabs(b.x - ppos.x);
    const float dy = std::fabs(b.y - ppos.y);
    if (dx <= 220.f && dy <= 120.f) {
      const float d2 = dx * dx + dy * dy;
      if (d2 < best_d2) {
        best_d2 = d2;
        best_bi = bi;
      }
    }
  }
  if (best_bi >= 0) {
    const auto& b = sim.boss(best_bi);
    v.boss_max_hp = b.max_hits();
    v.boss_hp = std::max(0, b.max_hits() - b.hits);
    v.boss_kind = b.dragon ? 2 : (b.sentinel ? 3 : (b.guardian2 ? 1 : 0));
    v.boss_hurt_flash = std::clamp(b.hurt / Sim::kBossHurtSec, 0.f, 1.f);
    if (tracker.last_boss_hp > 0 && v.boss_hp < tracker.last_boss_hp) ev.boss_hit = true;
    tracker.last_boss_hp = v.boss_hp;
  } else {
    tracker.last_boss_hp = 0;
  }
  const MinimapState mm = compute_minimap(scene);
  v.map_w = mm.map_w;
  v.map_h = mm.map_h;
  v.cur_cx = mm.cur_cx;
  v.cur_cy = mm.cur_cy;
  v.visited_mask = uint32_t(mm.visited_mask & 0xFFFFFFFFu);
  v.visited_mask_hi = uint32_t((mm.visited_mask >> 32) & 0xFFFFFFFFu);
  v.blocked_doors_lo = uint32_t(mm.blocked_doors_mask & 0xFFFFFFFFu);
  v.blocked_doors_hi = uint32_t((mm.blocked_doors_mask >> 32) & 0xFFFFFFFFu);
  v.missing_eggs_lo = uint32_t(mm.missing_eggs_mask & 0xFFFFFFFFu);
  v.missing_eggs_hi = uint32_t((mm.missing_eggs_mask >> 32) & 0xFFFFFFFFu);
  v.explored_pct = mm.explored_pct;
  v.seals_mask = int(mm.seals_mask);
  v.seals_total = mm.seals_total;
  v.map_open = (!menu.open() && !end_screen.open() && !dialogue.open() && map_open) ? 1 : 0;
  v.map_target_cx = -1;
  v.map_target_cy = -1;
  if (v.map_open) {
    std::string_view tlabel;
    bool tunlocked = false;
    if (map_travel_cursor >= 0) {
      const auto nodes = list_fast_travel_nodes(scene);
      if (!nodes.empty()) {
        const auto& nd = nodes[std::size_t(map_travel_cursor) % nodes.size()];
        tlabel = nd.label;
        tunlocked = nd.unlocked;
        if (nd.unlocked) {
          v.map_target_cx = nd.cx;
          v.map_target_cy = nd.cy;
        }
      }
    }
    v.ctl_txt = pack_text(world_map_text(zone_name_for(scene), mm.explored_pct, v.items, v.items_total,
                                         mm.seals_mask, mm.seals_total, tlabel, tunlocked));
  }

  const std::string_view cur_zone = zone_name_for(scene);
  const uint32_t new_seals = mm.seals_mask & ~tracker.last_seals;
  if (new_seals != 0u && !tracker.last_zone.empty()) {
    tracker.toast_text = (new_seals & 1u) ? "SELLO DE PIEDRA ROTO"
                       : (new_seals & 2u) ? "SELLO DE SOMBRA ROTO"
                                          : "SELLO DEL AGUA ROTO";
    tracker.toast_kind = 2;
    tracker.toast_ticks = 140;
  } else if (ev.new_dash_upgrade) {
    tracker.toast_text = "HITO: DASH II DESBLOQUEADO";
    tracker.toast_kind = 1;
    tracker.toast_ticks = 140;
  } else if (ev.new_extra_hp) {
    tracker.toast_text = (v.max_hp >= 3) ? "HITO: 3 CORAZONES" : "HITO: +1 CORAZON";
    tracker.toast_kind = 1;
    tracker.toast_ticks = 140;
  } else if (!tracker.last_zone.empty() && cur_zone != tracker.last_zone) {
    tracker.toast_text = std::string(cur_zone);
    tracker.toast_kind = 0;
    tracker.toast_ticks = 120;
  }
  tracker.last_zone = std::string(cur_zone);
  tracker.last_seals = mm.seals_mask;

  v.toast_alpha = 0.f;
  v.toast_kind = tracker.toast_kind;
  if (tracker.toast_ticks > 0 && !menu.open() && !end_screen.open() && !dialogue.open() && !v.map_open) {
    v.toast_alpha = std::min(1.f, float(tracker.toast_ticks) / 24.f);
    std::array<std::string, kTxtRows> rows{};
    std::string line = tracker.toast_text;
    if (line.size() > std::size_t(kTxtCols)) line.resize(std::size_t(kTxtCols));
    const std::size_t pad = (std::size_t(kTxtCols) - line.size()) / 2;
    rows[0] = std::string(pad, ' ') + line;
    v.ctl_txt = pack_text(rows);
    --tracker.toast_ticks;
  }
  return ev;
}

}  // namespace ew::game
