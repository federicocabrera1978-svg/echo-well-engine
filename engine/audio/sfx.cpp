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
    case SfxId::Pickup:     return {1320.f, 0.30f, 0.30f, 1};
    case SfxId::Ability:    return {1046.f, 0.36f, 0.34f, 1};
    case SfxId::Dash:       return {760.f,  0.11f, 0.33f, 3};
    case SfxId::Break:      return {175.f,  0.22f, 0.38f, 2};
    case SfxId::Hit:        return {420.f,  0.14f, 0.35f, 1};
    case SfxId::BossCrt:    return {880.f,  0.85f, 0.44f, 4};
    case SfxId::StepStone:  return {260.f,  0.038f, 0.16f, 5};
    case SfxId::StepMoss:   return {115.f,  0.048f, 0.15f, 6};
    case SfxId::StepMetal:  return {780.f,  0.055f, 0.15f, 7};
    case SfxId::StepWater:  return {340.f,  0.058f, 0.17f, 8};
    case SfxId::Warp:       return {392.f,  0.34f,  0.34f, 9};
    default:                return {440.f,  0.10f, 0.25f, 0};
  }
}

constexpr float kTwoPi = 6.28318530718f;
constexpr float kMusicFadeSec = 1.5f;
constexpr float kMusicMixGain = 0.16f;

inline float osc_sin(double t, float freq) {
  const double cyc = t * double(freq);
  const float ph = kTwoPi * float(cyc - std::floor(cyc));
  return std::sin(ph);
}

inline float pseudo_noise(float t, float rate) {
  const float x = t * rate;
  return (x - std::floor(x) - 0.5f) * 2.f;
}

// Envolvente de arpegio sin clics en los bordes de cada nota (0 en u=0 y u=1).
inline float arp_note(double t, double step_dur, int n_notes, int& out_idx) {
  const double pos = t / step_dur;
  const double step_f = std::floor(pos);
  out_idx = int(static_cast<long long>(step_f) % n_notes);
  if (out_idx < 0) out_idx += n_notes;
  const float u = float(pos - step_f);
  const float atk = std::min(1.f, u / 0.06f);
  const float rel = std::min(1.f, (1.f - u) / 0.06f);
  return atk * rel * std::exp(-u * 2.4f);
}

float synth_zone(MusicZone z, double t) {
  switch (z) {
    case MusicZone::Well: {
      // Dórico misterioso (Re): drone cálido + arpegio de gotas/campana.
      static constexpr float kNotes[8] = {293.66f, 349.23f, 440.00f, 523.25f, 440.00f, 329.63f, 392.00f, 440.00f};
      const float breath = 0.72f + 0.28f * osc_sin(t, 0.14f);
      const float drone = 0.34f * osc_sin(t, 73.42f) + 0.20f * osc_sin(t, 110.00f) + 0.14f * osc_sin(t, 146.83f);
      int idx = 0;
      const float env = arp_note(t, 0.625, 8, idx);
      const float f = kNotes[idx];
      const float bell = 0.68f * osc_sin(t, f) + 0.32f * osc_sin(t, f * 2.f);
      return std::clamp(breath * drone + 0.34f * env * bell, -1.f, 1.f);
    }
    case MusicZone::Catacombs: {
      // Frigio oscuro (Do): bordón profundo + campana hueca desafinada.
      static constexpr float kNotes[8] = {261.63f, 277.18f, 392.00f, 415.30f, 392.00f, 311.13f, 277.18f, 261.63f};
      const float sway = 0.70f + 0.30f * osc_sin(t, 0.11f);
      const float drone = 0.36f * osc_sin(t, 65.41f) + 0.22f * osc_sin(t, 98.00f) + 0.12f * osc_sin(t, 138.59f);
      int idx = 0;
      const float env = arp_note(t, 0.75, 8, idx);
      const float f = kNotes[idx];
      const float chime = 0.62f * osc_sin(t, f) + 0.26f * osc_sin(t, f * 1.004f) + 0.12f * osc_sin(t, f * 2.f);
      return std::clamp(sway * drone + 0.32f * env * chime, -1.f, 1.f);
    }
    case MusicZone::WyrmLair: {
      // Tensión dracónica (La): sub-drone pulsante + ostinato grave con armónicos impares.
      static constexpr float kNotes[8] = {220.00f, 233.08f, 329.63f, 349.23f, 329.63f, 277.18f, 233.08f, 220.00f};
      const float pulse = 0.68f + 0.32f * osc_sin(t, 0.45f);
      const float drone = 0.38f * osc_sin(t, 55.00f) + 0.22f * osc_sin(t, 82.41f) + 0.14f * osc_sin(t, 116.54f);
      int idx = 0;
      const float env = arp_note(t, 0.40, 8, idx);
      const float f = kNotes[idx];
      const float lead = 0.72f * osc_sin(t, f) + 0.28f * osc_sin(t, f * 3.f);
      return std::clamp(pulse * drone + 0.30f * env * lead, -1.f, 1.f);
    }
    case MusicZone::Boss: {
      // Combate urgente (Mi menor/frigio): bajo rítmico + arpegio rápido brillante.
      static constexpr float kBass[4] = {82.41f, 98.00f, 87.31f, 82.41f};
      static constexpr float kArp[8] = {329.63f, 392.00f, 493.88f, 659.25f, 587.33f, 523.25f, 493.88f, 349.23f};
      int b_idx = 0;
      const float b_env = arp_note(t, 0.50, 4, b_idx);
      const float gate = 0.62f + 0.38f * osc_sin(t, 6.0f);
      const float bass = (0.42f * b_env + 0.24f) * gate * (0.75f * osc_sin(t, kBass[b_idx]) + 0.25f * osc_sin(t, kBass[b_idx] * 2.f));
      int a_idx = 0;
      const float a_env = arp_note(t, 0.18, 8, a_idx);
      const float f = kArp[a_idx];
      const float arp = 0.60f * osc_sin(t, f) + 0.26f * osc_sin(t, f * 2.f) + 0.14f * osc_sin(t, f * 3.f);
      return std::clamp(bass + 0.36f * a_env * arp, -1.f, 1.f);
    }
    case MusicZone::Aqueduct: {
      // Sumergido (Re lidio-acuático): drone bajo + arpegio de gotas con campana suave.
      static constexpr float kNotes[8] = {293.66f, 329.63f, 392.00f, 440.00f, 493.88f, 440.00f, 392.00f, 329.63f};
      const float swell = 0.70f + 0.30f * osc_sin(t, 0.07f);
      const float drone = 0.34f * osc_sin(t, 73.42f) + 0.20f * osc_sin(t, 110.00f) + 0.12f * osc_sin(t, 146.83f);
      int idx = 0;
      const float env = arp_note(t, 0.85, 8, idx);
      const float f = kNotes[idx];
      const float drop = 0.66f * osc_sin(t, f) + 0.34f * osc_sin(t, f * 1.005f);
      return std::clamp(swell * drone + 0.30f * env * drop, -1.f, 1.f);
    }
    case MusicZone::Tower: {
      // Campanario (Do jónico-eco): repique metálico + bordón grave y armónicos de campana.
      static constexpr float kNotes[8] = {261.63f, 329.63f, 392.00f, 523.25f, 392.00f, 349.23f, 329.63f, 293.66f};
      const float sway = 0.72f + 0.28f * osc_sin(t, 0.10f);
      const float drone = 0.36f * osc_sin(t, 65.41f) + 0.20f * osc_sin(t, 98.00f) + 0.12f * osc_sin(t, 130.81f);
      int idx = 0;
      const float env = arp_note(t, 0.55, 8, idx);
      const float f = kNotes[idx];
      const float bell = 0.62f * osc_sin(t, f) + 0.26f * osc_sin(t, f * 2.76f) + 0.12f * osc_sin(t, f * 3.93f);
      return std::clamp(sway * drone + 0.34f * env * bell, -1.f, 1.f);
    }
    default:
      return 0.f;
  }
}
}  // namespace

void Mixer::set_music_zone(MusicZone z) {
  const MusicZone cur = cur_zone_.load(std::memory_order_relaxed);
  if (z == cur) return;
  prev_zone_.store(cur, std::memory_order_relaxed);
  cur_zone_.store(z, std::memory_order_relaxed);
  xfade_.store(0.f, std::memory_order_relaxed);
}

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
  const float master_gain = mute_.load(std::memory_order_relaxed) ? 0.f : gain_.load(std::memory_order_relaxed);
  const bool mus_on = music_enabled_.load(std::memory_order_relaxed);
  const MusicZone prev_z = prev_zone_.load(std::memory_order_relaxed);
  const MusicZone cur_z = cur_zone_.load(std::memory_order_relaxed);
  float xf = xfade_.load(std::memory_order_relaxed);
  const bool has_music = (prev_z != MusicZone::Silence || cur_z != MusicZone::Silence);
  int on = active_;
  for (int n = 0; n < frames; ++n) {
    float acc = 0.f;
    on = 0;
    for (int i = 0; i < kMaxVoices; ++i) {
      auto& v = v_[i];
      if (!v.on) continue;
      const float atk = (v.kind >= 5) ? 0.004f : 0.012f;
      const float env = (v.t < atk) ? (v.t / atk)
                      : std::max(0.f, 1.f - (v.t - atk) / std::max(0.001f, v.dur - atk));
      const float ph = kTwoPi * v.freq * v.t;
      const float u = std::clamp(v.t / std::max(0.001f, v.dur), 0.f, 1.f);
      float s = std::sin(ph);
      if (v.kind == 1) s = 0.65f * s + 0.35f * std::sin(2.f * ph);
      else if (v.kind == 2) s = s * 0.4f + ((v.t * 97.f) - std::floor(v.t * 97.f) - 0.5f) * 0.6f;
      else if (v.kind == 3) {
        const float sw = kTwoPi * (v.freq * (1.f - 0.45f * u)) * v.t;
        s = 0.7f * std::sin(sw) + 0.3f * ((v.t * 149.f) - std::floor(v.t * 149.f) - 0.5f);
      } else if (v.kind == 4) {  // descarga y resonancia de rayos catódicos (CRT) al vencer a un boss
        const float sweep = v.freq * (0.55f + 0.65f * std::sin(u * 5.2f) + 0.35f * (1.f - u));
        const float ph1 = kTwoPi * sweep * v.t;
        const float flyback = std::sin(kTwoPi * (v.freq * 2.75f) * v.t);
        const float pulse = 0.70f + 0.30f * std::sin(kTwoPi * 32.f * v.t);
        const float spark = (v.t * 211.f) - std::floor(v.t * 211.f) - 0.5f;
        s = pulse * (0.55f * std::sin(ph1) + 0.28f * flyback + 0.17f * spark);
      } else if (v.kind == 5) {  // StepStone: golpe seco de piedra
        const float tap = std::sin(kTwoPi * v.freq * (1.f - 0.45f * u) * v.t);
        s = (0.55f * tap + 0.45f * pseudo_noise(v.t, 419.f)) * std::exp(-u * 3.5f);
      } else if (v.kind == 6) {  // StepMoss: pisada blanda en musgo/tierra
        const float thud = std::sin(kTwoPi * v.freq * (1.f - 0.35f * u) * v.t);
        s = (0.65f * thud + 0.35f * pseudo_noise(v.t, 163.f)) * std::exp(-u * 2.2f);
      } else if (v.kind == 7) {  // StepMetal: ping metálico con armónicos
        s = (0.55f * s + 0.30f * std::sin(2.41f * ph) + 0.15f * std::sin(3.85f * ph)) * std::exp(-u * 2.6f);
      } else if (v.kind == 8) {  // StepWater: chapoteo suave
        const float flutter = 0.55f + 0.45f * std::sin(kTwoPi * 42.f * v.t);
        const float drop = std::sin(kTwoPi * v.freq * (0.85f + 0.45f * u) * v.t);
        s = flutter * (0.50f * drop + 0.50f * pseudo_noise(v.t, 317.f));
      } else if (v.kind == 9) {  // Warp: arpegio ascendente etéreo de Viaje Rápido entre Santuarios
        const float f = v.freq * (1.f + 0.75f * u);
        const float p1 = kTwoPi * f * v.t;
        s = 0.55f * std::sin(p1) + 0.30f * std::sin(1.5f * p1) + 0.15f * std::sin(2.0f * p1);
      }
      acc += v.amp * env * s;
      v.t += dt;
      if (v.t >= v.dur) v.on = false;
      else ++on;
    }
    if (has_music) {
      if (xf < 1.f) xf = std::min(1.f, xf + dt / kMusicFadeSec);
      if (mus_on) {
        const float w = xf * xf * (3.f - 2.f * xf);
        const float s_cur = (cur_z != MusicZone::Silence) ? synth_zone(cur_z, music_t_) : 0.f;
        float mus = s_cur;
        if (w < 1.f && prev_z != cur_z) {
          const float s_prev = (prev_z != MusicZone::Silence) ? synth_zone(prev_z, music_t_) : 0.f;
          mus = (1.f - w) * s_prev + w * s_cur;
        }
        acc += mus * kMusicMixGain;
      }
      music_t_ += double(dt);
    } else if (xf < 1.f) {
      xf = std::min(1.f, xf + dt / kMusicFadeSec);
    }
    out[n] = std::clamp(acc * master_gain, -1.f, 1.f);
  }
  xfade_.store(xf, std::memory_order_relaxed);
  active_ = on;
}

}  // namespace ew::audio
