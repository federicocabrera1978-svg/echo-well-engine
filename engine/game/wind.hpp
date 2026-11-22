#pragma once
// Campo de brisa global y estelas/ráfagas locales (sin dependencias de GL/GLFW).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ew::game {

struct WindImpulse {
  float x = 0.f, y = 0.f, push = 0.f, radius = 20.f, life = 0.f, max_life = 0.35f;
};

namespace detail {
inline float wind_hash_phase(uint32_t seed, uint32_t salt) {
  uint32_t h = (seed ^ salt) * 2654435761u;
  h ^= h >> 16;
  return float(h & 0xFFFFu) * (6.28318530718f / 65536.f);
}
}  // namespace detail

// Factor lento de ráfaga determinista según `seed`, en [0.35f, 1.85f].
inline float wind_gust(float time_s, uint32_t seed) {
  const float p1 = detail::wind_hash_phase(seed, 0x9E3779B9u);
  const float p2 = detail::wind_hash_phase(seed, 0x85EBCA6Bu);
  const float p3 = detail::wind_hash_phase(seed, 0xC2B2AE35u);
  const float wave = 0.50f * std::sin(time_s * 0.43f + p1) +
                     0.32f * std::sin(time_s * 0.91f + p2) +
                     0.18f * std::sin(time_s * 0.19f + p3);
  const float u = 0.5f + 0.5f * wave;
  return std::clamp(0.35f + 1.50f * u, 0.35f, 1.85f);
}

// Onda viajera horizontal compuesta por dos senoidales deterministas según `seed`.
inline float wind_wave(float world_x, float time_s, uint32_t seed) {
  const float p1 = detail::wind_hash_phase(seed, 0x27D4EB2Fu);
  const float p2 = detail::wind_hash_phase(seed, 0x165667B1u);
  const float w1 = std::sin(0.055f * world_x - 2.15f * time_s + p1);
  const float w2 = std::sin(0.115f * world_x - 3.45f * time_s + p2);
  return 0.68f * w1 + 0.32f * w2;
}

// Muestreo combinado de brisa global en `world_x` y `time_s`.
inline float wind_sample(float world_x, float time_s, uint32_t seed, bool enabled = true) {
  if (!enabled) return 0.f;
  return wind_wave(world_x, time_s, seed) * wind_gust(time_s, seed);
}

class WindField {
 public:
  static constexpr int kMaxImpulses = 4;

  bool enabled() const { return enabled_; }
  void set_enabled(bool e) {
    enabled_ = e;
    if (!enabled_) impulses_.fill(WindImpulse{});
  }

  void reset() {
    time_s_ = 0.f;
    impulses_.fill(WindImpulse{});
  }

  void set_seed(uint32_t s) { seed_ = s; }
  uint32_t seed() const { return seed_; }
  float time_s() const { return time_s_; }

  // Slot 0: estela continua al caminar/pasar cerca del suelo.
  void set_wake(float x, float y, float vx_px_s, float radius = 16.f) {
    if (!enabled_ || radius <= 0.f) {
      impulses_[0] = WindImpulse{};
      return;
    }
    auto& imp = impulses_[0];
    imp.x = x;
    imp.y = y;
    imp.push = std::clamp(vx_px_s / 90.f, -2.5f, 2.5f);
    imp.radius = radius;
    imp.life = 1.f;
    imp.max_life = 1.f;
  }

  void clear_wake() { impulses_[0] = WindImpulse{}; }

  // Slots 1..3: ráfagas locales (aterrizaje fuerte, Dash, golpe/salto de enemigo).
  void push_impulse(float x, float y, float push_dir_strength, float radius, float duration) {
    if (!enabled_ || radius <= 0.f || duration <= 0.f) return;
    int slot = -1;
    float min_life = 1e9f;
    for (int i = 1; i < kMaxImpulses; ++i) {
      if (impulses_[std::size_t(i)].life <= 0.f) {
        slot = i;
        break;
      }
      if (impulses_[std::size_t(i)].life < min_life) {
        min_life = impulses_[std::size_t(i)].life;
        slot = i;
      }
    }
    if (slot < 1) return;
    auto& imp = impulses_[std::size_t(slot)];
    imp.x = x;
    imp.y = y;
    imp.push = push_dir_strength;
    imp.radius = radius;
    imp.life = duration;
    imp.max_life = duration;
  }

  // Avanza el tiempo global y decae la vida de las ráfagas locales (slots 1..3).
  void step(float dt) {
    if (dt <= 0.f) return;
    time_s_ += dt;
    for (int i = 1; i < kMaxImpulses; ++i) {
      auto& imp = impulses_[std::size_t(i)];
      if (imp.life > 0.f) imp.life = std::max(0.f, imp.life - dt);
    }
  }

  float sample_at(float world_x, float world_y, uint32_t chunk_seed) const {
    if (!enabled_) return 0.f;
    float total = wind_sample(world_x, time_s_, chunk_seed, true);
    for (int i = 0; i < kMaxImpulses; ++i) {
      const auto& imp = impulses_[std::size_t(i)];
      if (imp.life <= 0.f || imp.radius <= 0.f) continue;
      const float dx = world_x - imp.x;
      const float dy = world_y - imp.y;
      const float d2 = dx * dx + dy * dy;
      const float r2 = imp.radius * imp.radius;
      if (d2 >= r2) continue;
      const float dist = std::sqrt(d2);
      const float falloff = 1.f - dist / imp.radius;
      const float env = std::clamp(imp.life / std::max(imp.max_life, 1e-4f), 0.f, 1.f);
      const float part = (dx / imp.radius) * 0.45f;
      total += (imp.push + part) * falloff * env;
    }
    return total;
  }

  float current_gust(uint32_t chunk_seed) const {
    return enabled_ ? wind_gust(time_s_, chunk_seed) : 0.f;
  }

  int pack_impulses(std::array<float, 16>& out) const {
    out.fill(0.f);
    if (!enabled_) return 0;
    int count = 0;
    for (int i = 0; i < kMaxImpulses; ++i) {
      const auto& imp = impulses_[std::size_t(i)];
      if (imp.life <= 0.f || imp.radius <= 0.f) continue;
      const float env = std::clamp(imp.life / std::max(imp.max_life, 1e-4f), 0.f, 1.f);
      const std::size_t base = std::size_t(count) * 4;
      out[base + 0] = imp.x;
      out[base + 1] = imp.y;
      out[base + 2] = imp.push * env;
      out[base + 3] = imp.radius;
      ++count;
    }
    return count;
  }

 private:
  bool enabled_ = true;
  uint32_t seed_ = 1u;
  float time_s_ = 0.f;
  std::array<WindImpulse, kMaxImpulses> impulses_{};
};

}  // namespace ew::game
