// Test sin GL: Scene sobre hello.ewmap (colisión, entidades, cámara).
#include "game/scene.hpp"
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  ew::game::Scene s;
  std::string err;
  if (!s.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  CHECK(s.width_px() == 640 && s.height_px() == 184);  // 2×1 chunks
  const auto* spawn = s.find_type("player_spawn");
  CHECK(spawn && spawn->x == 32 && spawn->y == 152);
  CHECK(s.solid_px(100, 170));    // piso
  CHECK(!s.solid_px(100, 100));   // aire
  CHECK(s.solid_px(2, 50));       // pared izquierda
  CHECK(s.solid_px(-5, 50));      // fuera del mapa = sólido
  CHECK(s.chunk_x() == 0 && s.chunk_y() == 0);
  s.follow(300, 180);
  CHECK(s.cam_x() == 0 && s.cam_y() == 4);  // clamp: mapa 320×184, vista 320×180
  constexpr int W = ew::game::Scene::kNbW;
  CHECK(s.active_tiles()[((23 + 22) * W + 40 + 5) * 2] == 1);   // piso del chunk 0 (centro del 3×3)
  CHECK(s.active_tiles()[((23 + 22) * W + 80 + 5) * 2] == 1);   // vecino derecho (chunk 1) también
  CHECK(s.active_tiles()[((23 + 22) * W + 5) * 2] == 0);        // vecino izquierdo: fuera del mapa
  // Seam: cruzar al chunk 1 cambia el chunk activo; la cámara desliza y termina en x=320.
  const auto ver = s.chunk_version();
  s.follow(330, 150, 1.f / 60.f);
  CHECK(s.chunk_x() == 1 && s.chunk_version() != ver);
  CHECK(s.cam_x() > 0 && s.cam_x() < 320 && s.cam_transitioning());
  for (int i = 0; i < 60; ++i) s.follow(330, 150, 1.f / 60.f);
  CHECK(s.cam_x() == 320 && !s.cam_transitioning());
  CHECK(s.active_tiles()[((23 + 22) * W + 5) * 2] == 1);        // ahora el izquierdo es el chunk 0
  CHECK(!s.solid_px(330, 150) && s.solid_px(636, 100));         // pared derecha del mapa
  // Dead-zone (función pura): vista 320, extensión 480, semi-zona 48 → zona [112,208] con off=0.
  using S = ew::game::Scene;
  CHECK(S::dead_zone_step(0, 200, 48, 320, 480) == 0);    // dentro: no arrastra
  CHECK(S::dead_zone_step(0, 220, 48, 320, 480) == 12);   // sale por derecha 12 px
  CHECK(S::dead_zone_step(12, 150, 48, 320, 480) == 12);  // vuelve adentro: queda
  CHECK(S::dead_zone_step(12, 100, 48, 320, 480) == 0);   // sale por izquierda
  CHECK(S::dead_zone_step(0, 470, 48, 320, 480) == 160);  // clamp al borde de pantalla
  // Dead-zone en la escena: vertical (holgura 4 px). Snap centrado y luego moverse.
  s.follow(400, 90);
  CHECK(s.cam_y() == 0);
  s.follow(400, 110, 1.f / 60.f); CHECK(s.cam_y() == 0);           // dentro de ±24
  for (int i = 0; i < 30; ++i) s.follow(400, 118, 1.f / 60.f);
  CHECK(s.cam_y() == 4);                                             // salió: arrastra (clamp 4)
  for (int i = 0; i < 30; ++i) s.follow(400, 100, 1.f / 60.f);
  CHECK(s.cam_y() == 4);                                             // vuelve a la zona: no re-centra
  CHECK(s.cam_x() == 320);                                           // x fijo (chunk = vista)
  std::puts("scene_test: OK");
  return 0;
}
