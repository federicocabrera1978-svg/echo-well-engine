#pragma once
// Secuencia de victoria al derrotar un boss (sin GL/GLFW):
// explosión CRT + partículas + shake + slow-motion + pausa dramática + diálogo.
#include "game/story.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ew::game {

struct BossVictoryConfig {
  float anim_sec = 3.0f;       // duración de desintegración CRT (2.5–3.5 s configurable)
  float pause_sec = 0.75f;     // pausa dramática antes del diálogo
  float slow_mo_scale = 0.55f; // escala temporal durante AftermathPause
  float shake_amp = 2.5f;      // amplitud inicial de sacudida de cámara (px)
  int particle_count = 14;     // chispas radiales CRT
};

enum class BossKind : uint8_t { Watcher = 0, Guardian2 = 1, Wyrm = 2 };

enum class VictoryPhase : uint8_t {
  Idle = 0,
  DefeatAnim = 1,
  AftermathPause = 2,
  ReadyForDialog = 3,
  Done = 4,
};

struct VictoryParticle {
  float x = 0, y = 0, vx = 0, vy = 0, radius = 1.5f, r = 1, g = 1, b = 1, life = 0, max_life = 1;
};

struct BossVictoryDialog {
  std::string speaker;
  std::string text;
};

// Diálogo de progresión al derrotar al boss de cada nivel (textos en story.hpp, fuente única).
inline BossVictoryDialog boss_victory_dialog(BossKind kind) {
  switch (kind) {
    case BossKind::Wyrm:
      return {std::string(kVictoryWyrmSpeaker), std::string(kVictoryWyrmText)};
    case BossKind::Guardian2:
      return {std::string(kVictoryGuardian2Speaker), std::string(kVictoryGuardian2Text)};
    case BossKind::Watcher:
    default:
      return {std::string(kVictoryWatcherSpeaker), std::string(kVictoryWatcherText)};
  }
}

// Máquina de estados determinista para la secuencia de victoria de un boss.
class BossVictorySeq {
 public:
  BossVictorySeq() = default;
  explicit BossVictorySeq(const BossVictoryConfig& cfg) : cfg_(cfg) {}

  const BossVictoryConfig& config() const { return cfg_; }
  void set_config(const BossVictoryConfig& cfg) { cfg_ = cfg; }

  // Inicia la secuencia en (x, y) generando partículas deterministas en abanico radial + chispas CRT.
  void start(float x, float y, BossKind kind) {
    phase_ = VictoryPhase::DefeatAnim;
    phase_t_ = 0.f;
    origin_x_ = x;
    origin_y_ = y;
    kind_ = kind;
    spawn_particles(x, y);
  }

  // Avanza partículas (vel + gravedad suave + fade) y transiciones de fase.
  void step(float dt) {
    if (dt <= 0.f) return;
    constexpr float kParticleGravity = 72.f;
    for (auto& p : particles_) {
      if (p.life <= 0.f) continue;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.vy += kParticleGravity * dt;
      p.life = std::max(0.f, p.life - dt);
    }
    if (phase_ == VictoryPhase::DefeatAnim) {
      phase_t_ += dt;
      if (phase_t_ >= cfg_.anim_sec) {
        phase_t_ = 0.f;
        phase_ = (cfg_.pause_sec > 0.f) ? VictoryPhase::AftermathPause : VictoryPhase::ReadyForDialog;
      }
    } else if (phase_ == VictoryPhase::AftermathPause) {
      phase_t_ += dt;
      if (phase_t_ >= cfg_.pause_sec) {
        phase_t_ = 0.f;
        phase_ = VictoryPhase::ReadyForDialog;
      }
    }
  }

  // Devuelve true únicamente en ReadyForDialog; entrega el diálogo y pasa a Done.
  bool poll_dialog(BossVictoryDialog& out) {
    if (phase_ != VictoryPhase::ReadyForDialog) return false;
    out = boss_victory_dialog(kind_);
    phase_ = VictoryPhase::Done;
    phase_t_ = 0.f;
    return true;
  }

  void mark_done() {
    phase_ = VictoryPhase::Done;
    phase_t_ = 0.f;
    particles_.clear();
  }

  void reset() {
    phase_ = VictoryPhase::Idle;
    phase_t_ = 0.f;
    origin_x_ = 0.f;
    origin_y_ = 0.f;
    kind_ = BossKind::Watcher;
    particles_.clear();
  }

  VictoryPhase phase() const { return phase_; }
  BossKind kind() const { return kind_; }
  bool active() const { return phase_ == VictoryPhase::DefeatAnim || phase_ == VictoryPhase::AftermathPause; }
  bool in_defeat_anim() const { return phase_ == VictoryPhase::DefeatAnim; }
  bool in_aftermath_pause() const { return phase_ == VictoryPhase::AftermathPause; }

  float anim_remaining() const {
    return (phase_ == VictoryPhase::DefeatAnim) ? std::max(0.f, cfg_.anim_sec - phase_t_) : 0.f;
  }

  // Progreso CRT: 1.0 -> 0.0 durante DefeatAnim, 0 en el resto.
  float crt_progress() const {
    if (phase_ != VictoryPhase::DefeatAnim || cfg_.anim_sec <= 0.f) return 0.f;
    return std::clamp(1.f - (phase_t_ / cfg_.anim_sec), 0.f, 1.f);
  }

  // Destello intenso al arranque que decae suavemente + pulso secundario en DefeatAnim (0..1).
  float flash_alpha() const {
    if (phase_ != VictoryPhase::DefeatAnim || cfg_.anim_sec <= 0.f) return 0.f;
    const float u = std::clamp(phase_t_ / cfg_.anim_sec, 0.f, 1.f);
    const float primary = std::exp(-u * 6.f) * (1.f - u);
    const float d = (u - 0.55f) / 0.14f;
    const float tail = std::clamp((1.f - u) * 4.f, 0.f, 1.f);
    const float secondary = 0.45f * std::exp(-d * d) * tail;
    return std::clamp(primary + secondary, 0.f, 1.f);
  }

  float time_scale() const {
    return (phase_ == VictoryPhase::AftermathPause) ? cfg_.slow_mo_scale : 1.f;
  }

  // Sacudida suave determinista amortiguada durante DefeatAnim, 0 en el resto.
  void camera_shake(float& dx, float& dy) const {
    if (phase_ != VictoryPhase::DefeatAnim || cfg_.anim_sec <= 0.f || cfg_.shake_amp <= 0.f) {
      dx = 0.f;
      dy = 0.f;
      return;
    }
    const float u = std::clamp(phase_t_ / cfg_.anim_sec, 0.f, 1.f);
    const float env = (1.f - u) * (1.f - u);
    const float amp = cfg_.shake_amp * env;
    dx = std::sin(phase_t_ * 52.f) * amp;
    dy = std::cos(phase_t_ * 41.f + 0.7f) * amp * 0.75f;
  }

  float origin_x() const { return origin_x_; }
  float origin_y() const { return origin_y_; }
  const std::vector<VictoryParticle>& particles() const { return particles_; }

 private:
  void spawn_particles(float x, float y) {
    particles_.clear();
    const int n = std::max(0, cfg_.particle_count);
    if (n == 0) return;
    particles_.reserve(std::size_t(n));
    // Paleta CRT: cian, magenta, dorado, blanco.
    static constexpr std::array<std::array<float, 3>, 4> kPalette{{
        {0.25f, 0.95f, 1.00f},
        {1.00f, 0.25f, 0.90f},
        {1.00f, 0.85f, 0.25f},
        {1.00f, 1.00f, 1.00f},
    }};
    constexpr float kTwoPi = 6.283185307179586f;
    for (int i = 0; i < n; ++i) {
      const float base_ang = (kTwoPi * float(i)) / float(n);
      const float jitter = 0.14f * std::sin(float(i) * 2.37f);
      const float ang = base_ang + jitter;
      const bool spark = (i % 3 == 0);
      const float speed = spark ? (92.f + 16.f * float(i % 4)) : (48.f + 14.f * float(i % 3));
      const float max_life = spark ? (0.85f + 0.18f * float(i % 3)) : (1.15f + 0.22f * float(i % 4));
      const auto& col = kPalette[std::size_t(i) % kPalette.size()];
      VictoryParticle pt;
      pt.x = x;
      pt.y = y;
      pt.vx = std::cos(ang) * speed;
      pt.vy = std::sin(ang) * speed - 18.f;
      pt.radius = spark ? 1.1f : (1.5f + 0.25f * float(i % 2));
      pt.r = col[0];
      pt.g = col[1];
      pt.b = col[2];
      pt.life = max_life;
      pt.max_life = max_life;
      particles_.push_back(pt);
    }
  }

  BossVictoryConfig cfg_{};
  VictoryPhase phase_ = VictoryPhase::Idle;
  BossKind kind_ = BossKind::Watcher;
  float phase_t_ = 0.f;
  float origin_x_ = 0.f;
  float origin_y_ = 0.f;
  std::vector<VictoryParticle> particles_;
};

}  // namespace ew::game
