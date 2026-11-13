// Mapeo de gamepad → Input, sin dispositivo ni GLFW.
#include "game/gamepad.hpp"
#include <cmath>
#include <cstdio>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main() {
  using namespace ew::game;
  CHECK(pad_axis(0) == 0.f);
  CHECK(pad_axis(0.2f) == 0.f);          // dentro de la zona muerta
  CHECK(pad_axis(-0.29f) == 0.f);
  CHECK(std::fabs(pad_axis(1.f) - 1.f) < 1e-5f);
  CHECK(std::fabs(pad_axis(-1.f) + 1.f) < 1e-5f);
  const float mid = pad_axis(0.65f);      // (0.65-0.3)/0.7 ≈ 0.5
  CHECK(std::fabs(mid - 0.5f) < 1e-5f);

  // Stick y d-pad se combinan con el teclado (suma + clamp).
  Input in; in.move_x = 1.f;  // tecla derecha
  PadRaw p; p.lx = 1.f;
  PadEdges e;
  apply_pad(p, in, false, e, 0);
  CHECK(in.move_x == 1.f);
  in = {}; p = {}; e = {};
  p.dl = true;
  apply_pad(p, in, false, e, 0);
  CHECK(in.move_x == -1.f);
  in = {}; in.move_x = 1.f; p = {}; p.dl = true; e = {};
  apply_pad(p, in, false, e, 0);
  CHECK(in.move_x == 0.f);  // teclado der + d-pad izq se anulan

  // Flanco de A: un solo jump al mantener.
  in = {}; p = {}; e = {}; p.a = true;
  apply_pad(p, in, false, e, 0);
  CHECK(in.jump);
  in.jump = false;
  apply_pad(p, in, false, e, 0.016);
  CHECK(!in.jump);
  p.a = false;
  apply_pad(p, in, false, e, 0.032);
  p.a = true;
  apply_pad(p, in, false, e, 0.048);
  CHECK(in.jump);

  // X/B interact, LB/Y grab, RB también salta.
  in = {}; p = {}; e = {}; p.x = true; p.lb = true;
  apply_pad(p, in, false, e, 0);
  CHECK(in.interact && in.grab && !in.jump);
  in = {}; p = {}; e = {}; p.rb = true;
  apply_pad(p, in, false, e, 0);
  CHECK(in.jump);

  // Trepar solo colgado (ly +abajo).
  in = {}; p = {}; e = {}; p.ly = 1.f; p.du = false;
  apply_pad(p, in, false, e, 0);
  CHECK(in.climb == 0.f);
  apply_pad(p, in, true, e, 0);
  CHECK(in.climb == 1.f);
  in = {}; p = {}; e = {}; p.du = true;
  apply_pad(p, in, true, e, 0);
  CHECK(in.climb == -1.f);

  // Start corto = restart; Start largo (0.8 s) = restart_full, suelta sin restart.
  in = {}; p = {}; e = {};
  p.start = true;
  apply_pad(p, in, false, e, 1.0, 0.8f);
  CHECK(!in.restart && !in.restart_full);
  p.start = false;
  apply_pad(p, in, false, e, 1.2, 0.8f);
  CHECK(in.restart && !in.restart_full);

  in = {}; p = {}; e = {};
  p.start = true;
  apply_pad(p, in, false, e, 2.0, 0.8f);
  CHECK(!in.restart_full);
  apply_pad(p, in, false, e, 2.85, 0.8f);
  CHECK(in.restart_full && !in.restart);
  in.restart_full = false;
  p.start = false;
  apply_pad(p, in, false, e, 2.9, 0.8f);
  CHECK(!in.restart && !in.restart_full);

  std::puts("gamepad ok");
  return 0;
}
