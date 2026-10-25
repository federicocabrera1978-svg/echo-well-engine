#include "physics/water.hpp"

#include <algorithm>
#include <cmath>

namespace ew::physics {

WaterBody::WaterBody(float x, float y, float w, float h, float stiffness, float damping, int max_cols)
    : x_(x), y_(y), w_(w), h_px_(h),
      k_(stiffness > 0 ? stiffness : 0.025f), d_(damping > 0 ? damping : 0.03f) {
  const int n = std::clamp(int(w / 2.f), 2, std::max(2, max_cols));  // ~2 px por columna
  h_.assign(std::size_t(n), 0.f);
  v_ = ldelta_ = rdelta_ = h_;
}

int WaterBody::col_of(float x) const {
  return std::clamp(int((x - x_) / w_ * float(cols())), 0, cols() - 1);
}

float WaterBody::surface_at(float x) const { return y_ + h_[std::size_t(col_of(x))]; }

void WaterBody::disturb(float x, float dv) {
  if (x < x_ || x >= x_ + w_) return;
  v_[std::size_t(col_of(x))] += dv;
}

void WaterBody::step() {
  const std::size_t n = h_.size();
  // Ley de Hooke por columna (desplazamiento h respecto del reposo).
  for (std::size_t i = 0; i < n; ++i) {
    v_[i] += -k_ * h_[i] - d_ * v_[i];
    h_[i] += v_[i];
    h_[i] = std::clamp(h_[i], -h_px_ * 0.9f, h_px_ * 0.9f);
  }
  // Propagación: cada columna tira de sus vecinas.
  for (int p = 0; p < spread_passes; ++p) {
    for (std::size_t i = 0; i < n; ++i) {
      ldelta_[i] = i > 0 ? spread * (h_[i] - h_[i - 1]) : 0.f;
      rdelta_[i] = i + 1 < n ? spread * (h_[i] - h_[i + 1]) : 0.f;
    }
    for (std::size_t i = 0; i < n; ++i) {
      if (i > 0) { v_[i - 1] += ldelta_[i]; h_[i - 1] += ldelta_[i] * 0.5f; }
      if (i + 1 < n) { v_[i + 1] += rdelta_[i]; h_[i + 1] += rdelta_[i] * 0.5f; }
    }
  }
}

void WaterBody::interact(World& world) {
  for (auto& p : world.points()) {
    if (p.inv_mass == 0 || p.pos.x < x_ || p.pos.x >= x_ + w_) continue;
    const float surf = surface_at(p.pos.x);
    const float top = p.pos.y - p.radius, bot = p.pos.y + p.radius;
    if (bot <= surf || top >= bottom()) continue;
    const float sub = std::clamp((bot - surf) / (2.f * p.radius), 0.f, 1.f);  // fracción sumergida
    p.force.y -= buoyancy * sub / p.inv_mass;  // empuje (Arquímedes simplificado)
    Vec2 v = p.velocity();
    // Salpicadura solo al cruzar la superficie (evita realimentación cuerpo↔resorte);
    // estela suave al moverse en horizontal cerca de la superficie.
    if ((p.prev.y < surf) != (p.pos.y < surf)) disturb(p.pos.x, v.y * splash * p.radius);
    else if (std::fabs(v.x) > 0.3f && top < surf + 1.f) disturb(p.pos.x, std::fabs(v.x) * 0.03f);
    v = v * (1.f - drag * sub);
    p.set_velocity(v);
  }
}

float WaterBody::energy() const {
  float e = 0;
  for (std::size_t i = 0; i < h_.size(); ++i) e += v_[i] * v_[i] + k_ * h_[i] * h_[i];
  return e;
}

}  // namespace ew::physics
