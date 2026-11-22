#pragma once
// SFX procedural (sin dispositivo): beep de seno + envolvente. Testeable y headless.
// La reproducción real (miniaudio) es opcional, solo en echo_engine, y falla en silencio
// si no hay dispositivo o se pasa --nosound / EW_NOSOUND.
#include <atomic>
#include <cstdint>

namespace ew::audio {

enum class SfxId : uint8_t {
  Jump,
  Die,
  Checkpoint,
  Goal,
  Switch,
  Splash,
  Pickup,
  Ability,
  Dash,
  Break,
  Hit,
  BossCrt,
  StepStone,
  StepMoss,
  StepMetal,
  StepWater,
  Count
};

// Zonas musicales procedurales (Silence por defecto para no alterar renders sin música).
enum class MusicZone : uint8_t { Silence = 0, Well = 1, Catacombs = 2, WyrmLair = 3, Boss = 4 };

inline constexpr int kRate = 44100;
inline constexpr int kMaxVoices = 8;

struct Mixer {
  void play(SfxId id);
  // Mezcla `frames` muestras mono f32 (–1..1) a `out`.
  void render(float* out, int frames, int rate = kRate);
  int active() const { return active_; }
  // Ganancia 0..1 (el menú de pausa la aplica). Mute anula el audio sin perder el valor.
  void set_gain(float g) { gain_.store(g < 0.f ? 0.f : (g > 1.f ? 1.f : g)); }
  void set_mute(bool m) { mute_.store(m); }
  float gain() const { return mute_.load() ? 0.f : gain_.load(); }
  // Música ambiental procedural por zona con crossfade (~1.5 s).
  void set_music_zone(MusicZone z);
  MusicZone music_zone() const { return cur_zone_.load(std::memory_order_relaxed); }
  float music_fade() const { return xfade_.load(std::memory_order_relaxed); }
  void set_music_enabled(bool e) { music_enabled_.store(e, std::memory_order_relaxed); }
  bool music_enabled() const { return music_enabled_.load(std::memory_order_relaxed); }

 private:
  struct Voice {
    bool on = false;
    float t = 0, dur = 0, freq = 0, amp = 0;
    int kind = 0;  // 0 seno, 1 seno+octava, 2 ruido corto, 3 dash, 4 boss CRT, 5..8 pasos
  };
  Voice v_[kMaxVoices]{};
  int active_ = 0;
  std::atomic<float> gain_{0.80f};
  std::atomic<bool> mute_{false};
  std::atomic<MusicZone> cur_zone_{MusicZone::Silence};
  std::atomic<MusicZone> prev_zone_{MusicZone::Silence};
  std::atomic<float> xfade_{1.f};
  std::atomic<bool> music_enabled_{true};
  double music_t_ = 0.0;
};

// Eventos que Sim acumula en un paso (bits).
inline constexpr uint32_t kSfxJump = 1u << 0;
inline constexpr uint32_t kSfxDie = 1u << 1;
inline constexpr uint32_t kSfxCheckpoint = 1u << 2;
inline constexpr uint32_t kSfxGoal = 1u << 3;
inline constexpr uint32_t kSfxSwitch = 1u << 4;
inline constexpr uint32_t kSfxSplash = 1u << 5;
inline constexpr uint32_t kSfxPickup = 1u << 6;
inline constexpr uint32_t kSfxAbility = 1u << 7;
inline constexpr uint32_t kSfxDash = 1u << 8;
inline constexpr uint32_t kSfxBreak = 1u << 9;
inline constexpr uint32_t kSfxHit = 1u << 10;
inline constexpr uint32_t kSfxBossCrt = 1u << 11;
inline constexpr uint32_t kSfxStepStone = 1u << 12;
inline constexpr uint32_t kSfxStepMoss = 1u << 13;
inline constexpr uint32_t kSfxStepMetal = 1u << 14;
inline constexpr uint32_t kSfxStepWater = 1u << 15;

}  // namespace ew::audio
