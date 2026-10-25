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
  rope_first_ = world_.add_rope({std::min(128.f, scene_.width_px() * 0.4f), 24}, rope_n_, 6.f, 1.5f);
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

void Sim::step(const Input& in) {
  auto& pl = world_.points()[player_];
  // Control: velocidad horizontal directa (respuesta arcade), salto si está apoyado.
  Vec2 v = pl.velocity();
  const float target = std::clamp(in.move_x, -1.f, 1.f) * kWalkSpeed * kDt;
  v.x += (target - v.x) * (pl.grounded ? 0.5f : 0.15f);
  const bool swim = player_in_water();
  if (in.jump && (pl.grounded || swim)) v.y = -kJumpSpeed * (swim ? 0.8f : 1.f) * kDt;
  pl.set_velocity(v);
  for (auto& w : water_) { w.interact(world_); w.step(); }
  world_.step(kDt, [this](int tx, int ty) { return scene_.solid_tile(tx, ty); });
  scene_.follow(world_.points()[player_].pos.x, world_.points()[player_].pos.y);
}

}  // namespace ew::game
