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
  bool restart = false;      // flanco: R corto → último checkpoint (o spawn)
  bool restart_full = false; // flanco: R largo → spawn inicial, estado limpio
};

class Sim {
 public:
  static constexpr float kDt = 1.f / 60.f;  // paso fijo
  static constexpr float kPlayerRadius = 4.f;
  static constexpr float kRopeRadius = 1.5f;  // nodos de cuerda (el agarrado usa kPlayerRadius)
  static constexpr float kWalkSpeed = 90.f;   // px/s
  static constexpr float kJumpSpeed = 230.f;  // px/s
  static constexpr float kGrabRange = 10.f;   // px desde el centro del player a un nodo
  static constexpr float kSwingForce = 900.f; // px/s² sobre el nodo agarrado
  static constexpr float kClimbSpeed = 40.f;  // px/s a lo largo de la cuerda
  static constexpr float kHeldInvMass = 0.25f;  // el player "pesa" 4× un nodo
  static constexpr int kGrabFlashFrames = 18;   // ~0.3 s de destello al agarrar
  static constexpr float kGoalReach = 16.f;     // px: distancia al entity "goal"
  static constexpr uint16_t kMetaGoal = 3;      // META tile = zona de meta
  static constexpr float kFinishFadeSec = 0.75f;  // fade de la pantalla de meta
  static constexpr float kCheckpointReach = 20.f; // px: activar checkpoint
  static constexpr uint16_t kMetaHazard = 4;      // META tile = pinchos / peligro
  static constexpr int kRespawnInvuln = 30;       // pasos sin daño al reaparecer
  static constexpr float kRestartFullHold = 0.8f; // s: R largo → reset total
  static constexpr float kFloodRestY = 72.f;      // superficie objetivo (justo bajo la cornisa)
  static constexpr float kFloodSpeed = 48.f;      // px/s que sube/baja el agua del flood_switch

  explicit Sim(Scene& scene);
  void step(const Input& in);
  // Reconstruye física y entidades; cámara al spawn. Limpia checkpoint y `finished()`.
  void reset();
  // Teleporta al último checkpoint (o al spawn); no toca puertas/switches.
  void respawn();

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
  int grab_flash() const { return grab_flash_; }  // frames de destello restantes (0 = nada)
  // Meta: entity "goal" a ≤ kGoalReach, o tile META == kMetaGoal bajo el player.
  bool finished() const { return finished_; }
  // 0..1, avanza en kFinishFadeSec segundos después de tocar la meta.
  float finish_t() const { return finish_t_; }
  // -1 = ninguno (se reaparece en player_spawn). Índice en Scene::entities().
  int checkpoint_index() const { return ckpt_; }
  const Entity* checkpoint() const;
  int deaths() const { return deaths_; }
  int invuln() const { return invuln_; }
  // Bits de SFX del último step (ew::audio::kSfx*). Se limpian al leerlos.
  uint32_t take_sfx() { const uint32_t b = sfx_; sfx_ = 0; return b; }

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
  int grab_flash_ = 0;   // destello visual al agarrar
  float held_r0_ = kRopeRadius;  // radio original del nodo agarrado
  bool finished_ = false;
  float finish_t_ = 0;  // 0..1, pantalla de fin
  int ckpt_ = -1;       // último checkpoint, -1 = spawn
  int deaths_ = 0;
  int invuln_ = 0;
  uint32_t sfx_ = 0;
  bool was_in_water_ = false;
  ew::physics::SolidFn solid_fn_;  // cache: evita alocar std::function en cada step
  void build();
  void grab(int rope, int node, bool flash = true);
  void check_goal();
  void check_checkpoints();
  void check_hazards();
  void kill();
  void apply_flood();
  uint32_t held_index() const { return ropes_[std::size_t(held_rope_)].first + uint32_t(held_); }
  void release();
};

}  // namespace ew::game
