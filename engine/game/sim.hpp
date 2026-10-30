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
  bool interact = false;  // flanco: activar switch cercano (E / X / ↓)
  bool grab = false;      // flanco: agarrar/soltar la cuerda (C / Shift)
  float climb = 0;        // -1 sube, +1 baja (solo colgado; ↑/↓ o W/S)
};

class Sim {
 public:
  static constexpr float kDt = 1.f / 60.f;  // paso fijo
  static constexpr float kPlayerRadius = 4.f;
  static constexpr float kWalkSpeed = 90.f;   // px/s
  static constexpr float kJumpSpeed = 230.f;  // px/s
  static constexpr float kRopeSeg = 6.f;     // largo de segmento
  static constexpr float kGrabRange = 10.f;   // px desde el centro del player a un nodo
  static constexpr float kSwingForce = 900.f; // px/s² sobre el nodo agarrado
  static constexpr float kClimbSpeed = 40.f;  // px/s a lo largo de la cuerda
  static constexpr float kHeldInvMass = 0.25f;  // el player "pesa" 4× un nodo

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
  bool on_rope() const { return held_ > 0; }
  int rope_node() const { return held_; }  // 1..rope_n_ (0 = no colgado)

 private:
  Scene& scene_;
  ew::physics::World world_;
  std::vector<ew::physics::WaterBody> water_;
  uint32_t player_ = 0, rope_first_ = 0;
  int rope_n_ = 0;
  int held_ = 0;         // nodo agarrado (0 = suelto)
  float climb_acc_ = 0;  // px acumulados de trepada
  int regrab_cd_ = 0;    // pasos sin auto-reagarre tras soltar
  void grab(int node);
  void release();
};

}  // namespace ew::game
