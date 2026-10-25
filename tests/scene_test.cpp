// Test sin GL: Scene sobre hello.ewmap (colisión, entidades, cámara).
#include "game/scene.hpp"
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  ew::game::Scene s;
  std::string err;
  if (!s.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  CHECK(s.width_px() == 320 && s.height_px() == 184);
  const auto* spawn = s.find_type("player_spawn");
  CHECK(spawn && spawn->x == 32 && spawn->y == 152);
  CHECK(s.solid_px(100, 170));    // piso
  CHECK(!s.solid_px(100, 100));   // aire
  CHECK(s.solid_px(2, 50));       // pared izquierda
  CHECK(s.solid_px(-5, 50));      // fuera del mapa = sólido
  CHECK(s.chunk_x() == 0 && s.chunk_y() == 0);
  s.follow(300, 180);
  CHECK(s.cam_x() == 0 && s.cam_y() == 4);  // clamp: mapa 320×184, vista 320×180
  CHECK(s.active_tiles()[(22 * 40 + 5) * 2] == 1);
  std::puts("scene_test: OK");
  return 0;
}
