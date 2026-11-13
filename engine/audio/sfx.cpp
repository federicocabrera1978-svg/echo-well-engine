#include "audio/sfx.hpp"
#include <cmath>
#include <algorithm>

namespace ew::audio {
namespace {
struct Spec { float freq, dur, amp; int kind; };
Spec spec(SfxId id) {
  switch (id) {
    case SfxId::Jump:       return {660.f,  0.09f, 0.35f, 0};
    case SfxId::Die:        return {140.f,  0.28f, 0.40f, 0};
    case SfxId::Checkpoint: return {880.f,  0.16f, 0.32f, 1};
    case SfxId::Goal:       return {523.f,  0.45f, 0.40f, 1};
    case SfxId::Switch:     return {990.f,  0.07f, 0.28f, 0};
    case SfxId::Splash:     return {220.f,  0.12f, 0.22f, 2};
    default:                return {440.f,  0.10f, 0.25f, 0};
  }
}
}  // namespace

void Mixer::play(SfxId id) {
  const auto s = spec(id);
  int slot = -1;
  for (int i = 0; i < kMaxVoices; ++i)
    if (!v_[i].on) { slot = i; break; }
  if (slot < 0) {  // reemplaza la más vieja
    slot = 0;
    for (int i = 1; i < kMaxVoices; ++i)
      if (v_[i].t > v_[slot].t) slot = i;
  }
  auto& v = v_[slot];
  v.on = true;
  v.t = 0;
  v.dur = s.dur;
  v.freq = s.freq;
  v.amp = s.amp;
  v.kind = s.kind;
  int on = 0;
  for (int i = 0; i < kMaxVoices; ++i) if (v_[i].on) ++on;
  active_ = on;
}

void Mixer::render(float* out, int frames, int rate) {
  if (!out || frames <= 0) return;
  const float dt = 1.f / float(std::max(1, rate));
  for (int n = 0; n < frames; ++n) {
    float acc = 0;
    int on = 0;
    for (int i = 0; i < kMaxVoices; ++i) {
      auto& v = v_[i];
      if (!v.on) continue;
      const float env = (v.t < 0.012f) ? (v.t / 0.012f)
                      : std::max(0.f, 1.f - (v.t - 0.012f) / std::max(0.001f, v.dur - 0.012f));
      const float ph = 6.28318530718f * v.freq * v.t;
      float s = std::sin(ph);
      if (v.kind == 1) s = 0.65f * s + 0.35f * std::sin(2.f * ph);
      if (v.kind == 2) s = s * 0.4f + ((v.t * 97.f) - std::floor(v.t * 97.f) - 0.5f) * 0.6f;
      acc += v.amp * env * s;
      v.t += dt;
      if (v.t >= v.dur) v.on = false;
      else ++on;
    }
    out[n] = std::clamp(acc, -1.f, 1.f);
    active_ = on;
  }
}

}  // namespace ew::audio
