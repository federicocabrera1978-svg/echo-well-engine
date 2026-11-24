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
};

struct HudEvents {
  bool new_item = false;
  bool new_double_jump = false;
  bool new_dash = false;
  bool new_wall_climb = false;
  bool new_reveal = false;
  bool new_extra_hp = false;
  bool new_dash_upgrade = false;
};

inline HudEvents populate_hud_and_overlays(FrameData& v, const Scene& scene, const Sim& sim, const PauseMenu& menu,
                                           const EndScreen& end_screen, const DialogueBox& dialogue,
                                           HudTracker& tracker, std::string_view version_str) {
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
  if (v.max_hp != tracker.last_max_hp) {
    if (v.max_hp > tracker.last_max_hp) ev.new_extra_hp = true;
    tracker.last_max_hp = v.max_hp;
  }
  const bool dash_up = sim.dash_upgraded();
  if (dash_up != tracker.last_dash_up) {
    if (dash_up && !tracker.last_dash_up) ev.new_dash_upgrade = true;
    tracker.last_dash_up = dash_up;
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
  return ev;
}

}  // namespace ew::game
