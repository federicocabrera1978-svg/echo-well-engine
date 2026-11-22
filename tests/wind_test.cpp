// Test sin GL: campo de brisa global (wind_gust, wind_wave, wind_sample), WindField
// (estela, ráfagas locales, límite kMaxImpulses, decaimiento, pack_impulses, apagado) e
// integración real con Sim sobre slice.ewmap.
#include "game/sim.hpp"
#include "game/wind.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)

using namespace ew;
using game::Input;
using game::Scene;
using game::Sim;
using game::WindField;

static int test_wind_functions() {
  // Determinismo bit a bit para la misma seed, world_x y time_s.
  for (uint32_t seed : {1u, 42u, 1337u, 65535u}) {
    for (float t : {0.0f, 1.25f, 7.8f, 23.4f}) {
      const float g1 = game::wind_gust(t, seed);
      const float g2 = game::wind_gust(t, seed);
      CHECK(std::bit_cast<uint32_t>(g1) == std::bit_cast<uint32_t>(g2));

      for (float x : {0.0f, 48.5f, 320.0f, 1280.0f}) {
        const float w1 = game::wind_wave(x, t, seed);
        const float w2 = game::wind_wave(x, t, seed);
        const float s1 = game::wind_sample(x, t, seed, true);
        const float s2 = game::wind_sample(x, t, seed, true);
        CHECK(std::bit_cast<uint32_t>(w1) == std::bit_cast<uint32_t>(w2));
        CHECK(std::bit_cast<uint32_t>(s1) == std::bit_cast<uint32_t>(s2));
      }
    }
  }

  // Seeds distintas producen valores distintos.
  CHECK(game::wind_gust(2.5f, 1u) != game::wind_gust(2.5f, 42u));
  CHECK(game::wind_wave(120.f, 2.5f, 1u) != game::wind_wave(120.f, 2.5f, 42u));
  CHECK(game::wind_sample(120.f, 2.5f, 7u, true) != game::wind_sample(120.f, 2.5f, 99u, true));

  // Ráfaga acotada en [0.35f, 1.85f] y variable en el tiempo.
  for (uint32_t seed : {1u, 5u, 99u, 12345u}) {
    float min_g = 10.f;
    float max_g = -10.f;
    for (int i = 0; i <= 400; ++i) {
      const float t = float(i) * 0.15f;
      const float g = game::wind_gust(t, seed);
      CHECK(std::isfinite(g));
      CHECK(g >= 0.35f && g <= 1.85f);
      if (g < min_g) min_g = g;
      if (g > max_g) max_g = g;
    }
    CHECK((max_g - min_g) > 0.30f);
  }

  // Propagación de ola viajera: diferencia de fase espacial y desplazamiento al avanzar t.
  {
    const uint32_t seed = 42u;
    const float t0 = 1.25f;
    const float w_a = game::wind_wave(0.f, t0, seed);
    const float w_b = game::wind_wave(18.f, t0, seed);
    const float w_c = game::wind_wave(45.f, t0, seed);
    CHECK(w_a != w_b && w_b != w_c && w_a != w_c);

    // Avanzar t cambia el valor local y desplaza la onda hacia +x.
    float err_forward = 0.f;
    float err_backward = 0.f;
    const float dt = 0.25f;
    const float approx_shift_x = 35.f * dt;
    bool any_time_diff = false;
    for (int i = 0; i < 12; ++i) {
      const float x = float(i) * 16.f;
      const float cur = game::wind_wave(x, t0, seed);
      const float next = game::wind_wave(x, t0 + dt, seed);
      if (std::fabs(cur - next) > 1e-3f) any_time_diff = true;
      err_forward += std::fabs(cur - game::wind_wave(x + approx_shift_x, t0 + dt, seed));
      err_backward += std::fabs(cur - game::wind_wave(x - approx_shift_x, t0 + dt, seed));
    }
    CHECK(any_time_diff);
    CHECK(err_forward < err_backward);
  }

  // Apagado: wind_sample(..., false) == 0.f.
  CHECK(game::wind_sample(100.f, 3.2f, 42u, false) == 0.f);
  CHECK(game::wind_sample(0.f, 0.f, 1u, false) == 0.f);
  return 0;
}

static int test_wind_field() {
  WindField wf;
  std::array<float, 16> buf{};
  CHECK(wf.enabled());
  CHECK(wf.time_s() == 0.f);
  CHECK(wf.pack_impulses(buf) == 0);

  // Estela de movimiento (set_wake) y muestreo local vs lejano.
  wf.set_wake(100.f, 150.f, 180.f, 16.f);
  CHECK(wf.pack_impulses(buf) == 1);
  CHECK(buf[0] == 100.f && buf[1] == 150.f);
  CHECK(std::fabs(buf[2] - 2.0f) < 1e-5f);
  CHECK(buf[3] == 16.f);

  const float base_near = game::wind_sample(100.f, wf.time_s(), 7u, true);
  const float sample_near = wf.sample_at(100.f, 150.f, 7u);
  CHECK(std::fabs((sample_near - base_near) - 2.0f) < 1e-4f);

  const float base_far = game::wind_sample(220.f, wf.time_s(), 7u, true);
  CHECK(std::fabs(wf.sample_at(220.f, 150.f, 7u) - base_far) < 1e-6f);

  // Clamp de velocidad de estela en [-2.5f, 2.5f].
  wf.set_wake(100.f, 150.f, 600.f, 16.f);
  CHECK(wf.pack_impulses(buf) == 1 && buf[2] == 2.5f);
  wf.set_wake(100.f, 150.f, -600.f, 16.f);
  CHECK(wf.pack_impulses(buf) == 1 && buf[2] == -2.5f);
  wf.clear_wake();
  CHECK(wf.pack_impulses(buf) == 0);

  // Límite kMaxImpulses = 4 (1 estela + 3 ráfagas locales), reemplazo y decaimiento.
  CHECK(WindField::kMaxImpulses == 4);
  wf.set_wake(10.f, 20.f, 90.f, 16.f);
  wf.push_impulse(100.f, 20.f, 2.0f, 24.f, 0.20f);
  wf.push_impulse(200.f, 20.f, -2.4f, 28.f, 0.50f);
  wf.push_impulse(300.f, 20.f, 3.0f, 30.f, 0.80f);
  CHECK(wf.pack_impulses(buf) == WindField::kMaxImpulses);
  CHECK(buf[4] == 100.f && std::fabs(buf[6] - 2.0f) < 1e-5f);

  // Decaimiento tras step(dt): la envolvente reduce la amplitud empaquetada.
  wf.step(0.05f);
  CHECK(std::fabs(wf.time_s() - 0.05f) < 1e-6f);
  CHECK(wf.pack_impulses(buf) == 4);
  CHECK(std::fabs(buf[6] - 1.5f) < 1e-4f);  // 2.0 * (0.15 / 0.20)

  // Al agregar una 4ta ráfaga local, sobrescribe la de menor vida restante (x = 100.f).
  wf.push_impulse(400.f, 20.f, 2.5f, 32.f, 0.60f);
  CHECK(wf.pack_impulses(buf) == WindField::kMaxImpulses);
  bool found_100 = false, found_200 = false, found_300 = false, found_400 = false;
  for (int i = 0; i < 4; ++i) {
    const float x = buf[std::size_t(i) * 4];
    if (std::fabs(x - 100.f) < 0.1f) found_100 = true;
    if (std::fabs(x - 200.f) < 0.1f) found_200 = true;
    if (std::fabs(x - 300.f) < 0.1f) found_300 = true;
    if (std::fabs(x - 400.f) < 0.1f) found_400 = true;
  }
  CHECK(!found_100 && found_200 && found_300 && found_400);

  // Al avanzar 0.50 s expira la ráfaga de x = 200.f (0.50 - 0.05 = 0.45 s restantes).
  wf.step(0.50f);
  CHECK(wf.pack_impulses(buf) == 3);
  // Al avanzar 0.65 s más expiran todas las ráfagas locales y solo queda la estela.
  wf.step(0.65f);
  CHECK(wf.pack_impulses(buf) == 1 && buf[0] == 10.f);

  // Apagado con set_enabled(false): todo en 0.f.
  wf.push_impulse(150.f, 40.f, 2.0f, 24.f, 0.40f);
  CHECK(wf.pack_impulses(buf) == 2);
  wf.set_enabled(false);
  CHECK(!wf.enabled());
  CHECK(wf.sample_at(10.f, 20.f, 7u) == 0.f);
  CHECK(wf.sample_at(150.f, 40.f, 7u) == 0.f);
  CHECK(wf.current_gust(7u) == 0.f);
  CHECK(wf.pack_impulses(buf) == 0);
  for (float v : buf) CHECK(v == 0.f);

  // Ignora nuevas estelas/ráfagas mientras está apagado.
  wf.set_wake(50.f, 50.f, 120.f, 16.f);
  wf.push_impulse(60.f, 50.f, 2.0f, 24.f, 0.50f);
  CHECK(wf.pack_impulses(buf) == 0);
  return 0;
}

static bool has_impulse_near(const std::array<float, 16>& buf, int count,
                             float target_x, float target_y, float radius, float tol = 4.f) {
  for (int i = 0; i < count; ++i) {
    const std::size_t base = std::size_t(i) * 4;
    if (std::fabs(buf[base + 0] - target_x) <= tol &&
        std::fabs(buf[base + 1] - target_y) <= tol &&
        std::fabs(buf[base + 3] - radius) < 0.5f) {
      return true;
    }
  }
  return false;
}

static int test_sim_integration(const char* slice_path) {
  std::string err;
  Scene scene;
  if (!scene.load(slice_path, err)) {
    std::fprintf(stderr, "No se pudo cargar %s: %s\n", slice_path, err.c_str());
    return 1;
  }

  Sim sim(scene);
  std::array<float, 16> buf{};

  // Asentar al jugador en el suelo.
  for (int i = 0; i < 20 && !sim.player().grounded; ++i) sim.step({});
  CHECK(sim.player().grounded);
  CHECK(sim.wind_enabled());

  // 1) Caminar hacia la derecha: estela en las cercanías de los pies del jugador.
  for (int i = 0; i < 12; ++i) sim.step({1.f});
  int count = sim.wind().pack_impulses(buf);
  CHECK(count >= 1);
  const float feet_y = sim.player().pos.y + sim.down() * Sim::kPlayerRadius;
  CHECK(has_impulse_near(buf, count, sim.player().pos.x, feet_y, 16.f, 2.f));
  CHECK(buf[2] > 0.f);

  // 2) Saltar y aterrizar: ráfaga local de aterrizaje (radio 24 px).
  {
    Input j;
    j.jump = true;
    sim.step(j);
  }
  bool left_ground = false;
  bool landed = false;
  for (int i = 0; i < 90; ++i) {
    sim.step({});
    if (!sim.player().grounded) left_ground = true;
    else if (left_ground) { landed = true; break; }
  }
  CHECK(left_ground && landed);
  count = sim.wind().pack_impulses(buf);
  CHECK(count >= 2);
  CHECK(has_impulse_near(buf, count, sim.player().pos.x,
                         sim.player().pos.y + sim.down() * Sim::kPlayerRadius, 24.f, 2.f));

  // 3) Usar Dash: ráfaga direccional fuerte (radio 30 px).
  sim.grant_ability(game::kAbilityDash);
  CHECK(sim.dash_ready());
  {
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
  }
  CHECK(sim.dash_ticks() > 0);
  count = sim.wind().pack_impulses(buf);
  CHECK(has_impulse_near(buf, count, sim.player().pos.x, sim.player().pos.y, 30.f, 2.f));

  // 4) Boss aterrizando tras su primer salto (cycle 0 -> 1) genera ráfaga en su posición.
  {
    Scene s_boss;
    CHECK(s_boss.load(slice_path, err));
    Sim sb(s_boss);
    CHECK(sb.boss_count() >= 1);
    bool boss_landed = false;
    for (int i = 0; i < 300; ++i) {
      sb.step({});
      if (sb.boss(0).cycle >= 1) { boss_landed = true; break; }
    }
    CHECK(boss_landed);
    count = sb.wind().pack_impulses(buf);
    CHECK(has_impulse_near(buf, count, sb.boss(0).x, sb.boss(0).y, 28.f, 2.f));
  }

  // 5) Desactivar vía sim.set_wind_enabled(false) anula la respuesta.
  sim.set_wind_enabled(false);
  CHECK(!sim.wind_enabled());
  for (int i = 0; i < 10; ++i) sim.step({1.f});
  {
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
  }
  CHECK(sim.wind().pack_impulses(buf) == 0);
  CHECK(sim.wind().current_gust(1u) == 0.f);
  CHECK(sim.wind().sample_at(sim.player().pos.x, sim.player().pos.y, 1u) == 0.f);
  for (float v : buf) CHECK(v == 0.f);

  return 0;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: wind_test <slice.ewmap>\n");
    return 2;
  }
  if (test_wind_functions() != 0) return 1;
  if (test_wind_field() != 0) return 1;
  if (test_sim_integration(argv[1]) != 0) return 1;
  std::puts("wind_test ok");
  return 0;
}
