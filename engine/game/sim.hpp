// Sim: simulación de juego sin GL. Une Scene (mapa) + física Verlet.
// Player = círculo Verlet controlado (caminar/saltar/nadar) + cuerdas colgantes (sección ROPE)
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
  // Cuerdas: salen de la sección ROPE del mapa (0..N). Índice 0 por defecto.
  int rope_count() const { return int(ropes_.size()); }
  uint32_t rope_first(int r = 0) const { return ropes_[std::size_t(r)].first; }
  int rope_points(int r = 0) const { return ropes_.empty() ? 0 : ropes_[std::size_t(r)].n + 1; }
  float rope_seg(int r = 0) const { return ropes_[std::size_t(r)].seg; }
  int held_rope() const { return held_ ? held_rope_ : -1; }
  const std::vector<ew::physics::WaterBody>& water() const { return water_; }
  std::vector<ew::physics::WaterBody>& water() { return water_; }
  bool player_in_water() const;
  bool on_rope() const { return held_ > 0; }
  int rope_node() const { return held_; }  // 1..n de held_rope() (0 = no colgado)

 private:
  Scene& scene_;
  ew::physics::World world_;
  std::vector<ew::physics::WaterBody> water_;
  struct Rope { uint32_t first; int n; float seg; };
  std::vector<Rope> ropes_;
  uint32_t player_ = 0;
  int held_rope_ = 0;  // cuerda del nodo agarrado
  int held_ = 0;         // nodo agarrado (0 = suelto)
  float climb_acc_ = 0;  // px acumulados de trepada
  int regrab_cd_ = 0;    // pasos sin auto-reagarre tras soltar
  void grab(int rope, int node);
  uint32_t held_index() const { return ropes_[std::size_t(held_rope_)].first + uint32_t(held_); }
  void release();
};

}  // namespace ew::game
