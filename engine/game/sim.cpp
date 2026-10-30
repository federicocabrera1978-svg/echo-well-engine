#include "game/sim.hpp"

#include <algorithm>

namespace ew::game {
using ew::physics::Vec2;

Sim::Sim(Scene& scene) : scene_(scene) {
  world_.tile_size = float(ew::map::kTileSize);
  const Entity* spawn = scene_.find_type("player_spawn");
  const Vec2 p = spawn ? Vec2{spawn->x, spawn->y} : Vec2{16, 16};
  player_ = world_.add_point(p, kPlayerRadius, 1);
  // Cuerda demo: cuelga del techo de la vista sobre el centro-izquierda del mapa.
  rope_n_ = 16;
  rope_first_ = world_.add_rope({std::min(128.f, scene_.width_px() * 0.4f), 24}, rope_n_, kRopeSeg, 1.5f);
  for (int i = 1; i <= rope_n_; ++i) world_.add_collision_pair(player_, rope_first_ + uint32_t(i));
  for (const auto& w : scene_.map().water())
    water_.emplace_back(float(w.x), float(w.y), float(w.w), float(w.h), w.stiffness, w.damping);
}

bool Sim::player_in_water() const {
  const auto& p = player();
  for (const auto& w : water_)
    if (w.contains(p.pos.x, p.pos.y + p.radius)) return true;
  return false;
}

void Sim::grab(int node) {
  held_ = node;
  climb_acc_ = 0;
  auto& pts = world_.points();
  pts[rope_first_ + uint32_t(node)].inv_mass = kHeldInvMass;
  world_.set_pairs_enabled(player_, false);
}

void Sim::release() {
  if (!held_) return;
  world_.points()[rope_first_ + uint32_t(held_)].inv_mass = 1;
  world_.set_pairs_enabled(player_, true);
  held_ = 0;
  regrab_cd_ = 12;
}

void Sim::step(const Input& in) {
  auto& pts = world_.points();
  if (regrab_cd_ > 0) --regrab_cd_;
  // Agarrar: nodo más cercano dentro de kGrabRange.
  if (in.grab && !held_ && regrab_cd_ == 0) {
    int best = 0;
    float bd = kGrabRange + 1e-3f;
    for (int i = 1; i <= rope_n_; ++i) {
      const float d = ew::physics::length(pts[rope_first_ + uint32_t(i)].pos - pts[player_].pos);
      if (d < bd) { bd = d; best = i; }
    }
    if (best) grab(best);
  } else if (in.grab && held_) {
    release();
  }

  if (held_) {
    // Colgado: ←/→ empujan el nodo (balanceo), ↑/↓ trepan, saltar suelta con impulso.
    auto& node = pts[rope_first_ + uint32_t(held_)];
    node.force += Vec2{std::clamp(in.move_x, -1.f, 1.f) * kSwingForce / kHeldInvMass, 0};
    const float seg = kRopeSeg;
    climb_acc_ += std::clamp(in.climb, -1.f, 1.f) * kClimbSpeed * kDt;
    if (climb_acc_ <= -seg && held_ > 1) { climb_acc_ = 0; int n = held_ - 1; release(); regrab_cd_ = 0; grab(n); }
    else if (climb_acc_ >= seg && held_ < rope_n_) { climb_acc_ = 0; int n = held_ + 1; release(); regrab_cd_ = 0; grab(n); }
    else climb_acc_ = std::clamp(climb_acc_, -seg, seg);
    if (in.jump) {
      const Vec2 v = pts[rope_first_ + uint32_t(held_)].velocity();
      release();
      pts[player_].set_velocity({v.x * 1.2f, std::min(v.y, 0.f) - kJumpSpeed * 0.8f * kDt});
    }
  }

  auto& pl = pts[player_];
  if (!held_) {
    // Control: velocidad horizontal directa (respuesta arcade), salto si está apoyado.
    Vec2 v = pl.velocity();
    const float target = std::clamp(in.move_x, -1.f, 1.f) * kWalkSpeed * kDt;
    v.x += (target - v.x) * (pl.grounded ? 0.5f : 0.15f);
    const bool swim = player_in_water();
    if (in.jump && (pl.grounded || swim)) v.y = -kJumpSpeed * (swim ? 0.8f : 1.f) * kDt;
    pl.set_velocity(v);
    if (in.interact) scene_.interact(pl.pos.x, pl.pos.y);
  }
  for (auto& w : water_) { w.interact(world_); w.step(); }
  world_.step(kDt, [this](int tx, int ty) { return scene_.solid_tile(tx, ty); });
  if (held_) {  // el player viaja pegado al nodo (hereda su velocidad)
    const auto& node = world_.points()[rope_first_ + uint32_t(held_)];
    auto& p = world_.points()[player_];
    p.pos = node.pos;
    p.prev = node.prev;
    p.grounded = false;
  }
  scene_.follow(world_.points()[player_].pos.x, world_.points()[player_].pos.y, kDt);
}

}  // namespace ew::game
