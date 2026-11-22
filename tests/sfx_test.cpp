// Test sin dispositivo: mixer procedural de SFX (seno + envolvente).
#include "audio/sfx.hpp"
#include <cmath>
#include <cstdio>
#include <vector>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)
int main() {
  using namespace ew::audio;
  Mixer m;
  CHECK(m.active() == 0);
  std::vector<float> buf(kRate / 2, 99.f);  // 0.5 s, precargado para detectar escritura
  m.render(buf.data(), 64);  // silencio
  for (int i = 0; i < 64; ++i) CHECK(buf[i] == 0.f);
  m.play(SfxId::Jump);
  std::fill(buf.begin(), buf.end(), 0.f);
  m.render(buf.data(), int(buf.size()));
  float peak = 0, sum2 = 0;
  int nonzero = 0;
  for (float s : buf) {
    peak = std::fmax(peak, std::fabs(s));
    sum2 += s * s;
    if (std::fabs(s) > 1e-4f) ++nonzero;
  }
  CHECK(peak > 0.05f && peak <= 1.f);
  CHECK(nonzero > 100);
  CHECK(std::fabs(buf[0]) < 0.15f);  // arranca con ataque, no un click a full
  CHECK(m.active() == 0);            // el jump dura ~90 ms, 0.5 s lo consume
  // Dos voces se mezclan (no se pisan del todo).
  Mixer m2;
  m2.play(SfxId::Goal);
  m2.play(SfxId::Jump);
  CHECK(m2.active() == 2);
  std::vector<float> b2(1024);
  m2.render(b2.data(), 1024);
  float p2 = 0;
  for (float s : b2) p2 = std::fmax(p2, std::fabs(s));
  CHECK(p2 > 0.05f && p2 <= 1.f);
  // Ganancia / mute.
  Mixer mv;
  mv.play(SfxId::Jump);
  std::vector<float> bv(2048);
  mv.render(bv.data(), 2048);
  float full = 0;
  for (float s : bv) full = std::fmax(full, std::fabs(s));
  CHECK(full > 0.05f);
  mv.set_gain(0.5f);
  mv.play(SfxId::Jump);
  std::fill(bv.begin(), bv.end(), 0.f);
  mv.render(bv.data(), 2048);
  float half = 0;
  for (float s : bv) half = std::fmax(half, std::fabs(s));
  CHECK(half > 0.02f && half < full * 0.8f);
  mv.set_mute(true);
  mv.play(SfxId::Jump);
  std::fill(bv.begin(), bv.end(), 99.f);
  mv.render(bv.data(), 64);
  for (int i = 0; i < 64; ++i) CHECK(bv[i] == 0.f);
  CHECK(mv.gain() == 0.f);
  // Habilidad desbloqueada, Dash, Break, Hit y BossCrt (rayos catódicos): pico válido y fin limpio.
  std::vector<float> buf_long(kRate, 0.f);  // 1.0 s para cubrir BossCrt (0.85 s)
  for (SfxId id : {SfxId::Ability, SfxId::Dash, SfxId::Break, SfxId::Hit, SfxId::BossCrt}) {
    Mixer ma;
    ma.play(id);
    CHECK(ma.active() == 1);
    std::fill(buf_long.begin(), buf_long.end(), 0.f);
    ma.render(buf_long.data(), int(buf_long.size()));
    float pa = 0;
    for (float s : buf_long) pa = std::fmax(pa, std::fabs(s));
    CHECK(pa > 0.05f && pa <= 1.f);
    CHECK(ma.active() == 0);
  }
  // Pasos por superficie: pico sutil válido, sin clic inicial y fin limpio.
  std::vector<float> buf_step(kRate / 5, 0.f);  // 0.2 s cubre todos los pasos (~38..58 ms)
  for (SfxId id : {SfxId::StepStone, SfxId::StepMoss, SfxId::StepMetal, SfxId::StepWater}) {
    Mixer ms;
    ms.play(id);
    CHECK(ms.active() == 1);
    std::fill(buf_step.begin(), buf_step.end(), 0.f);
    ms.render(buf_step.data(), int(buf_step.size()));
    float ps = 0.f;
    int nz = 0;
    for (float s : buf_step) {
      ps = std::fmax(ps, std::fabs(s));
      if (std::fabs(s) > 1e-4f) ++nz;
    }
    CHECK(std::fabs(buf_step[0]) < 0.15f);
    CHECK(ps > 0.04f && ps <= 0.35f);
    CHECK(nz > 50);
    CHECK(ms.active() == 0);
  }
  // Música procedural por zona: acotada, no nula, crossfade ~1.5 s y respeta mute/enable.
  {
    Mixer def;
    CHECK(def.music_zone() == MusicZone::Silence);
    CHECK(def.music_fade() == 1.f);
    CHECK(def.music_enabled());
  }
  for (MusicZone z : {MusicZone::Well, MusicZone::Catacombs, MusicZone::WyrmLair, MusicZone::Boss}) {
    Mixer mz;
    mz.set_music_zone(z);
    CHECK(mz.music_zone() == z);
    CHECK(mz.music_fade() < 1.f);
    std::vector<float> half_sec(kRate / 2, 0.f);
    mz.render(half_sec.data(), int(half_sec.size()));
    CHECK(mz.music_fade() > 0.1f && mz.music_fade() < 0.9f);
    std::vector<float> rest(kRate + kRate / 10, 0.f);
    mz.render(rest.data(), int(rest.size()));
    CHECK(mz.music_fade() == 1.f);
    std::fill(half_sec.begin(), half_sec.end(), 0.f);
    mz.render(half_sec.data(), int(half_sec.size()));
    float pm = 0.f;
    int nz = 0;
    for (float s : half_sec) {
      CHECK(std::isfinite(s) && s >= -1.f && s <= 1.f);
      pm = std::fmax(pm, std::fabs(s));
      if (std::fabs(s) > 1e-4f) ++nz;
    }
    CHECK(pm > 0.02f && pm <= 0.35f);
    CHECK(nz > kRate / 4);
    mz.set_music_enabled(false);
    std::fill(half_sec.begin(), half_sec.end(), 99.f);
    mz.render(half_sec.data(), 128);
    for (int i = 0; i < 128; ++i) CHECK(half_sec[i] == 0.f);
    mz.set_music_enabled(true);
    mz.set_mute(true);
    std::fill(half_sec.begin(), half_sec.end(), 99.f);
    mz.render(half_sec.data(), 128);
    for (int i = 0; i < 128; ++i) CHECK(half_sec[i] == 0.f);
  }
  // Transición directa entre dos zonas no silenciosas.
  {
    Mixer mc;
    mc.set_music_zone(MusicZone::Well);
    std::vector<float> blk(kRate * 2, 0.f);
    mc.render(blk.data(), int(blk.size()));
    CHECK(mc.music_fade() == 1.f);
    mc.set_music_zone(MusicZone::Boss);
    CHECK(mc.music_fade() < 1.f);
    mc.render(blk.data(), int(blk.size()));
    CHECK(mc.music_fade() == 1.f);
    for (float s : blk) CHECK(std::isfinite(s) && s >= -1.f && s <= 1.f);
  }
  std::puts("sfx ok");
  return 0;
}
