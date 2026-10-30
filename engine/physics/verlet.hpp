// Física Verlet: puntos (círculos), constraints de distancia, damping y
// colisión contra un campo de tiles sólidos (p. ej. capa COL del .ewmap).
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace ew::physics {

struct Vec2 {
  float x = 0, y = 0;
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
};
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(Vec2 a) { return std::sqrt(dot(a, a)); }

struct Point {
  Vec2 pos, prev, force;  // force: acumulada por frame (se limpia en step)
  float inv_mass = 1;     // 0 = fijo (pin)
  float radius = 1;
  bool collide_tiles = true;
  bool grounded = false;  // tocó suelo en el último step
  Vec2 velocity() const { return pos - prev; }  // px por step
  void set_velocity(Vec2 v) { prev = pos - v; }
};

struct Stick {
  uint32_t a, b;
  float rest;
  float stiffness = 1;  // 0..1 por iteración
};

// Devuelve true si el tile (tx, ty) es sólido.
using SolidFn = std::function<bool(int tx, int ty)>;

class World {
 public:
  Vec2 gravity{0, 600};  // px/s² (y hacia abajo)
  float damping = 0.995f;  // fracción de velocidad conservada por step
  float friction = 0.08f;  // pérdida tangencial por iteración al tocar un tile
  int iterations = 6;
  float tile_size = 8;

  uint32_t add_point(Vec2 p, float radius, float inv_mass = 1);
  uint32_t add_stick(uint32_t a, uint32_t b, float stiffness = 1);  // rest = distancia actual
  // Cuerda colgando de `anchor` (primer punto fijo), `n` segmentos de `seg` px.
  // Devuelve el índice del primer punto.
  uint32_t add_rope(Vec2 anchor, int n, float seg, float radius = 1.5f);

  void step(float dt, const SolidFn& solid);

  std::vector<Point>& points() { return points_; }
  const std::vector<Point>& points() const { return points_; }
  const std::vector<Stick>& sticks() const { return sticks_; }
  // Pares de puntos que colisionan entre sí (círculo-círculo), p. ej. player ↔ cuerda.
  void add_collision_pair(uint32_t a, uint32_t b) { pairs_.push_back({a, b, true}); }
  // Desactiva/activa las colisiones de pares que involucran al punto `i` (p. ej. player colgado).
  void set_pairs_enabled(uint32_t i, bool on);

 private:
  void solve_sticks();
  void solve_pairs();
  void collide(Point& p, const SolidFn& solid);
  std::vector<Point> points_;
  std::vector<Stick> sticks_;
  struct Pair { uint32_t a, b; bool on = true; };
  std::vector<Pair> pairs_;
};

}  // namespace ew::physics
