#include "physics/verlet.hpp"

#include <algorithm>

namespace ew::physics {

uint32_t World::add_point(Vec2 p, float radius, float inv_mass) {
  Point pt;
  pt.pos = pt.prev = p;
  pt.radius = radius;
  pt.inv_mass = inv_mass;
  points_.push_back(pt);
  return uint32_t(points_.size() - 1);
}

uint32_t World::add_stick(uint32_t a, uint32_t b, float stiffness) {
  sticks_.push_back({a, b, length(points_[b].pos - points_[a].pos), stiffness});
  return uint32_t(sticks_.size() - 1);
}

uint32_t World::add_rope(Vec2 anchor, int n, float seg, float radius) {
  const uint32_t first = add_point(anchor, radius, 0);
  points_[first].collide_tiles = false;
  for (int i = 1; i <= n; ++i) {
    const uint32_t p = add_point(anchor + Vec2{0, seg * float(i)}, radius, 1);
    add_stick(p - 1, p);
  }
  return first;
}

void World::step(float dt, const SolidFn& solid) {
  // Integración Verlet: x' = x + (x - x_prev) * damping + a * dt²
  for (auto& p : points_) {
    if (p.inv_mass == 0) { p.force = {}; p.prev = p.pos; continue; }
    const Vec2 acc = gravity + p.force * p.inv_mass;
    const Vec2 vel = (p.pos - p.prev) * damping;
    p.prev = p.pos;
    p.pos += vel + acc * (dt * dt);
    p.force = {};
    p.grounded = false;
  }
  for (int it = 0; it < iterations; ++it) {
    solve_sticks();
    solve_pairs();
    for (auto& p : points_)
      if (p.inv_mass > 0 && p.collide_tiles) collide(p, solid);
    collide_sticks(solid);
  }
}

void World::solve_sticks() {
  for (const auto& s : sticks_) {
    Point& a = points_[s.a];
    Point& b = points_[s.b];
    const float w = a.inv_mass + b.inv_mass;
    if (w == 0) continue;
    const Vec2 d = b.pos - a.pos;
    const float len = length(d);
    if (len < 1e-6f) continue;
    const Vec2 corr = d * ((len - s.rest) / len * s.stiffness / w);
    a.pos += corr * a.inv_mass;
    b.pos -= corr * b.inv_mass;
  }
}

void World::set_pairs_enabled(uint32_t i, bool on) {
  for (auto& pr : pairs_)
    if (pr.a == i || pr.b == i) pr.on = on;
}

void World::solve_pairs() {
  for (const auto& pr : pairs_) {
    if (!pr.on) continue;
    Point& a = points_[pr.a];
    Point& b = points_[pr.b];
    const float w = a.inv_mass + b.inv_mass;
    if (w == 0) continue;
    const Vec2 d = b.pos - a.pos;
    const float len = length(d), min = a.radius + b.radius;
    if (len >= min || len < 1e-6f) continue;
    const Vec2 corr = d * ((len - min) / len / w);
    a.pos += corr * a.inv_mass;
    b.pos -= corr * b.inv_mass;
  }
}

// Círculo vs tiles sólidos: empuja fuera por el punto más cercano de cada tile
// solapado y aplica fricción a la componente tangencial de la velocidad.
void World::collide(Point& p, const SolidFn& solid) {
  const float ts = tile_size;
  const int x0 = int(std::floor((p.pos.x - p.radius) / ts)), x1 = int(std::floor((p.pos.x + p.radius) / ts));
  const int y0 = int(std::floor((p.pos.y - p.radius) / ts)), y1 = int(std::floor((p.pos.y + p.radius) / ts));
  for (int ty = y0; ty <= y1; ++ty)
    for (int tx = x0; tx <= x1; ++tx) {
      if (!solid(tx, ty)) continue;
      const Vec2 closest{std::clamp(p.pos.x, tx * ts, (tx + 1) * ts), std::clamp(p.pos.y, ty * ts, (ty + 1) * ts)};
      Vec2 d = p.pos - closest;
      float dist = length(d);
      Vec2 n;
      float pen;
      if (dist > 1e-6f) {
        if (dist >= p.radius) continue;
        n = d * (1.f / dist);
        pen = p.radius - dist;
      } else {  // centro dentro del tile: salir por el eje más corto
        const float l = p.pos.x - tx * ts, r = (tx + 1) * ts - p.pos.x;
        const float u = p.pos.y - ty * ts, b = (ty + 1) * ts - p.pos.y;
        const float m = std::min({l, r, u, b});
        n = m == l ? Vec2{-1, 0} : m == r ? Vec2{1, 0} : m == u ? Vec2{0, -1} : Vec2{0, 1};
        pen = m + p.radius;
      }
      p.pos += n * pen;
      // Fricción + sin rebote: quitar velocidad normal entrante, atenuar tangencial.
      Vec2 v = p.pos - p.prev;
      const float vn = dot(v, n);
      if (vn < 0) v -= n * vn;
      const Vec2 vt = v - n * dot(v, n);
      v -= vt * friction;
      p.prev = p.pos - v;
      if (n.y < -0.5f) p.grounded = true;
    }
}

namespace {
// Segmento vs AABB (Liang-Barsky). El AABB se achica un epsilon para permitir
// rozar la cara (envolverse en una esquina) sin contar como atravesar.
bool segment_hits_aabb(Vec2 a, Vec2 b, float l, float t, float r, float bot) {
  constexpr float eps = 0.05f;
  l += eps; t += eps; r -= eps; bot -= eps;
  if (l >= r || t >= bot) return false;
  float t0 = 0.f, t1 = 1.f;
  const float dx = b.x - a.x, dy = b.y - a.y;
  auto clip = [&](float p, float q) {
    if (std::fabs(p) < 1e-12f) return q >= 0.f;
    const float u = q / p;
    if (p < 0.f) { if (u > t1) return false; if (u > t0) t0 = u; }
    else         { if (u < t0) return false; if (u < t1) t1 = u; }
    return true;
  };
  return clip(-dx, a.x - l) && clip(dx, r - a.x) && clip(-dy, a.y - t) && clip(dy, bot - a.y);
}

bool segment_blocked(Vec2 a, Vec2 b, float ts, const SolidFn& solid) {
  const float minx = std::min(a.x, b.x), maxx = std::max(a.x, b.x);
  const float miny = std::min(a.y, b.y), maxy = std::max(a.y, b.y);
  const int x0 = int(std::floor(minx / ts)), x1 = int(std::floor(maxx / ts));
  const int y0 = int(std::floor(miny / ts)), y1 = int(std::floor(maxy / ts));
  for (int ty = y0; ty <= y1; ++ty)
    for (int tx = x0; tx <= x1; ++tx)
      if (solid(tx, ty) && segment_hits_aabb(a, b, tx * ts, ty * ts, (tx + 1) * ts, (ty + 1) * ts))
        return true;
  return false;
}
}  // namespace

void World::collide_sticks(const SolidFn& solid) {
  const float ts = tile_size;
  for (const auto& s : sticks_) {
    Point& a = points_[s.a];
    Point& b = points_[s.b];
    if (a.inv_mass == 0 && b.inv_mass == 0) continue;
    if (!segment_blocked(a.pos, b.pos, ts, solid)) continue;
    const bool ma = a.inv_mass > 0, mb = b.inv_mass > 0;
    float best = 1e9f;
    Vec2 da{}, db{}, ndir{};
    const Vec2 dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    const float hi0 = std::max(ts * 2.f, length(b.pos - a.pos) + ts);
    for (const auto& dir : dirs) {
      auto clear_at = [&](float d) {
        const Vec2 pa = ma ? a.pos + dir * d : a.pos;
        const Vec2 pb = mb ? b.pos + dir * d : b.pos;
        return !segment_blocked(pa, pb, ts, solid);
      };
      if (!clear_at(hi0)) continue;
      float lo = 0.f, hi = hi0;
      for (int i = 0; i < 12; ++i) {
        const float mid = (lo + hi) * 0.5f;
        if (clear_at(mid)) hi = mid; else lo = mid;
      }
      if (hi < best) {
        best = hi;
        da = ma ? dir * hi : Vec2{};
        db = mb ? dir * hi : Vec2{};
        ndir = dir;
      }
    }
    if (best > 1e8f) continue;
    auto apply = [&](Point& p, Vec2 dlt) {
      if (dlt.x == 0 && dlt.y == 0) return;
      // Conservar velocidad previa al empuje y anular la componente entrante.
      const Vec2 v0 = p.pos - p.prev;
      p.pos += dlt;
      Vec2 v = v0;
      const float vn = dot(v, ndir);
      if (vn < 0) v -= ndir * vn;
      p.prev = p.pos - v;
    };
    apply(a, da);
    apply(b, db);
  }
}

}  // namespace ew::physics
