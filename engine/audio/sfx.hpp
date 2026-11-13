#pragma once
// SFX procedural (sin dispositivo): beep de seno + envolvente. Testeable y headless.
// La reproducción real (miniaudio) es opcional, solo en echo_engine, y falla en silencio
// si no hay dispositivo o se pasa --nosound / EW_NOSOUND.
#include <atomic>
#include <cstdint>

namespace ew::audio {

enum class SfxId : uint8_t { Jump, Die, Checkpoint, Goal, Switch, Splash, Count };

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

 private:
  struct Voice {
    bool on = false;
    float t = 0, dur = 0, freq = 0, amp = 0;
    int kind = 0;  // 0 seno, 1 seno+octava, 2 ruido corto
  };
  Voice v_[kMaxVoices]{};
  int active_ = 0;
  std::atomic<float> gain_{0.80f};
  std::atomic<bool> mute_{false};
};

// Eventos que Sim acumula en un paso (bits).
inline constexpr uint32_t kSfxJump = 1u << 0;
inline constexpr uint32_t kSfxDie = 1u << 1;
inline constexpr uint32_t kSfxCheckpoint = 1u << 2;
inline constexpr uint32_t kSfxGoal = 1u << 3;
inline constexpr uint32_t kSfxSwitch = 1u << 4;
inline constexpr uint32_t kSfxSplash = 1u << 5;

}  // namespace ew::audio
