#pragma once
// Poblado de HUD y overlays (pausa, controles, pantalla final, diálogo, minimapa) sin GL/GLFW.
#include "game/dialogue.hpp"
#include "game/end_screen.hpp"
#include "game/frame_view.hpp"
#include "game/menu.hpp"
#include "game/menu_text.hpp"
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
};

struct HudEvents {
  bool new_item = false;
  bool new_double_jump = false;
  bool new_dash = false;
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
    tracker.last_abilities = ab;
  }
  v.abilities = int(ab);
  v.ability_flash = float(sim.ability_flash()) / float(Sim::kAbilityFlashFrames);
  v.map_w = scene.map().header().map_w_chunks;
  v.map_h = scene.map().header().map_h_chunks;
  v.cur_cx = scene.chunk_x();
  v.cur_cy = scene.chunk_y();
  v.visited_mask = uint32_t(scene.visited_chunks() & 0xFFFFFFFFu);
  return ev;
}

}  // namespace ew::game
