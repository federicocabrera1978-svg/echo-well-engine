#include "game/sim.hpp"

#include <algorithm>
#include <cmath>

namespace ew::game {
using ew::physics::Vec2;

Sim::Sim(Scene& scene) : scene_(scene) { build(); }

void Sim::build() {
  solid_fn_ = [this](int tx, int ty) { return scene_.solid_tile(tx, ty); };
  world_.clear();
  world_.tile_size = float(ew::map::kTileSize);
  water_.clear();
  ropes_.clear();
  movers_.clear();
  ridden_ = -1;
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
  pickup_flash_ = 0;
  sfx_ = 0;
  was_in_water_ = false;
  flip_ = ckpt_flip_ = false;
  flips_ = 0;
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
  const auto ents = scene_.map().entities();
  for (const auto& rec : scene_.map().movers()) {
    if (rec.entity >= ents.size()) continue;
    if (scene_.map().string_at(ents[rec.entity].type_str) != "mover") continue;
    if (rec.n_wp < 2 || rec.n_wp > ew::map::kMaxMoverWaypoints) continue;
    if (rec.w_tiles < 1 || rec.h_tiles < 1) continue;
    if (!(rec.speed > 0.f)) continue;
    MoverState m;
    m.w = float(rec.w_tiles * ew::map::kTileSize);
    m.h = float(rec.h_tiles * ew::map::kTileSize);
    m.speed = rec.speed;
    m.pause = rec.pause < 0.f ? 0.f : rec.pause;
    m.tile = rec.tile;
    m.wp.reserve(rec.n_wp);
    for (uint16_t i = 0; i < rec.n_wp; ++i) m.wp.push_back({float(rec.wp_x[i]), float(rec.wp_y[i])});
    m.x = m.prev_x = m.wp[0].x;
    m.y = m.prev_y = m.wp[0].y;
    m.seg = 0;
    m.dir = 1;
    m.dist = 0;
    m.wait = 0;
    movers_.push_back(std::move(m));
  }
  rearm_flips();
  bosses_.clear();
  const auto& es = scene_.entities();
  for (std::size_t i = 0; i < es.size(); ++i) {
    if (es[i].type != "boss") continue;
    Boss b;
    b.home_x = es[i].x;
    b.home_y = es[i].y;
    b.ent = i;
    bosses_.push_back(b);
  }
  reset_bosses(false);
}

void Sim::reset_bosses(bool keep_defeated) {
  for (auto& b : bosses_) {
    if (keep_defeated && b.defeated) continue;
    b.x = b.home_x;
    b.y = b.home_y;
    b.hits = 0;
    b.hurt = 0;
    b.defeated = false;
    b.phase = 0;
    b.t = 0;
    b.dir = -1;
    b.vy = 0;
  }
}

bool Sim::bosses_defeated() const {
  for (const auto& b : bosses_)
    if (!b.defeated) return false;
  return true;
}

void Sim::advance_bosses() {
  for (auto& b : bosses_) {
    if (b.defeated) continue;
    if (b.hurt > 0) b.hurt = std::max(0.f, b.hurt - kDt);
    const float lo = b.home_x - kBossRange, hi = b.home_x + kBossRange;
    auto walk = [&](float v) {
      b.x += float(b.dir) * v * kDt;
      if (b.x > hi) { b.x = hi; b.dir = -1; }
      if (b.x < lo) { b.x = lo; b.dir = 1; }
    };
    b.t += kDt;
    if (b.phase == 0) {
      walk(b.speed());
      if (b.t >= kBossWalkSec) { b.phase = 1; b.t = 0; }
    } else if (b.phase == 1) {
      if (b.t >= kBossTellSec) { b.phase = 2; b.t = 0; b.vy = -kBossHopSpeed; }
    } else {
      walk(b.speed() * 1.5f);
      b.vy += 600.f * kDt;
      b.y += b.vy * kDt;
      if (b.y >= b.home_y) { b.y = b.home_y; b.vy = 0; b.phase = 0; b.t = 0; }
    }
  }
}

void Sim::set_gravity_flipped(bool f) {
  flip_ = f;
  world_.points()[player_].gscale = f ? -1.f : 1.f;
}

// Orbes al alcance quedan desarmados (no disparan al reaparecer encima); el resto, armados.
void Sim::rearm_flips() {
  const auto& ents = scene_.entities();
  flip_armed_.assign(ents.size(), 0);
  const auto& p = world_.points()[player_].pos;
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].type != "gravity_flip") continue;
    const float dx = ents[i].x - p.x, dy = ents[i].y - p.y;
    flip_armed_[i] = (dx * dx + dy * dy > kFlipReach * kFlipReach) ? 1 : 0;
  }
}

void Sim::check_gravity_flips() {
  const auto& p = player().pos;
  const auto& ents = scene_.entities();
  if (flip_armed_.size() != ents.size()) flip_armed_.assign(ents.size(), 1);
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].type != "gravity_flip") continue;
    const float dx = ents[i].x - p.x, dy = ents[i].y - p.y, d2 = dx * dx + dy * dy;
    if (!flip_armed_[i]) {
      if (d2 > kFlipRearm * kFlipRearm) flip_armed_[i] = 1;
      continue;
    }
    if (d2 > kFlipReach * kFlipReach) continue;
    flip_armed_[i] = 0;
    set_gravity_flipped(!flip_);
    ++flips_;
    // El orbe muestra el sentido actual (active = invertida) para el render.
    for (std::size_t k = 0; k < ents.size(); ++k)
      if (ents[k].type == "gravity_flip") scene_.set_active(k, flip_);
    sfx_ |= 16u;  // kSfxSwitch
    return;
  }
}

void Sim::reset_movers() {
  ridden_ = -1;
  for (auto& m : movers_) {
    if (m.wp.empty()) continue;
    m.x = m.prev_x = m.wp[0].x;
    m.y = m.prev_y = m.wp[0].y;
    m.seg = 0;
    m.dir = 1;
    m.dist = 0;
    m.wait = 0;
  }
}

void Sim::reset() {
  scene_.reset_state();
  build();
}

void Sim::load_state(int checkpoint, bool flipped, const std::vector<uint8_t>& active) {
  reset();
  const auto& ents = scene_.entities();
  for (std::size_t i = 0; i < ents.size() && i < active.size(); ++i) {
    if (!active[i]) continue;
    scene_.set_active(i, true);
    if (ents[i].type == "door") scene_.entities_mut()[i].anim = 1.f;  // abierta, sin animación
  }
  for (auto& b : bosses_)
    if (b.ent < active.size() && active[b.ent]) b.defeated = true;
  ckpt_ = (checkpoint >= 0 && std::size_t(checkpoint) < ents.size() && ents[std::size_t(checkpoint)].type == "checkpoint")
              ? checkpoint : -1;
  ckpt_flip_ = ckpt_ >= 0 && flipped;
  respawn();
  invuln_ = 0;
  // Pileta inundada: el nivel queda directo en su destino (sin la subida animada).
  for (int i = 0; i < 4096; ++i) {
    bool moved = false;
    std::vector<float> before;
    for (const auto& w : water_) before.push_back(w.rest_y());
    apply_flood();
    for (std::size_t k = 0; k < water_.size(); ++k) moved = moved || water_[k].rest_y() != before[k];
    if (!moved) break;
  }
  const auto& p = world_.points()[player_].pos;
  scene_.follow(p.x, p.y);  // snap de cámara
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
  reset_movers();
  reset_bosses(true);  // el boss se cura y vuelve a su lugar (si ya estaba vencido, queda así)
  set_gravity_flipped(ckpt_flip_);  // la gravedad vuelve a la del checkpoint (spawn = normal)
  for (std::size_t k = 0; k < scene_.entities().size(); ++k)
    if (scene_.entities()[k].type == "gravity_flip") scene_.set_active(k, flip_);
  rearm_flips();
  scene_.follow(p.x, p.y, 0.f);
}

void Sim::kill() {
  if (invuln_ > 0 || finished_) return;
  ++deaths_;
  sfx_ |= 2u;  // kSfxDie
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
    apply_flood();
    for (auto& w : water_) { w.interact(world_); w.step(); }
    advance_movers();
    advance_bosses();
    world_.step(kDt, solid_fn_);
    collide_movers();
    const auto& p = world_.points()[player_];
    scene_.follow(p.pos.x, p.pos.y, kDt);
    scene_.tick(kDt);
    return;
  }
  auto& pts = world_.points();
  if (regrab_cd_ > 0) --regrab_cd_;
  if (grab_flash_ > 0) --grab_flash_;
  if (pickup_flash_ > 0) --pickup_flash_;
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
      const float d = down();  // el salto empuja contra la gravedad del player
      const float vy = d > 0 ? std::min(v.y, 0.f) : std::max(v.y, 0.f);
      pts[player_].set_velocity({v.x * 1.2f, vy - d * kJumpSpeed * 0.8f * kDt});
      sfx_ |= 1u;  // kSfxJump
    }
  }

  auto& pl = pts[player_];
  if (!held_) {
    // Control: velocidad horizontal directa (respuesta arcade), salto si está apoyado.
    Vec2 v = pl.velocity();
    const float target = std::clamp(in.move_x, -1.f, 1.f) * kWalkSpeed * kDt;
    v.x += (target - v.x) * (pl.grounded ? 0.5f : 0.15f);
    const bool swim = player_in_water();
    if (in.jump && (pl.grounded || swim)) {
      v.y = -down() * kJumpSpeed * (swim ? 0.8f : 1.f) * kDt;
      sfx_ |= 1u;  // kSfxJump (audio/sfx.hpp)
    }
    pl.set_velocity(v);
    if (in.interact && scene_.interact(pl.pos.x, pl.pos.y) >= 0) sfx_ |= 16u;  // kSfxSwitch
  }
  apply_flood();
  for (auto& w : water_) { w.interact(world_); w.step(); }
  advance_movers();
  advance_bosses();
  carry_player();
  world_.step(kDt, solid_fn_);
  if (held_) {  // el player viaja pegado al nodo (hereda su velocidad)
    const auto& node = world_.points()[held_index()];
    auto& p = world_.points()[player_];
    p.pos = node.pos;
    p.prev = node.prev;
    p.grounded = false;
  }
  collide_movers();
  if (held_) {  // re-sync si un mover empujó el nodo agarrado
    const auto& node = world_.points()[held_index()];
    auto& p = world_.points()[player_];
    p.pos = node.pos;
    p.prev = node.prev;
    p.grounded = false;
  }
  scene_.follow(world_.points()[player_].pos.x, world_.points()[player_].pos.y, kDt);
  scene_.tick(kDt);
  const bool in_w = player_in_water();
  if (in_w && !was_in_water_) sfx_ |= 32u;  // kSfxSplash
  was_in_water_ = in_w;
  check_gravity_flips();
  check_checkpoints();
  check_collectibles();
  collide_bosses();
  check_hazards();
  check_goal();
}

int Sim::collected() const {
  int n = 0;
  for (const auto& e : scene_.entities()) n += (e.type == "collectible" && e.active) ? 1 : 0;
  return n;
}

int Sim::collectible_total() const {
  int n = 0;
  for (const auto& e : scene_.entities()) n += e.type == "collectible" ? 1 : 0;
  return n;
}

void Sim::check_collectibles() {
  const auto& p = player();
  const auto& ents = scene_.entities();
  const float r2 = kCollectReach * kCollectReach;
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].type != "collectible" || ents[i].active) continue;
    const float dx = ents[i].x - p.pos.x, dy = ents[i].y - p.pos.y;
    if (dx * dx + dy * dy > r2) continue;
    scene_.set_active(i, true);  // agarrado: persiste hasta reset()
    pickup_flash_ = kPickupFlashFrames;
    sfx_ |= 64u;  // kSfxPickup
  }
}

void Sim::check_goal() {
  if (finished_ || !bosses_defeated()) return;  // con un boss vivo la meta está cerrada
  const auto& p = player();
  for (const auto& e : scene_.entities()) {
    if (e.type != "goal") continue;
    const float dx = e.x - p.pos.x, dy = e.y - p.pos.y;
    if (dx * dx + dy * dy <= kGoalReach * kGoalReach) { finished_ = true; sfx_ |= 8u; return; }
  }
  const int tx = int(std::floor(p.pos.x / float(ew::map::kTileSize)));
  const int ty = int(std::floor(p.pos.y / float(ew::map::kTileSize)));
  if (scene_.tile(tx, ty, ew::map::Layer::META) == kMetaGoal) { finished_ = true; sfx_ |= 8u; }
}

void Sim::check_checkpoints() {
  const auto& p = player();
  const auto& ents = scene_.entities();
  const float r2 = kCheckpointReach * kCheckpointReach;
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].type != "checkpoint") continue;
    const float dx = ents[i].x - p.pos.x, dy = ents[i].y - p.pos.y;
    if (dx * dx + dy * dy > r2) continue;
    if (ckpt_ == int(i)) return;  // la gravedad se guarda solo al activarlo
    if (ckpt_ >= 0) scene_.set_active(std::size_t(ckpt_), false);
    ckpt_ = int(i);
    ckpt_flip_ = flip_;
    scene_.set_active(i, true);
    sfx_ |= 4u;  // kSfxCheckpoint
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
  if (p.pos.y > float(scene_.height_px()) + r || p.pos.y < -r) kill();  // caída fuera del mapa
}

void Sim::apply_flood() {
  bool on = false;
  for (const auto& e : scene_.entities())
    if (e.type == "flood_switch" && e.active) { on = true; break; }
  for (auto& w : water_) {
    if (w.x() < 400.f) continue;  // la pileta del inicio no se inunda
    const float tgt = on ? kFloodRestY : w.initial_rest_y();
    const float y = w.rest_y();
    const float dy = tgt - y;
    if (std::fabs(dy) < 0.05f) { w.set_rest_y(tgt); continue; }
    const float step = kFloodSpeed * kDt;
    w.set_rest_y(y + std::copysign(std::min(step, std::fabs(dy)), dy));
  }
}

void Sim::advance_movers() {
  using ew::physics::length;
  for (auto& m : movers_) {
    m.prev_x = m.x;
    m.prev_y = m.y;
    if (m.wp.size() < 2 || !(m.speed > 0.f)) continue;
    const int n = int(m.wp.size());
    float leftover = kDt;
    int guard = 0;
    while (leftover > 1e-8f && guard++ < 16) {
      if (m.wait > 0.f) {
        const float d = std::min(m.wait, leftover);
        m.wait -= d;
        leftover -= d;
        continue;
      }
      int b = m.seg + m.dir;
      if (b < 0 || b >= n) { m.dir = -m.dir; b = m.seg + m.dir; }
      if (b < 0 || b >= n) break;
      const auto& A = m.wp[std::size_t(m.seg)];
      const auto& B = m.wp[std::size_t(b)];
      const float seg_len = length(B - A);
      if (seg_len < 1e-6f) {
        m.seg = b;
        m.dist = 0;
        if (m.seg + m.dir < 0 || m.seg + m.dir >= n) m.dir = -m.dir;
        m.wait = m.pause;
        continue;
      }
      const float remain = seg_len - m.dist;
      const float step = m.speed * leftover;
      if (step < remain) {
        m.dist += step;
        leftover = 0;
      } else {
        leftover = (step - remain) / m.speed;
        m.seg = b;
        m.dist = 0;
        if (m.seg + m.dir < 0 || m.seg + m.dir >= n) m.dir = -m.dir;
        m.wait = m.pause;
      }
    }
    int b = m.seg + m.dir;
    if (b < 0 || b >= n) b = m.seg;
    const auto& A = m.wp[std::size_t(m.seg)];
    const auto& B = m.wp[std::size_t(b)];
    const float seg_len = length(B - A);
    const float t = seg_len > 1e-6f ? m.dist / seg_len : 0.f;
    m.x = A.x + (B.x - A.x) * t;
    m.y = A.y + (B.y - A.y) * t;
  }
}

void Sim::carry_player() {
  if (held_ || ridden_ < 0 || ridden_ >= int(movers_.size())) return;
  const auto& m = movers_[std::size_t(ridden_)];
  const float dx = m.x - m.prev_x, dy = m.y - m.prev_y;
  if (dx == 0.f && dy == 0.f) return;
  auto& p = world_.points()[player_];
  p.pos.x += dx; p.pos.y += dy;
  p.prev.x += dx; p.prev.y += dy;
}

namespace {
// Círculo vs AABB. Empuja afuera, fricción como en tiles, grounded si sale por arriba (n.y < -0.5).
bool collide_circle_aabb(ew::physics::Point& p, float l, float top, float r, float bot) {
  const float gs = p.gscale < 0 ? -1.f : 1.f;
  using ew::physics::Vec2;
  using ew::physics::dot;
  using ew::physics::length;
  const Vec2 closest{std::clamp(p.pos.x, l, r), std::clamp(p.pos.y, top, bot)};
  const Vec2 d = p.pos - closest;
  const float dist = length(d);
  Vec2 n;
  float pen;
  if (dist > 1e-6f) {
    if (dist >= p.radius) return false;
    n = d * (1.f / dist);
    pen = p.radius - dist;
  } else {
    const float left = p.pos.x - l, right = r - p.pos.x;
    const float up = p.pos.y - top, down = bot - p.pos.y;
    const float m = std::min({left, right, up, down});
    n = m == left ? Vec2{-1, 0} : m == right ? Vec2{1, 0} : m == up ? Vec2{0, -1} : Vec2{0, 1};
    pen = m + p.radius;
  }
  p.pos += n * pen;
  Vec2 v = p.pos - p.prev;
  const float vn = dot(v, n);
  if (vn < 0) v -= n * vn;
  const Vec2 vt = v - n * dot(v, n);
  v -= vt * 0.08f;
  p.prev = p.pos - v;
  if (n.y * gs < -0.5f) p.grounded = true;  // apoyado contra su gravedad (arriba si está invertida)
  return true;
}

bool circle_hits_aabb(ew::physics::Vec2 c, float rad, float l, float top, float r, float bot, float eps) {
  using ew::physics::Vec2;
  using ew::physics::length;
  const Vec2 closest{std::clamp(c.x, l, r), std::clamp(c.y, top, bot)};
  return length(c - closest) < rad - eps;
}
}  // namespace

void Sim::collide_movers() {
  if (movers_.empty()) { ridden_ = -1; return; }
  auto& pts = world_.points();
  const uint32_t skip = held_ ? player_ : ~0u;  // colgado: el nodo representa al player
  int ride = -1;
  for (int it = 0; it < 4; ++it) {
    for (int i = 0; i < int(movers_.size()); ++i) {
      const auto& m = movers_[std::size_t(i)];
      const float l = m.x, top = m.y, r = m.x + m.w, bot = m.y + m.h;
      for (uint32_t k = 0; k < uint32_t(pts.size()); ++k) {
        auto& p = pts[k];
        if (p.inv_mass == 0 || !p.collide_tiles || k == skip) continue;
        const bool was_grounded = p.grounded;
        if (collide_circle_aabb(p, l, top, r, bot) && k == player_ && p.grounded && !was_grounded)
          ride = i;
        else if (k == player_ && p.grounded && ride < 0 &&
                 (flip_ ? p.pos.y - p.radius >= bot - 1.f : p.pos.y + p.radius <= top + 1.f) &&
                 p.pos.x + p.radius > l && p.pos.x - p.radius < r)
          ride = i;
      }
    }
  }
  if (!held_) ridden_ = ride;
  else ridden_ = -1;

  // Aplastamiento: si después de empujar el player sigue metido en un sólido (tile u
  // otra plataforma) más que un epsilon, muere. Quien queda solo apoyado no pisa esto.
  if (held_ || invuln_ > 0 || finished_) return;
  auto& pl = pts[player_];
  constexpr float eps = 0.15f;
  const float ts = float(ew::map::kTileSize), rad = pl.radius;
  const int x0 = int(std::floor((pl.pos.x - rad) / ts)), x1 = int(std::floor((pl.pos.x + rad) / ts));
  const int y0 = int(std::floor((pl.pos.y - rad) / ts)), y1 = int(std::floor((pl.pos.y + rad) / ts));
  for (int ty = y0; ty <= y1; ++ty)
    for (int tx = x0; tx <= x1; ++tx) {
      if (!scene_.solid_tile(tx, ty)) continue;
      const ew::physics::Vec2 c{std::clamp(pl.pos.x, tx * ts, (tx + 1) * ts),
                                std::clamp(pl.pos.y, ty * ts, (ty + 1) * ts)};
      if (ew::physics::length(pl.pos - c) < rad - eps) { kill(); return; }
    }
  for (const auto& m : movers_) {
    if (circle_hits_aabb(pl.pos, rad, m.x, m.y, m.x + m.w, m.y + m.h, eps)) { kill(); return; }
  }
}

}  // namespace ew::game

namespace ew::game {

void Sim::collide_bosses() {
  if (bosses_.empty() || finished_) return;
  auto& pts = world_.points();
  auto& pl = pts[player_];
  const float r = pl.radius, d = down();
  for (auto& b : bosses_) {
    if (b.defeated) continue;
    const float l = b.x - kBossW * 0.5f, rt = b.x + kBossW * 0.5f, top = b.y - kBossH, bot = b.y;
    const ew::physics::Vec2 c{std::clamp(pl.pos.x, l, rt), std::clamp(pl.pos.y, top, bot)};
    if (ew::physics::length(pl.pos - c) >= r) continue;
    if (b.hurt > 0) continue;  // recién golpeado: se lo puede atravesar
    // Pisotón: el player va hacia su "abajo" y su centro está del lado de su "arriba" del boss.
    const float vy = pl.velocity().y;
    const bool from_above = d > 0 ? (vy > 0.f && pl.pos.y <= top + 3.f) : (vy < 0.f && pl.pos.y >= bot - 3.f);
    if (from_above) {
      ++b.hits;
      b.hurt = kBossHurtSec;
      sfx_ |= 16u;  // kSfxSwitch (golpe)
      // Rebote: fuera de la caja y con impulso contra su gravedad.
      if (held_) release();
      pl.pos.y = d > 0 ? top - r : bot + r;
      pl.set_velocity({pl.velocity().x, -d * kJumpSpeed * 0.9f * kDt});
      if (b.hits >= kBossHits) {
        b.defeated = true;
        scene_.set_linked(b.ent, true);  // abre las puertas linkeadas
        sfx_ |= 4u;  // kSfxCheckpoint (fanfarria corta)
      }
      return;
    }
    if (invuln_ > 0) continue;
    kill();
    return;
  }
}

}  // namespace ew::game
