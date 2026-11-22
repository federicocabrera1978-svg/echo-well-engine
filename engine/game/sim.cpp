#include "game/sim.hpp"

#include "audio/sfx.hpp"
#include "game/hero.hpp"

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
  abilities_ = 0;
  air_jumps_ = 0;
  double_jump_flash_ = 0;
  double_jump_x_ = 0;
  double_jump_y_ = 0;
  ability_flash_ = 0;
  dash_ticks_ = 0;
  dash_cd_ = 0;
  dash_dir_ = 1;
  dash_ready_ = false;
  dash_flash_ = 0;
  wall_break_flash_ = 0;
  wall_break_x_ = 0;
  wall_break_y_ = 0;
  boss_burst_ = 0;
  boss_burst_x_ = 0;
  boss_burst_y_ = 0;
  victory_.reset();
  wind_.reset();
  was_grounded_ = false;
  facing_ = 1;
  anim_ = PlayerAnim::Idle;
  anim_frame_ = 0;
  anim_ticks_ = 0;
  air_ticks_ = 0;
  pending_dialog_ = -1;
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
  const auto links = scene_.map().links();
  for (const auto& rec : scene_.map().movers()) {
    if (rec.entity >= ents.size()) continue;
    const auto type = scene_.map().string_at(ents[rec.entity].type_str);
    if (type != "mover" && type != "elevator") continue;
    if (rec.n_wp < 2 || rec.n_wp > ew::map::kMaxMoverWaypoints) continue;
    if (rec.w_tiles < 1 || rec.h_tiles < 1) continue;
    if (!(rec.speed > 0.f)) continue;
    MoverState m;
    m.is_elevator = (type == "elevator");
    m.ent = rec.entity;
    m.w = float(rec.w_tiles * ew::map::kTileSize);
    m.h = float(rec.h_tiles * ew::map::kTileSize);
    m.speed = rec.speed;
    m.pause = rec.pause < 0.f ? 0.f : rec.pause;
    m.tile = rec.tile;
    m.wp.reserve(rec.n_wp);
    for (uint16_t i = 0; i < rec.n_wp; ++i) m.wp.push_back({float(rec.wp_x[i]), float(rec.wp_y[i])});
    m.wp0_x = m.wp[0].x;
    m.wp0_y = m.wp[0].y;
    m.wp1_x = m.wp[1].x;
    m.wp1_y = m.wp[1].y;
    m.x = m.prev_x = m.wp[0].x;
    m.y = m.prev_y = m.wp[0].y;
    m.seg = 0;
    m.dir = 1;
    m.dist = 0;
    m.wait = 0;
    m.elev_target = 0;
    m.elev_at = 0;
    m.elev_wait = 0.f;
    if (m.is_elevator) {
      bool has_link = false;
      for (const auto& l : links) {
        if (l.to_entity == m.ent) { has_link = true; break; }
      }
      scene_.set_active(m.ent, !has_link);
    }
    movers_.push_back(std::move(m));
  }
  rearm_flips();
  bosses_.clear();
  fireballs_.clear();
  imps_.clear();
  elapsed_sec_ = 0.f;
  const auto& es = scene_.entities();
  for (std::size_t i = 0; i < es.size(); ++i) {
    if (es[i].type == "imp") {
      imps_.push_back({es[i].x, es[i].y, es[i].x, es[i].y, 1, false, i});
      continue;
    }
    if (es[i].type != "boss" && es[i].type != "boss_dragon") continue;
    Boss b;
    b.home_x = es[i].x;
    b.home_y = es[i].y;
    b.ent = i;
    b.dragon = (es[i].type == "boss_dragon");
    b.guardian2 = (es[i].name == "boss_catacomb");
    bosses_.push_back(b);
  }
  reset_bosses(false);
  reset_imps();
}

void Sim::reset_imps() {
  for (auto& im : imps_) {
    im.x = im.home_x;
    im.y = im.home_y;
    im.dir = 1;
    im.defeated = false;
  }
}

void Sim::reset_bosses(bool keep_defeated) {
  fireballs_.clear();
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
    b.cycle = 0;
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
    const float range = b.dragon ? kDragonRange : kBossRange;
    const float lo = b.home_x - range, hi = b.home_x + range;
    auto walk = [&](float v) {
      b.x += float(b.dir) * v * kDt;
      if (b.x > hi) { b.x = hi; b.dir = -1; }
      if (b.x < lo) { b.x = lo; b.dir = 1; }
    };
    b.t += kDt;
    if (!b.dragon) {
      if (b.phase == 0) {
        walk(b.speed());
        if (b.t >= kBossWalkSec) { b.phase = 1; b.t = 0; }
      } else if (b.phase == 1) {
        if (b.t >= kBossTellSec) {
          b.phase = 2;
          b.t = 0;
          b.vy = -kBossHopSpeed;
          // Al saltar, el boss lanza brasas (en arco alto en Nivel 1 herido; dobles laterales en Nivel 2).
          if (b.guardian2 && fireballs_.size() + 2 <= 8) {
            const float fy = b.y - 5.f;
            fireballs_.push_back({b.x - 10.f, fy, -76.f, -18.f, 0.36f});
            fireballs_.push_back({b.x + 10.f, fy, 76.f, -18.f, 0.36f});
          } else if (!b.guardian2 && b.hits >= 1 && fireballs_.size() < 8) {
            fireballs_.push_back({b.x + float(b.dir) * 14.f, b.y - kBossH - 4.f, float(b.dir) * 55.f, -75.f, 0.34f});
          }
        }
      } else {
        walk(std::min(b.speed() * (b.guardian2 ? 1.45f : 1.5f), 105.f));
        b.vy += 600.f * kDt;
        b.y += b.vy * kDt;
        if (b.y >= b.home_y) {
          b.y = b.home_y; b.vy = 0; b.phase = 0; b.t = 0; ++b.cycle;
          wind_.push_impulse(b.x, b.y, float(b.dir) * 2.2f, 28.f, 0.38f);
        }
      }
      continue;
    }
    // Dragón multipatrón: patrulla → aviso → alterna salto con bolas de fuego (par) y embestida rasa (impar).
    if (b.phase == 0) {
      walk(b.speed());
      if (b.t >= kDragonWalkSec) { b.phase = 1; b.t = 0; }
    } else if (b.phase == 1) {
      if (b.t >= kDragonTellSec) {
        b.dir = (player().pos.x >= b.x) ? 1 : -1;
        b.t = 0;
        if ((b.cycle % 2) == 0) {
          b.phase = 2;
          b.vy = -kDragonHopSpeed;
          const float fy = b.y - kDragonH * 0.55f;
          if (fireballs_.size() + 2 <= 8) {
            fireballs_.push_back({b.x - 10.f, fy, -kFireballSpeed, 18.f, kFireballLife});
            fireballs_.push_back({b.x + 10.f, fy, kFireballSpeed, 18.f, kFireballLife});
          }
        } else {
          b.phase = 3;
        }
      }
    } else if (b.phase == 2) {
      walk(b.speed() * 1.45f);
      b.vy += 600.f * kDt;
      b.y += b.vy * kDt;
      if (b.y >= b.home_y) {
        b.y = b.home_y;
        b.vy = 0;
        b.phase = 0;
        b.t = 0;
        ++b.cycle;
        wind_.push_impulse(b.x, b.y, float(b.dir) * 2.8f, 34.f, 0.42f);
      }
    } else {
      walk(kDragonChargeSpeed + float(b.hits) * 10.f);
      if (b.t >= kDragonChargeSec) {
        b.phase = 0;
        b.t = 0;
        ++b.cycle;
      }
    }
  }
  for (std::size_t i = 0; i < fireballs_.size();) {
    auto& fb = fireballs_[i];
    fb.x += fb.vx * kDt;
    fb.y += fb.vy * kDt;
    fb.life -= kDt;
    if (fb.life <= 0.f || scene_.solid_px(fb.x, fb.y)) {
      fireballs_[i] = fireballs_.back();
      fireballs_.pop_back();
    } else {
      ++i;
    }
  }
}

void Sim::grant_ability(uint32_t mask) { set_abilities(abilities_ | mask); }

void Sim::set_abilities(uint32_t mask) {
  const uint32_t next = mask & kAbilityAll;
  const bool had_dj = has_ability(kAbilityDoubleJump);
  const bool has_dj = (next & kAbilityDoubleJump) != 0;
  const bool had_dash = has_ability(kAbilityDash);
  const bool has_dash = (next & kAbilityDash) != 0;
  abilities_ = next;
  if (!has_dj) air_jumps_ = 0;
  else if (!had_dj) air_jumps_ = 1;
  if (!has_dash) { dash_ready_ = false; dash_ticks_ = 0; }
  else if (!had_dash) dash_ready_ = true;
}

void Sim::set_gravity_flipped(bool f) {
  flip_ = f;
  if (dash_ticks_ == 0) world_.points()[player_].gscale = f ? -1.f : 1.f;
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
    m.elev_at = 0;
    m.elev_target = 0;
    m.elev_wait = 0.f;
  }
}

void Sim::reset() {
  scene_.reset_state();
  build();
}

void Sim::load_state(int checkpoint, bool flipped, const std::vector<uint8_t>& active, uint32_t abilities) {
  reset();
  const auto& ents = scene_.entities();
  uint32_t ab = abilities & kAbilityAll;
  for (std::size_t i = 0; i < ents.size() && i < active.size(); ++i) {
    if (!active[i]) continue;
    scene_.set_active(i, true);
    if (ents[i].type == "door" || ents[i].type == "one_way_door") scene_.entities_mut()[i].anim = 1.f;  // abierta, sin animación
    if (ents[i].type == "ability_double_jump") ab |= kAbilityDoubleJump;
    if (ents[i].type == "ability_dash") ab |= kAbilityDash;
  }
  abilities_ = ab;
  for (auto& b : bosses_)
    if (b.ent < active.size() && active[b.ent]) b.defeated = true;
  for (auto& d : scene_.dialogs_mut())
    if (d.once && d.entity < active.size() && active[d.entity]) d.triggered = true;
  ckpt_ = (checkpoint >= 0 && std::size_t(checkpoint) < ents.size() && ents[std::size_t(checkpoint)].type == "checkpoint")
              ? checkpoint : -1;
  ckpt_flip_ = ckpt_ >= 0 && flipped;
  respawn();
  if (bosses_defeated() && !bosses_.empty()) {
    victory_.mark_done();
    boss_burst_ = 0;
  }
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
  double_jump_flash_ = 0;
  dash_ticks_ = 0;
  dash_cd_ = 0;
  dash_flash_ = 0;
  wall_break_flash_ = 0;
  boss_burst_ = 0;
  regrab_cd_ = 0;
  climb_acc_ = 0;
  anim_ = PlayerAnim::Idle;
  anim_frame_ = 0;
  anim_ticks_ = 0;
  air_ticks_ = 0;
  was_grounded_ = false;
  wind_.reset();
  pending_dialog_ = -1;
  air_jumps_ = has_ability(kAbilityDoubleJump) ? 1 : 0;
  dash_ready_ = has_ability(kAbilityDash);
  reset_movers();
  reset_bosses(true);  // el boss se cura y vuelve a su lugar (si ya estaba vencido, queda así)
  if (bosses_defeated() && !bosses_.empty()) victory_.mark_done();
  else victory_.reset();
  reset_imps();        // los enemigos menores vuelven al reaparecer
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
  victory_.step(kDt);
  wind_.step(kDt);
  boss_burst_ = victory_.in_defeat_anim() ? std::max(1, int(std::ceil(victory_.anim_remaining() / kDt))) : 0;
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
    update_player_anim({});
    was_grounded_ = p.grounded;
    return;
  }
  auto& pts = world_.points();
  if (regrab_cd_ > 0) --regrab_cd_;
  if (grab_flash_ > 0) --grab_flash_;
  if (pickup_flash_ > 0) --pickup_flash_;
  if (double_jump_flash_ > 0) --double_jump_flash_;
  if (ability_flash_ > 0) --ability_flash_;
  if (dash_cd_ > 0) --dash_cd_;
  if (dash_flash_ > 0) --dash_flash_;
  if (wall_break_flash_ > 0) --wall_break_flash_;
  if (dash_ticks_ > 0) {
    --dash_ticks_;
    if (dash_ticks_ == 0) {
      pts[player_].gscale = flip_ ? -1.f : 1.f;
      Vec2 dv = pts[player_].velocity();
      dv.x = std::clamp(dv.x, -kWalkSpeed * kDt, kWalkSpeed * kDt);
      pts[player_].set_velocity(dv);
    }
  }
  // Agarrar: nodo más cercano dentro de kGrabRange.
  if (in.grab && !held_ && regrab_cd_ == 0 && dash_ticks_ == 0) {
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

  bool rope_jumped = false;
  if (held_) {
    // Colgado: recarga doble salto y embestida; ←/→ empujan el nodo, ↑/↓ trepan, saltar o dash sueltan.
    if (has_ability(kAbilityDoubleJump)) air_jumps_ = 1;
    if (has_ability(kAbilityDash)) dash_ready_ = true;
    auto& node = pts[held_index()];
    node.force += Vec2{std::clamp(in.move_x, -1.f, 1.f) * kSwingForce / kHeldInvMass, 0};
    const float seg = ropes_[std::size_t(held_rope_)].seg;
    const int rn = ropes_[std::size_t(held_rope_)].n, hr = held_rope_;
    climb_acc_ += std::clamp(in.climb, -1.f, 1.f) * kClimbSpeed * kDt;
    if (climb_acc_ <= -seg && held_ > 1) { climb_acc_ = 0; int n = held_ - 1; release(); regrab_cd_ = 0; grab(hr, n, false); }
    else if (climb_acc_ >= seg && held_ < rn) { climb_acc_ = 0; int n = held_ + 1; release(); regrab_cd_ = 0; grab(hr, n, false); }
    else climb_acc_ = std::clamp(climb_acc_, -seg, seg);
    if (in.dash && has_ability(kAbilityDash) && dash_ticks_ == 0 && dash_cd_ == 0 && dash_ready_) {
      release();
    } else if (in.jump) {
      const Vec2 v = pts[held_index()].velocity();
      release();
      const float d = down();  // el salto empuja contra la gravedad del player
      const float vy = d > 0 ? std::min(v.y, 0.f) : std::max(v.y, 0.f);
      pts[player_].set_velocity({v.x * 1.2f, vy - d * kJumpSpeed * 0.8f * kDt});
      sfx_ |= 1u;  // kSfxJump
      rope_jumped = true;
    }
  }

  auto& pl = pts[player_];
  if (!held_) {
    // Interacción (switch / diálogo / puerta unidireccional / ascensor) tiene prioridad sobre el Dash.
    bool interacted = false;
    if (in.interact) {
      if (scene_.interact(pl.pos.x, pl.pos.y) >= 0) {
        sfx_ |= ew::audio::kSfxSwitch;
        interacted = true;
      } else if (const int dlg = nearby_interact_dialog(); dlg >= 0) {
        auto& d = scene_.dialogs_mut()[std::size_t(dlg)];
        d.triggered = true;
        const auto& ent = scene_.entities()[d.entity];
        if (ent.type == "npc" || ent.type == "lore") scene_.set_active(d.entity, true);
        pending_dialog_ = dlg;
        sfx_ |= ew::audio::kSfxSwitch;
        interacted = true;
      } else if (const int ow = scene_.try_open_one_way(pl.pos.x, pl.pos.y, false); ow >= 0) {
        sfx_ |= ew::audio::kSfxSwitch;
        interacted = true;
      } else if (ow == -2) {
        sfx_ |= ew::audio::kSfxHit;
        interacted = true;
      } else if (call_elevator(pl.pos.x, pl.pos.y)) {
        sfx_ |= ew::audio::kSfxSwitch;
        interacted = true;
      }
    }
    if (in.dash && !interacted && has_ability(kAbilityDash) && dash_ticks_ == 0 && dash_cd_ == 0 && dash_ready_) {
      dash_dir_ = std::fabs(in.move_x) > 0.1f ? (in.move_x > 0.f ? 1 : -1) : facing_;
      facing_ = dash_dir_;
      dash_ticks_ = kDashFrames;
      dash_cd_ = kDashCooldownFrames;
      dash_ready_ = false;
      dash_flash_ = kDashFlashFrames;
      sfx_ |= 256u;  // kSfxDash
    }

    if (dash_ticks_ > 0) {
      break_walls_on_dash();
      pl.gscale = 0.f;
      pl.set_velocity({float(dash_dir_) * kDashSpeed * kDt, 0.f});
    } else {
      // Control normal: caminar, salto apoyado o doble salto aéreo.
      Vec2 v = pl.velocity();
      const float target = std::clamp(in.move_x, -1.f, 1.f) * kWalkSpeed * kDt;
      v.x += (target - v.x) * (pl.grounded ? 0.5f : 0.15f);
      const bool swim = player_in_water();
      if (pl.grounded || swim) {
        if (has_ability(kAbilityDoubleJump)) air_jumps_ = 1;
        if (has_ability(kAbilityDash)) dash_ready_ = true;
      }
      if (in.jump && !rope_jumped) {
        if (pl.grounded || swim) {
          v.y = -down() * kJumpSpeed * (swim ? 0.8f : 1.f) * kDt;
          sfx_ |= 1u;  // kSfxJump (audio/sfx.hpp)
        } else if (has_ability(kAbilityDoubleJump) && air_jumps_ > 0) {
          --air_jumps_;
          v.y = -down() * kDoubleJumpSpeed * kDt;
          double_jump_flash_ = kDoubleJumpFlashFrames;
          double_jump_x_ = pl.pos.x;
          double_jump_y_ = pl.pos.y + down() * kPlayerRadius;
          sfx_ |= 1u;  // kSfxJump
        }
      }
      pl.set_velocity(v);
    }
  }
  if (scene_.try_open_one_way(pts[player_].pos.x, pts[player_].pos.y, true) >= 0) {
    sfx_ |= ew::audio::kSfxSwitch;
  }
  apply_flood();
  elapsed_sec_ += kDt;
  for (auto& w : water_) { w.interact(world_); w.step(); }
  advance_movers();
  advance_bosses();
  advance_imps();
  carry_player();
  if (wind_.enabled()) {
    for (int r = 0; r < rope_count(); ++r) {
      const int n = ropes_[std::size_t(r)].n;
      const uint32_t first = ropes_[std::size_t(r)].first;
      for (int i = 1; i <= n; ++i) {
        if (held_ && held_rope_ == r && i == held_) continue;
        auto& pt = pts[first + uint32_t(i)];
        if (pt.inv_mass > 0.f) pt.force.x += wind_.sample_at(pt.pos.x, pt.pos.y, 1u) * 24.f;
      }
    }
  }
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
  if (!held_ && world_.points()[player_].grounded) {
    if (has_ability(kAbilityDoubleJump)) air_jumps_ = 1;
    if (dash_ticks_ == 0 && has_ability(kAbilityDash)) dash_ready_ = true;
  }
  scene_.follow(world_.points()[player_].pos.x, world_.points()[player_].pos.y, kDt);
  scene_.tick(kDt);
  const bool in_w = player_in_water();
  if (in_w && !was_in_water_) sfx_ |= 32u;  // kSfxSplash
  if (in_w && !held_) {
    if (has_ability(kAbilityDoubleJump)) air_jumps_ = 1;
    if (dash_ticks_ == 0 && has_ability(kAbilityDash)) dash_ready_ = true;
  }
  was_in_water_ = in_w;
  check_gravity_flips();
  check_checkpoints();
  check_collectibles();
  check_abilities();
  check_dialogs_proximity();
  collide_bosses();
  collide_imps();
  check_hazards();
  check_goal();
  update_player_anim(in);
  {
    const auto& p = world_.points()[player_];
    if (wind_.enabled()) {
      if (p.grounded || air_ticks_ < 6) {
        wind_.set_wake(p.pos.x, p.pos.y + down() * kPlayerRadius, (p.pos.x - p.prev.x) / kDt);
      } else {
        wind_.clear_wake();
      }
      if (!was_grounded_ && p.grounded) {
        wind_.push_impulse(p.pos.x, p.pos.y + down() * kPlayerRadius,
                           (p.pos.x >= p.prev.x ? 1.f : -1.f) * 2.2f, 24.f, 0.38f);
      }
      if (dash_ticks_ > 0) {
        wind_.push_impulse(p.pos.x, p.pos.y, float(dash_dir_) * 3.8f, 30.f, 0.25f);
      }
    }
    was_grounded_ = p.grounded;
  }
}

void Sim::update_player_anim(const Input& in) {
  if (in.move_x > 0.05f) facing_ = 1;
  else if (in.move_x < -0.05f) facing_ = -1;

  const bool gr = player().grounded;
  air_ticks_ = (gr || on_rope() || player_in_water()) ? 0 : (air_ticks_ + 1);
  const float vy_rel = player().velocity().y * down();
  const bool in_air_anim = !gr && (anim_ == PlayerAnim::Jump || anim_ == PlayerAnim::DoubleJump ||
                                   std::fabs(vy_rel) > 0.35f || air_ticks_ > 4);

  if (on_rope()) {
    if (anim_ != PlayerAnim::Climb) { anim_ = PlayerAnim::Climb; anim_ticks_ = 0; }
    if (std::fabs(in.climb) > 0.05f || std::fabs(in.move_x) > 0.05f) ++anim_ticks_;
    anim_frame_ = (anim_ticks_ / 6) & 3;
  } else if (player_in_water()) {
    if (anim_ != PlayerAnim::Swim) { anim_ = PlayerAnim::Swim; anim_ticks_ = 0; }
    ++anim_ticks_;
    anim_frame_ = (anim_ticks_ / 8) & 3;
  } else if (double_jump_flash_ > 0) {
    anim_ = PlayerAnim::DoubleJump;
    const int elapsed = std::clamp(kDoubleJumpFlashFrames - double_jump_flash_, 0, kDoubleJumpFlashFrames - 1);
    anim_frame_ = std::clamp((elapsed * 4) / kDoubleJumpFlashFrames, 0, 3);
  } else if (in_air_anim) {
    if (anim_ != PlayerAnim::Jump) { anim_ = PlayerAnim::Jump; anim_ticks_ = 0; }
    ++anim_ticks_;
    anim_frame_ = (vy_rel < -0.15f) ? ((anim_ticks_ / 8) & 1) : (2 + ((anim_ticks_ / 8) & 1));
  } else if (std::fabs(in.move_x) > 0.05f || std::fabs(player().velocity().x) > 0.25f) {
    if (anim_ != PlayerAnim::Walk) { anim_ = PlayerAnim::Walk; anim_ticks_ = 0; }
    ++anim_ticks_;
    anim_frame_ = (anim_ticks_ / 6) & 3;
  } else {
    if (anim_ != PlayerAnim::Idle) { anim_ = PlayerAnim::Idle; anim_ticks_ = 0; }
    ++anim_ticks_;
    anim_frame_ = (anim_ticks_ / 16) & 3;
  }
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
    trigger_entity_dialog(i, ew::map::DialogTrigger::Pickup);
  }
}

void Sim::check_abilities() {
  const auto& p = player();
  const auto& ents = scene_.entities();
  const float r2 = kAbilityReach * kAbilityReach;
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].active) continue;
    uint32_t grant = 0;
    if (ents[i].type == "ability_double_jump") grant = kAbilityDoubleJump;
    else if (ents[i].type == "ability_dash") grant = kAbilityDash;
    else continue;
    const float dx = ents[i].x - p.pos.x, dy = ents[i].y - p.pos.y;
    if (dx * dx + dy * dy > r2) continue;
    scene_.set_active(i, true);  // desbloqueada: persiste hasta reset()
    abilities_ |= grant;
    if (grant & kAbilityDoubleJump) air_jumps_ = 1;
    if (grant & kAbilityDash) dash_ready_ = true;
    ability_flash_ = kAbilityFlashFrames;
    sfx_ |= 128u;  // kSfxAbility
    trigger_entity_dialog(i, ew::map::DialogTrigger::Pickup);
  }
}

void Sim::trigger_entity_dialog(std::size_t entity_idx, ew::map::DialogTrigger trig) {
  auto& dlgs = scene_.dialogs_mut();
  for (std::size_t k = 0; k < dlgs.size(); ++k) {
    auto& d = dlgs[k];
    if (d.entity != entity_idx || d.trigger != trig) continue;
    if (d.once && d.triggered) continue;
    d.triggered = true;
    pending_dialog_ = int(k);
    return;
  }
}

int Sim::nearby_interact_dialog() const {
  const auto& p = player().pos;
  const auto& ents = scene_.entities();
  const auto& dlgs = scene_.dialogs();
  int best = -1;
  float best_d2 = kDialogReach * kDialogReach;
  for (std::size_t k = 0; k < dlgs.size(); ++k) {
    const auto& d = dlgs[k];
    if (d.trigger != ew::map::DialogTrigger::Interact) continue;
    if (d.once && d.triggered) continue;
    if (d.entity >= ents.size()) continue;
    const auto& e = ents[d.entity];
    const float dx = e.x - p.x, dy = e.y - p.y, d2 = dx * dx + dy * dy;
    if (d2 <= best_d2) { best_d2 = d2; best = int(k); }
  }
  return best;
}

void Sim::check_dialogs_proximity() {
  const auto& p = player().pos;
  const auto& ents = scene_.entities();
  auto& dlgs = scene_.dialogs_mut();
  const float r2 = kDialogReach * kDialogReach;
  for (std::size_t k = 0; k < dlgs.size(); ++k) {
    auto& d = dlgs[k];
    if (d.trigger != ew::map::DialogTrigger::Proximity || d.triggered) continue;
    if (d.entity >= ents.size()) continue;
    const auto& e = ents[d.entity];
    const float dx = e.x - p.x, dy = e.y - p.y;
    if (dx * dx + dy * dy <= r2) {
      d.triggered = true;
      pending_dialog_ = int(k);
      return;
    }
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
    if (w.x() < 400.f || w.x() >= 1280.f || w.initial_rest_y() >= 184.f) continue;  // solo la cisterna del Pozo se inunda
    const float tgt = on ? kFloodRestY : w.initial_rest_y();
    const float y = w.rest_y();
    const float dy = tgt - y;
    if (std::fabs(dy) < 0.05f) { w.set_rest_y(tgt); continue; }
    const float step = kFloodSpeed * kDt;
    w.set_rest_y(y + std::copysign(std::min(step, std::fabs(dy)), dy));
  }
}

bool Sim::call_elevator(float px, float py) {
  const auto& ents = scene_.entities();
  for (auto& m : movers_) {
    if (!m.is_elevator || m.wp.size() < 2) continue;
    if (m.ent >= ents.size() || !ents[m.ent].active) continue;
    if (std::fabs(px - (m.x + m.w * 0.5f)) > 28.f) continue;
    const float d0 = std::fabs(py - m.wp[0].y);
    const float d1 = std::fabs(py - m.wp[1].y);
    if (std::min(d0, d1) > 32.f) continue;
    const int closest = (d0 <= d1) ? 0 : 1;
    if (m.elev_target == closest && m.x == m.wp[std::size_t(closest)].x &&
        m.y == m.wp[std::size_t(closest)].y)
      continue;
    m.elev_target = closest;
    m.elev_wait = 0.f;
    return true;
  }
  return false;
}

void Sim::advance_movers() {
  using ew::physics::length;
  for (std::size_t i = 0; i < movers_.size(); ++i) {
    auto& m = movers_[i];
    m.prev_x = m.x;
    m.prev_y = m.y;
    if (m.wp.size() < 2 || !(m.speed > 0.f)) continue;
    if (m.is_elevator) {
      if (m.ent >= scene_.entities().size() || !scene_.entities()[m.ent].active) {
        m.x = m.wp[0].x;
        m.y = m.wp[0].y;
        m.elev_at = m.elev_target = 0;
        m.elev_wait = 0.f;
        continue;
      }
      const auto& tgt = m.wp[std::size_t(m.elev_target)];
      if (m.x == tgt.x && m.y == tgt.y) {
        m.elev_at = m.elev_target;
        if (m.elev_wait > 0.f) {
          m.elev_wait = std::max(0.f, m.elev_wait - kDt);
        } else if (ridden_ == int(i)) {
          m.elev_target = 1 - m.elev_at;
        }
      }
      const auto& dst = m.wp[std::size_t(m.elev_target)];
      if (m.x != dst.x || m.y != dst.y) {
        const float dx = dst.x - m.x, dy = dst.y - m.y;
        const float dist = std::hypot(dx, dy);
        const float step = m.speed * kDt;
        if (dist <= step) {
          m.x = dst.x;
          m.y = dst.y;
          m.elev_at = m.elev_target;
          m.elev_wait = std::max(m.pause, 0.65f);
        } else {
          m.x += (dx / dist) * step;
          m.y += (dy / dist) * step;
        }
      }
      continue;
    }
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

void Sim::break_walls_on_dash() {
  const auto& pl = player();
  const float reach_x = pl.pos.x + float(dash_dir_) * 4.5f;
  const float wall_w = float(kDoorWTiles * ew::map::kTileSize);
  const float wall_h = float(kDoorHTiles * ew::map::kTileSize);
  const auto& ents = scene_.entities();
  for (std::size_t i = 0; i < ents.size(); ++i) {
    if (ents[i].type != "break_wall" || ents[i].active) continue;
    if (circle_hits_aabb({reach_x, pl.pos.y}, kPlayerRadius + 1.5f,
                         ents[i].x, ents[i].y, ents[i].x + wall_w, ents[i].y + wall_h, 0.f)) {
      scene_.set_active(i, true);
      wall_break_flash_ = kWallBreakFlashFrames;
      wall_break_x_ = ents[i].x + wall_w * 0.5f;
      wall_break_y_ = ents[i].y + wall_h * 0.5f;
      sfx_ |= 512u;  // kSfxBreak
    }
  }
}

}  // namespace ew::game

namespace ew::game {

void Sim::collide_bosses() {
  if ((bosses_.empty() && fireballs_.empty()) || finished_) return;
  auto& pts = world_.points();
  auto& pl = pts[player_];
  const float r = pl.radius, d = down();
  for (auto& b : bosses_) {
    if (b.defeated) continue;
    const float bw = b.width(), bh = b.height();
    const float l = b.x - bw * 0.5f, rt = b.x + bw * 0.5f, top = b.y - bh, bot = b.y;
    const ew::physics::Vec2 c{std::clamp(pl.pos.x, l, rt), std::clamp(pl.pos.y, top, bot)};
    if (ew::physics::length(pl.pos - c) >= r) continue;
    if (b.hurt > 0) continue;  // recién golpeado: se lo puede atravesar
    // Pisotón: el player va hacia su "abajo" y su centro está del lado de su "arriba" del boss.
    const float vy = pl.velocity().y;
    const bool from_above = d > 0 ? (vy > 0.f && pl.pos.y <= top + 4.f) : (vy < 0.f && pl.pos.y >= bot - 4.f);
    if (from_above) {
      ++b.hits;
      b.hurt = kBossHurtSec;
      sfx_ |= 16u | 1024u;  // kSfxSwitch | kSfxHit (golpe)
      // Rebote: fuera de la caja y con impulso contra su gravedad.
      if (held_) release();
      if (has_ability(kAbilityDoubleJump)) air_jumps_ = 1;
      if (has_ability(kAbilityDash)) dash_ready_ = true;
      pl.pos.y = d > 0 ? top - r : bot + r;
      pl.set_velocity({pl.velocity().x, -d * kJumpSpeed * 0.9f * kDt});
      if (b.hits >= b.max_hits()) {
        b.defeated = true;
        fireballs_.clear();
        scene_.set_linked(b.ent, true);  // abre las puertas linkeadas
        const float cy = b.y - bh * 0.5f;
        const BossKind kind = b.dragon ? BossKind::Wyrm : (b.guardian2 ? BossKind::Guardian2 : BossKind::Watcher);
        victory_.start(b.x, cy, kind);
        boss_burst_x_ = b.x;
        boss_burst_y_ = cy;
        boss_burst_ = victory_.in_defeat_anim() ? std::max(1, int(std::ceil(victory_.anim_remaining() / kDt))) : 0;
        wind_.push_impulse(b.x, b.y, 3.6f, 44.f, 0.55f);
        sfx_ |= 4u | 2048u;  // kSfxCheckpoint | kSfxBossCrt (rayos catódicos)
      }
      return;
    }
    if (invuln_ > 0) continue;
    kill();
    return;
  }
  // Bolas de fuego de los bosses: hacen daño salvo durante invuln o i-frames de embestida (Dash).
  if (invuln_ == 0 && dash_ticks_ == 0) {
    const float hit_r = r + kFireballRadius;
    for (const auto& fb : fireballs_) {
      const float dx = fb.x - pl.pos.x, dy = fb.y - pl.pos.y;
      if (dx * dx + dy * dy < hit_r * hit_r) { kill(); return; }
    }
  }
}

void Sim::advance_imps() {
  for (auto& im : imps_) {
    if (im.defeated) continue;
    im.x += float(im.dir) * kImpSpeed * kDt;
    if (im.x > im.home_x + kImpRange) { im.x = im.home_x + kImpRange; im.dir = -1; }
    if (im.x < im.home_x - kImpRange) { im.x = im.home_x - kImpRange; im.dir = 1; }
  }
}

void Sim::collide_imps() {
  if (imps_.empty() || finished_) return;
  auto& pts = world_.points();
  auto& pl = pts[player_];
  const float r = pl.radius, d = down();
  for (auto& im : imps_) {
    if (im.defeated) continue;
    const float l = im.x - kImpW * 0.5f, rt = im.x + kImpW * 0.5f, top = im.y - kImpH, bot = im.y;
    const ew::physics::Vec2 c{std::clamp(pl.pos.x, l, rt), std::clamp(pl.pos.y, top, bot)};
    if (ew::physics::length(pl.pos - c) >= r) continue;
    const float vy = pl.velocity().y;
    const bool from_above = d > 0 ? (vy > 0.f && pl.pos.y <= top + 4.f) : (vy < 0.f && pl.pos.y >= bot - 4.f);
    if (dash_ticks_ > 0 || from_above) {
      im.defeated = true;
      wind_.push_impulse(im.x, im.y, float(im.dir) * 2.0f, 22.f, 0.35f);
      sfx_ |= 1024u;  // kSfxHit
      if (has_ability(kAbilityDoubleJump)) air_jumps_ = 1;
      if (has_ability(kAbilityDash)) dash_ready_ = true;
      if (dash_ticks_ == 0 && from_above) {
        pl.pos.y = d > 0 ? top - r : bot + r;
        pl.set_velocity({pl.velocity().x, -d * kJumpSpeed * 0.85f * kDt});
      }
      return;
    }
    if (invuln_ > 0) continue;
    kill();
    return;
  }
}

}  // namespace ew::game
