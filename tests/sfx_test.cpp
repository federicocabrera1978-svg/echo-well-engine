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
  std::puts("sfx ok");
  return 0;
}
