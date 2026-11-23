// Test sin GL: nuevos arquetipos de enemigos menores (`bat` y `crustacean`).
//   - `bat`: enemigo volador con patrulla ondulante; cae de pisotón o Dash y recarga
//     Doble Salto y Dash aéreos.
//   - `crustacean`: enemigo terrestre acorazado; salto ligero rebota en su caparazón
//     sin derrotarlo, mientras que un Dash o un pisotón de gran caída rompen su defensa.
#include "audio/sfx.hpp"
#include "game/frame_view.hpp"
#include "game/sim.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#define CHECK(c)                                                                \
  do {                                                                          \
    if (!(c)) {                                                                 \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
      return 1;                                                                 \
    }                                                                           \
  } while (0)

using namespace ew;
using game::Input;
using game::Scene;
using game::Sim;

static void place(Sim& s, float x, float y, float vy = 0.f) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = {x, y};
  p.prev = {x, y - vy};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

static const game::Entity* find_name(const Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: enemies_test <enemies.ewmap>\n");
    return 2;
  }

  Scene scene;
  std::string err;
  CHECK(scene.load(argv[1], err));

  const auto* bat_ent = find_name(scene, "bat_a");
  const auto* crust_ent = find_name(scene, "crust_a");
  CHECK(bat_ent && bat_ent->type == "bat");
  CHECK(crust_ent && crust_ent->type == "crustacean");

  // --- 1) Inicialización en Sim ---
  {
    Sim sim(scene);
    CHECK(sim.bat_count() == 1);
    CHECK(sim.crustacean_count() == 1);
    CHECK(!sim.bat(0).defeated);
    CHECK(!sim.crustacean(0).defeated);
    CHECK(sim.bat(0).home_x == bat_ent->x && sim.bat(0).home_y == bat_ent->y);
    CHECK(sim.crustacean(0).home_x == crust_ent->x && sim.crustacean(0).home_y == crust_ent->y);
  }

  // --- 2) Patrulla ondulante de bat y patrulla terrestre de crustacean ---
  {
    Sim sim(scene);
    const float b_x0 = sim.bat(0).x;
    const float b_y0 = sim.bat(0).y;
    const float c_x0 = sim.crustacean(0).x;
    for (int i = 0; i < 20; ++i) sim.step({});
    CHECK(sim.bat(0).x != b_x0);
    CHECK(sim.bat(0).y != b_y0);  // vuelo sinusoidal en Y
    CHECK(sim.crustacean(0).x != c_x0);
  }

  // --- 3) Bat: daño por contacto lateral, respawn y pisotón ---
  {
    Sim sim(scene);
    sim.grant_ability(game::kAbilityDoubleJump | game::kAbilityDash);

    // Contacto lateral sin pisotón ni Dash daña al jugador.
    place(sim, sim.bat(0).x - 6.f, sim.bat(0).y);
    sim.step({});
    CHECK(sim.deaths() == 1);

    // R corto revive al murciélago.
    { Input rst; rst.restart = true; sim.step(rst); }
    CHECK(!sim.bat(0).defeated);

    // Pisotón desde arriba: agota saltos aéreos, cae encima del bat -> derrotado, rebota y recarga habilidades.
    place(sim, sim.bat(0).x, sim.bat(0).y - 12.f, 6.f);
    // Consumir salto aéreo
    sim.step({});
    for (int i = 0; i < 15 && !sim.bat(0).defeated; ++i) sim.step({});
    CHECK(sim.bat(0).defeated);
    CHECK((sim.take_sfx() & ew::audio::kSfxHit) != 0u);
    CHECK(sim.air_jumps() > 0);  // recargó salto aéreo al pisar enemigo volador

    // R corto revive.
    { Input rst; rst.restart = true; sim.step(rst); }
    CHECK(!sim.bat(0).defeated);

    // Dash frontal destruye al bat al paso.
    place(sim, sim.bat(0).x - 14.f, sim.bat(0).y);
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
    for (int i = 0; i < 6 && !sim.bat(0).defeated; ++i) sim.step({});
    CHECK(sim.bat(0).defeated);
  }

  // --- 4) Crustacean: caparazón acorazado (rebote ligero vs destrucción por caída/dash) ---
  {
    Sim sim(scene);
    sim.grant_ability(game::kAbilityDoubleJump | game::kAbilityDash);

    // Contacto lateral daña al jugador.
    place(sim, sim.crustacean(0).x - 8.f, sim.crustacean(0).y - 4.f);
    sim.step({});
    CHECK(sim.deaths() == 1);

    // R corto revive.
    { Input rst; rst.restart = true; sim.step(rst); }
    CHECK(!sim.crustacean(0).defeated);

    // Salto ligero desde baja altura (caída lenta): el caparazón rebota sin morir.
    place(sim, sim.crustacean(0).x, sim.crustacean(0).y - 10.f, 0.4f);
    sim.step({});
    // El jugador fue rebotado hacia arriba
    CHECK(!sim.crustacean(0).defeated);
    CHECK(sim.player().velocity().y < 0.f);

    // Pisotón de gran caída (desde la plataforma superior a y=64): velocidad vertical alta
    place(sim, sim.crustacean(0).x, sim.crustacean(0).y - 40.f, 2.5f);
    for (int i = 0; i < 20 && !sim.crustacean(0).defeated; ++i) sim.step({});
    CHECK(sim.crustacean(0).defeated);
    CHECK((sim.take_sfx() & ew::audio::kSfxBreak) != 0u);

    // R corto revive.
    { Input rst; rst.restart = true; sim.step(rst); }
    CHECK(!sim.crustacean(0).defeated);

    // Dash directo contra el crustáceo rompe su caparazón y lo destruye.
    place(sim, sim.crustacean(0).x - 16.f, sim.crustacean(0).y - 4.f);
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
    for (int i = 0; i < 8 && !sim.crustacean(0).defeated; ++i) sim.step({});
    CHECK(sim.crustacean(0).defeated);
    CHECK((sim.take_sfx() & ew::audio::kSfxBreak) != 0u);
  }

  // --- 5) FrameData / Marcadores de render sin GL ---
  {
    Sim sim(scene);
    game::FrameData fv_alive;
    game::build_world_frame(fv_alive, scene, sim, 0.5f, false, false);
    const int markers_alive = fv_alive.marker_count;
    CHECK(markers_alive > 0);

    // Derrotar ambos y verificar que dejan de emitir marcadores
    sim.grant_ability(game::kAbilityDash);
    place(sim, sim.bat(0).x - 12.f, sim.bat(0).y);
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
    for (int i = 0; i < 6 && !sim.bat(0).defeated; ++i) sim.step({});

    place(sim, sim.crustacean(0).x - 12.f, sim.crustacean(0).y - 4.f);
    sim.step(d);
    for (int i = 0; i < 6 && !sim.crustacean(0).defeated; ++i) sim.step({});
    CHECK(sim.bat(0).defeated && sim.crustacean(0).defeated);

    game::FrameData fv_dead;
    game::build_world_frame(fv_dead, scene, sim, 0.5f, false, false);
    CHECK(fv_dead.marker_count < markers_alive);
  }

  std::puts("enemies_test OK");
  return 0;
}
