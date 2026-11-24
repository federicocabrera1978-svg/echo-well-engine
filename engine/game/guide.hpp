#pragma once
// Flecha guía de navegación basada en datos del mapa (sección GUIDE = 11, sin GL).
#include "game/scene.hpp"
#include "game/sim.hpp"

#include <cmath>

namespace ew::game {

struct GuideArrow {
  float x = 0, y = 0, ux = 0, uy = 0, target_x = 0, target_y = 0;
  int style = 0;
  bool valid = false;
};

// Selecciona el primer objetivo activo del mapa (o fallback por links en mapas viejos),
// rutea por un pozo vertical si `split_y` separa al jugador del objetivo, y devuelve la
// posición/dirección de la flecha guía en pantalla.
inline GuideArrow compute_guide_arrow(const Scene& s, const Sim& sim, float time_s = 0.f) {
  if (sim.finished()) return {};
  const auto& ents = s.entities();
  const float px = sim.player().pos.x, py = sim.player().pos.y;
  const auto guides = s.map().guides();

  bool has_target = false;
  float tx = 0.f, ty = 0.f;
  int style = 0;

  if (!guides.empty()) {
    for (const auto& rec : guides) {
      if (rec.kind != uint16_t(ew::map::GuideKind::Objective)) continue;
      if (rec.entity >= ents.size()) continue;
      const auto& e = ents[rec.entity];
      const bool cross = (rec.flags & ew::map::kGuideCrossWhenOpen) != 0;
      const bool has_fwd = (rec.flags & ew::map::kGuideForwardX) != 0;
      const bool has_below = (rec.flags & ew::map::kGuideBelowY) != 0;

      if (cross) {
        if (!e.active) continue;
        if (rec.gate_entity != ~0u) {
          if (rec.gate_entity >= ents.size() || ents[rec.gate_entity].active) continue;
        }
        if (has_fwd && has_below) {
          if (py < float(rec.y_bot) && px > float(rec.x)) continue;
        } else if (has_below) {
          if (py < float(rec.y_bot)) continue;
        } else if (has_fwd) {
          if (px > float(rec.x)) continue;
        }
      } else {
        if (e.active) continue;
        if (e.type == "ability_double_jump" && sim.has_ability(kAbilityDoubleJump)) continue;
        if (e.type == "ability_dash" && sim.has_ability(kAbilityDash)) continue;
        if (e.type == "ability_wall_climb" && sim.has_ability(kAbilityWallClimb)) continue;
        if (e.type == "ability_reveal" && sim.has_ability(kAbilityReveal)) continue;
        if (rec.gate_entity != ~0u && rec.gate_entity < ents.size() && ents[rec.gate_entity].active) continue;
        if (has_fwd && px > float(rec.x)) continue;
        if (has_below && py < float(rec.y_bot)) continue;
      }

      has_target = true;
      tx = e.x;
      ty = e.y + (e.type == "door" ? float(kDoorHTiles * ew::map::kTileSize) * 0.5f : 0.f);
      style = (rec.flags & ew::map::kGuideAbility) ? 1 : 0;
      break;
    }
  } else {
    // Retrocompatibilidad con mapas sin sección GUIDE: primer switch inactivo hacia door inactiva.
    for (const auto& l : s.map().links()) {
      if (l.from_entity >= ents.size() || l.to_entity >= ents.size()) continue;
      const auto& from = ents[l.from_entity];
      const auto& to = ents[l.to_entity];
      if (to.type != "door" || to.active) continue;
      if (is_switch_type(from.type) && !from.active) {
        has_target = true;
        tx = from.x;
        ty = from.y;
        style = 0;
        break;
      }
    }
  }

  if (!has_target) return {};
  const float dist_full = std::hypot(tx - px, ty - py);
  if (dist_full <= 24.f) return {};

  float wx = tx, wy = ty;
  if (!guides.empty()) {
    const ew::map::GuideRecord* shaft = nullptr;
    float best_dx = 0.f;
    for (const auto& rec : guides) {
      if (rec.kind != uint16_t(ew::map::GuideKind::Shaft)) continue;
      const float split_y = float(rec.gate_entity);
      const bool going_down = (py < split_y && ty >= split_y);
      const bool going_up = (py >= split_y && ty < split_y);
      if (!going_down && !going_up) continue;
      const float sx = going_down
                           ? float(rec.x)
                           : float(rec.entity == ~0u ? rec.x : static_cast<int32_t>(rec.entity));
      const float d = std::abs(px - sx);
      if (!shaft || d < best_dx) {
        shaft = &rec;
        best_dx = d;
        wx = sx;
        wy = going_down ? ((d > 42.f) ? float(rec.y_top) : float(rec.y_bot))
                        : ((d > 42.f) ? float(rec.y_bot) : float(rec.y_top));
      }
    }
  }

  const float dx = wx - px, dy = wy - py;
  const float len = std::hypot(dx, dy);
  if (len < 1e-3f) return {};
  const float ux = dx / len, uy = dy / len;
  const float pulse = std::sin(time_s * 6.0f) * 2.0f;

  GuideArrow out;
  out.x = std::round(px + ux * (17.f + pulse));
  out.y = std::round(py - 4.f + uy * (17.f + pulse));
  out.ux = ux;
  out.uy = uy;
  out.target_x = tx;
  out.target_y = ty;
  out.style = style;
  out.valid = true;
  return out;
}

}  // namespace ew::game
