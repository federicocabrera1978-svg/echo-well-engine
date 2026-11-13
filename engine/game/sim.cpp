#include "game/sim.hpp"

#include <algorithm>
#include <cmath>

namespace ew::game {
using ew::physics::Vec2;

Sim::Sim(Scene& scene) : scene_(scene) { build(); }

void Sim::build() {
  world_.clear();
  world_.tile_size = float(ew::map::kTileSize);
  water_.clear();
  ropes_.clear();
  held_rope_ = 0;
  held_ = 0;
  climb_acc_ = 0;
  regrab_cd_ = 0;
  grab_flash_ = 0;
  held_r0_ = kRopeRadius;
  finished_ = false;
  finish_t_ = 0;
  ckpt_ = -1;
  deaths_ = 0;
  invuln_ = 0;
  const Entity* spawn = scene_.find_type("player_spawn");
  const Vec2 p = spawn ? Vec2{spawn->x, spawn->y} : Vec2{16, 16};
  player_ = world_.add_point(p, kPlayerRadius, 1);
  for (const auto& r : scene_.map().ropes()) {  // sección ROPE (validada por ewmapc)
    if (r.segments < 1 || r.segments > 256 || !(r.seg_len > 0)) continue;
    const int n = int(r.segments);
    const uint32_t first = world_.add_rope({float(r.x), float(r.y)}, n, r.seg_len, kRopeRadius);
    for (int i = 1; i <= n; ++i) world_.add_collision_pair(player_, first + uint32_t(i));
    ropes_.push_back({first, n, r.seg_len});
  }
  for (const auto& w : scene_.map().water())
    water_.emplace_back(float(w.x), float(w.y), float(w.w), float(w.h), w.stiffness, w.damping);
}

void Sim::reset() {
  scene_.reset_state();
  build();
}

const Entity* Sim::checkpoint() const {
  if (ckpt_ < 0) return nullptr;
  const auto& e = scene_.entities();
  if (std::size_t(ckpt_) >= e.size()) return nullptr;
  return &e[std::size_t(ckpt_)];
}

void Sim::respawn() {
  if (held_) release();
  const Entity* at = checkpoint();
  if (!at) at = scene_.find_type("player_spawn");
  const Vec2 p = at ? Vec2{at->x, at->y} : Vec2{16, 16};
  auto& pl = world_.points()[player_];
  pl.pos = pl.prev = p;
  pl.force = {};
  pl.grounded = false;
  finished_ = false;
  finish_t_ = 0;
  invuln_ = kRespawnInvuln;
  grab_flash_ = 0;
  regrab_cd_ = 0;
  climb_acc_ = 0;
  scene_.follow(p.x, p.y, 0.f);
}

void Sim::kill() {
  if (invuln_ > 0 || finished_) return;
  ++deaths_;
  respawn();
}

bool Sim::player_in_water() const {
  const auto& p = player();
  for (const auto& w : water_)
    if (w.contains(p.pos.x, p.pos.y + p.radius)) return true;
  return false;
}

void Sim::grab(int rope, int node, bool flash) {
  held_rope_ = rope;
  held_ = node;
  climb_acc_ = 0;
  if (flash) grab_flash_ = kGrabFlashFrames;
  auto& pts = world_.points();
  auto& nd = pts[held_index()];
  held_r0_ = nd.radius;
  nd.inv_mass = kHeldInvMass;
  nd.radius = kPlayerRadius;  // el player viaja en el nodo: misma colisión contra tiles
  world_.set_pairs_enabled(player_, false);
}

void Sim::release() {
  if (!held_) return;
  auto& nd = world_.points()[held_index()];
  nd.inv_mass = 1;
  nd.radius = held_r0_;
  world_.set_pairs_enabled(player_, true);
  held_ = 0;
  regrab_cd_ = 12;
}

void Sim::step(const Input& in) {
  if (in.restart_full) { reset(); return; }
  if (in.restart) { respawn(); return; }
  if (invuln_ > 0) --invuln_;
  if (finished_) {
    finish_t_ = std::min(1.f, finish_t_ + kDt / kFinishFadeSec);
    for (auto& w : water_) { w.interact(world_); w.step(); }
    world_.step(kDt, [this](int tx, int ty) { return scene_.solid_tile(tx, ty); });
    const auto& p = world_.points()[player_];
    scene_.follow(p.pos.x, p.pos.y, kDt);
    scene_.tick(kDt);
    return;
  }
  auto& pts = world_.points();
  if (regrab_cd_ > 0) --regrab_cd_;
  if (grab_flash_ > 0) --grab_flash_;
  // Agarrar: nodo más cercano dentro de kGrabRange.
  if (in.grab && !held_ && regrab_cd_ == 0) {
    int best = 0, best_r = 0;
    float bd = kGrabRange + 1e-3f;
    for (int r = 0; r < rope_count(); ++r)
      for (int i = 1; i <= ropes_[std::size_t(r)].n; ++i) {
        const float d = ew::physics::length(pts[ropes_[std::size_t(r)].first + uint32_t(i)].pos - pts[player_].pos);
        if (d < bd) { bd = d; best = i; best_r = r; }
      }
    if (best) grab(best_r, best);
  } else if (in.grab && held_) {
    release();
  }

  if (held_) {
    // Colgado: ←/→ empujan el nodo (balanceo), ↑/↓ trepan, saltar suelta con impulso.
    auto& node = pts[held_index()];
    node.force += Vec2{std::clamp(in.move_x, -1.f, 1.f) * kSwingForce / kHeldInvMass, 0};
    const float seg = ropes_[std::size_t(held_rope_)].seg;
    const int rn = ropes_[std::size_t(held_rope_)].n, hr = held_rope_;
    climb_acc_ += std::clamp(in.climb, -1.f, 1.f) * kClimbSpeed * kDt;
    if (climb_acc_ <= -seg && held_ > 1) { climb_acc_ = 0; int n = held_ - 1; release(); regrab_cd_ = 0; grab(hr, n, false); }
    else if (climb_acc_ >= seg && held_ < rn) { climb_acc_ = 0; int n = held_ + 1; release(); regrab_cd_ = 0; grab(hr, n, false); }
    else climb_acc_ = std::clamp(climb_acc_, -seg, seg);
    if (in.jump) {
      const Vec2 v = pts[held_index()].velocity();
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
    const auto& node = world_.points()[held_index()];
    auto& p = world_.points()[player_];
    p.pos = node.pos;
    p.prev = node.prev;
    p.grounded = false;
  }
  scene_.follow(world_.points()[player_].pos.x, world_.points()[player_].pos.y, kDt);
  scene_.tick(kDt);
  check_checkpoints();
  check_hazards();
  check_goal();
}

void Sim::check_goal() {
  if (finished_) return;
  const auto& p = player();
  for (const auto& e : scene_.entities()) {
    if (e.type != "goal") continue;
    const float dx = e.x - p.pos.x, dy = e.y - p.pos.y;
    if (dx * dx + dy * dy <= kGoalReach * kGoalReach) { finished_ = true; return; }
  }
  const int tx = int(std::floor(p.pos.x / float(ew::map::kTileSize)));
  const int ty = int(std::floor(p.pos.y / float(ew::map::kTileSize)));
  if (scene_.tile(tx, ty, ew::map::Layer::META) == kMetaGoal) finished_ = true;
}

void Sim::check_checkpoints() {
  const auto& p = player();
  const auto& ents = scene_.entities();
  const float r2 = kCheckpointReach * kCheckpointReach;
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].type != "checkpoint") continue;
    const float dx = ents[i].x - p.pos.x, dy = ents[i].y - p.pos.y;
    if (dx * dx + dy * dy > r2) continue;
    if (ckpt_ == int(i)) return;
    if (ckpt_ >= 0) scene_.set_active(std::size_t(ckpt_), false);
    ckpt_ = int(i);
    scene_.set_active(i, true);
    return;
  }
}

void Sim::check_hazards() {
  if (invuln_ > 0 || finished_) return;
  const auto& p = player();
  const float ts = float(ew::map::kTileSize), r = p.radius;
  const int x0 = int(std::floor((p.pos.x - r) / ts)), x1 = int(std::floor((p.pos.x + r) / ts));
  const int y0 = int(std::floor((p.pos.y - r) / ts)), y1 = int(std::floor((p.pos.y + r) / ts));
  for (int ty = y0; ty <= y1; ++ty)
    for (int tx = x0; tx <= x1; ++tx) {
      if (scene_.tile(tx, ty, ew::map::Layer::META) != kMetaHazard) continue;
      const Vec2 c{std::clamp(p.pos.x, tx * ts, (tx + 1) * ts),
                   std::clamp(p.pos.y, ty * ts, (ty + 1) * ts)};
      if (ew::physics::length(p.pos - c) < r) { kill(); return; }
    }
  if (p.pos.y > float(scene_.height_px()) + r) kill();  // caída fuera del mapa
}

}  // namespace ew::game
