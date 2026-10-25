// Sim: simulación de juego sin GL. Une Scene (mapa) + física Verlet.
// Player = círculo Verlet controlado (caminar/saltar/nadar) + una cuerda colgante
// + cuerpos de agua Hooke (sección WATER del mapa).
#pragma once
#include "game/scene.hpp"
#include "physics/verlet.hpp"
#include "physics/water.hpp"

#include <vector>

namespace ew::game {

struct Input {
  float move_x = 0;  // -1..1
  bool jump = false;  // flanco: true el frame en que se aprieta
};

class Sim {
 public:
  static constexpr float kDt = 1.f / 60.f;  // paso fijo
  static constexpr float kPlayerRadius = 4.f;
  static constexpr float kWalkSpeed = 90.f;   // px/s
  static constexpr float kJumpSpeed = 230.f;  // px/s

  explicit Sim(Scene& scene);
  void step(const Input& in);

  Scene& scene() { return scene_; }
  ew::physics::World& world() { return world_; }
  const ew::physics::World& world() const { return world_; }
  const ew::physics::Point& player() const { return world_.points()[player_]; }
  uint32_t player_index() const { return player_; }
  uint32_t rope_first() const { return rope_first_; }
  int rope_points() const { return rope_n_ + 1; }
  const std::vector<ew::physics::WaterBody>& water() const { return water_; }
  std::vector<ew::physics::WaterBody>& water() { return water_; }
  bool player_in_water() const;

 private:
  Scene& scene_;
  ew::physics::World world_;
  std::vector<ew::physics::WaterBody> water_;
  uint32_t player_ = 0, rope_first_ = 0;
  int rope_n_ = 0;
};

}  // namespace ew::game
