#pragma once
// Mapeo de gamepad → Input, sin GLFW (testeable). GLFW solo rellena PadRaw en echo_engine.
#include "game/sim.hpp"
#include <algorithm>
#include <cmath>

namespace ew::game {

inline constexpr float kPadDeadzone = 0.30f;

struct PadRaw {
  float lx = 0, ly = 0;  // GLFW: ly +1 = abajo
  bool a = false, b = false, x = false, y = false;
  bool lb = false, rb = false;
  bool start = false;
  bool du = false, dd = false, dl = false, dr = false;
};

struct PadEdges {
  bool jump = false, interact = false, grab = false;
  bool start_was = false, start_full = false;
  double start_since = -1;
};

// Eje con zona muerta, reescalado a [-1,1] fuera de la zona.
inline float pad_axis(float v, float dz = kPadDeadzone) {
  const float a = std::fabs(v);
  if (a < dz || dz >= 1.f) return 0.f;
  return ((v > 0.f) ? 1.f : -1.f) * (a - dz) / (1.f - dz);
}

// Combina el pad sobre un Input ya lleno por teclado (se suman y se clampea).
// `now` en segundos (glfwGetTime o reloj de test). Start = R (corto / largo).
inline void apply_pad(const PadRaw& p, Input& in, bool hanging, PadEdges& e, double now,
                      float hold = Sim::kRestartFullHold) {
  float mx = pad_axis(p.lx);
  if (p.dr) mx = 1.f;
  else if (p.dl) mx = -1.f;
  in.move_x = std::clamp(in.move_x + mx, -1.f, 1.f);

  if (hanging) {
    float cl = pad_axis(p.ly);  // +abajo, igual que Input::climb
    if (p.dd) cl = 1.f;
    else if (p.du) cl = -1.f;
    in.climb = std::clamp(in.climb + cl, -1.f, 1.f);
  }

  const bool jump_down = p.a || p.rb;
  if (jump_down && !e.jump) in.jump = true;
  e.jump = jump_down;

  const bool act_down = p.x || p.b;
  if (act_down && !e.interact) in.interact = true;
  e.interact = act_down;

  const bool grab_down = p.lb || p.y;
  if (grab_down && !e.grab) in.grab = true;
  e.grab = grab_down;

  if (p.start) {
    if (e.start_since < 0) e.start_since = now;
    if (!e.start_full && (now - e.start_since) >= double(hold)) {
      in.restart_full = true;
      e.start_full = true;
    }
  } else {
    if (e.start_was && !e.start_full) in.restart = true;
    e.start_since = -1;
    e.start_full = false;
  }
  e.start_was = p.start;
}

}  // namespace ew::game
