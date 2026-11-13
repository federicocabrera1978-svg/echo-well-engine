// Benchmark headless: timestep fijo verificado + costo por paso de Sim (sin GL).
// Umbral holgado para CI (runners compartidos). Reporta us/paso.
#include "game/sim.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main(int argc, char** argv) {
  using namespace ew::game;
  using clock = std::chrono::steady_clock;
  CHECK(std::fabs(Sim::kDt * 60.f - 1.f) < 1e-6f);
  CHECK(argc >= 2);

  std::string err;
  Scene s1, s2;
  if (!s1.load(argv[1], err) || !s2.load(argv[1], err)) {
    std::printf("load: %s\n", err.c_str());
    return 1;
  }
  Sim a(s1), b(s2);
  Input in; in.move_x = 1;
  // Determinismo: dos sims, mismos inputs, mismas posiciones (paso fijo kDt, sin reloj de pared).
  for (int i = 0; i < 180; ++i) { a.step(in); b.step(in); }
  CHECK(std::fabs(a.player().pos.x - b.player().pos.x) < 1e-4f);
  CHECK(std::fabs(a.player().pos.y - b.player().pos.y) < 1e-4f);

  // 2 s caminando en suelo abierto (spawn de slice): ~ kWalkSpeed * 2 px, no más.
  // (puede chocarse con cosas; solo acotamos).
  CHECK(a.player().pos.x > 32.f);

  // Benchmark: 6000 pasos (~100 s de sim) con input tipo demo.
  Scene sb;
  if (!sb.load(argv[1], err)) return 1;
  Sim sim(sb);
  const int N = 6000;
  const auto t0 = clock::now();
  for (int i = 0; i < N; ++i) {
    Input in;
    in.move_x = (i / 150) % 2 ? -1.f : 1.f;
    in.jump = i % 90 == 45;
    in.interact = i % 300 == 43;
    sim.step(in);
  }
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
  const double us_step = double(us) / double(N);
  const double ms_total = double(us) / 1000.0;
  std::printf("bench: %d pasos (%.1f s de sim) en %.2f ms, %.1f us/paso (%.2f x tiempo real a 60 Hz)\n",
              N, N * Sim::kDt, ms_total, us_step, (us_step / 1000.0) / (Sim::kDt * 1000.0));
  // Umbral holgado: 2 ms/paso (el presupuesto de frame es 16.6 ms). En esta caja suele ser <50 us.
  constexpr double kMaxUs = 2000.0;
  if (us_step > kMaxUs) {
    std::printf("FAIL bench demasiado lento: %.1f us/paso (tope %.0f)\n", us_step, kMaxUs);
    return 1;
  }
  std::puts("bench ok");
  return 0;
}
