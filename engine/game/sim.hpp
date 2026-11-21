// Sim: simulación de juego sin GL. Une Scene (mapa) + física Verlet.
// Player = círculo Verlet controlado (caminar/saltar/nadar) + cuerdas colgantes (sección ROPE)
// + cuerpos de agua Hooke (sección WATER del mapa).
#pragma once
#include "game/scene.hpp"
#include "physics/verlet.hpp"
#include "physics/water.hpp"

#include <vector>

namespace ew::game {

// Bitmask de habilidades permanentes del jugador (se persisten en save.ewsv v3).
inline constexpr uint32_t kAbilityDoubleJump = 1u << 0;
inline constexpr uint32_t kAbilityAll = kAbilityDoubleJump;

// Estado de animación del protagonista (mapea a filas 0..5 de assets/atlas/sprites.png).
enum class PlayerAnim : uint8_t {
  Idle = 0,
  Walk = 1,
  Jump = 2,
  DoubleJump = 3,
  Climb = 4,
  Swim = 5,
};

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
  static constexpr float kCollectReach = 12.f;    // px: agarrar un collectible (huevo)
  static constexpr int kPickupFlashFrames = 40;   // ~0.66 s de destello del HUD al agarrar
  static constexpr float kFlipReach = 10.f;       // px: tocar un orbe gravity_flip lo dispara
  static constexpr float kFlipRearm = 18.f;       // px: alejarse para que el orbe vuelva a armarse
  static constexpr float kDoubleJumpSpeed = 215.f;  // px/s (segundo salto en el aire)
  static constexpr float kAbilityReach = 12.f;      // px: agarrar una reliquia de habilidad
  static constexpr float kDialogReach = 16.f;       // px: alcance para hablar/leer NPC o estela
  static constexpr int kDoubleJumpFlashFrames = 14; // ~0.23 s de aro bajo los pies
  static constexpr int kAbilityFlashFrames = 45;    // ~0.75 s de destello del HUD al desbloquear
  // Boss (type "boss"): (x, y) del YAML = centro de los pies. Caja de kBossW×kBossH px.
  static constexpr float kBossW = 24.f, kBossH = 16.f;
  static constexpr int kBossHits = 3;              // pisotones para vencerlo
  static constexpr float kBossRange = 64.f;        // px a cada lado de su posición inicial
  static constexpr float kBossSpeed = 36.f;        // px/s caminando (+kBossSpeedPerHit por golpe)
  static constexpr float kBossSpeedPerHit = 14.f;
  static constexpr float kBossWalkSec = 1.6f;      // camina, después avisa y salta
  static constexpr float kBossTellSec = 0.5f;      // quieto (aviso) antes del salto
  static constexpr float kBossHopSpeed = 200.f;    // px/s inicial del salto (gravedad 600)
  static constexpr float kBossHurtSec = 1.0f;      // tras un golpe: no daña ni recibe daño

  // Plataforma sólida que recorre waypoints en ping-pong (sección MOVER / type "mover").
  // (x, y) es la esquina superior izquierda actual, en px; w/h en px (tiles × 8).
  struct Mover {
    float x = 0, y = 0, w = 0, h = 0;
    float speed = 40.f, pause = 0.5f;
    uint16_t tile = 6;
  };

  explicit Sim(Scene& scene);
  void step(const Input& in);
  // Reconstruye física y entidades; cámara al spawn. Limpia checkpoint, `finished()` y
  // collectibles (R largo = partida limpia). Las plataformas vuelven al primer waypoint.
  void reset();
  // Teleporta al último checkpoint (o al spawn); no toca puertas/switches ni collectibles.
  // Las plataformas sí vuelven a su estado inicial (el puzzle queda armado otra vez).
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
  // Collectibles (entidades type "collectible"; Entity::active = agarrado). Sobreviven a
  // respawn / muerte; reset() los vuelve a poner. El goal no los requiere (HUD x/total).
  int collected() const;
  int collectible_total() const;
  int pickup_flash() const { return pickup_flash_; }
  int invuln() const { return invuln_; }
  int mover_count() const { return int(movers_.size()); }
  const Mover& mover(int i) const { return movers_[std::size_t(i)]; }
  // Boss: patrón determinista (caminar → aviso → salto, en loop; más rápido con cada golpe).
  // Se le pega cayéndole encima (pisotón: el player se mueve hacia su "abajo" y viene del
  // lado de su "arriba"; con gravedad invertida, desde abajo). Tocarlo de otra forma mata.
  // Al vencerlo, Entity::active = true y se abren sus puertas linkeadas; mientras haya un
  // boss vivo la meta no cuenta. respawn() lo cura (si no estaba vencido); reset() todo.
  struct Boss {
    float x = 0, y = 0;   // centro de los pies (px)
    int hits = 0;
    float hurt = 0;       // s de invulnerabilidad restante
    bool defeated = false;
    int phase = 0;        // 0 camina, 1 aviso, 2 salto
    float t = 0;          // s en la fase actual
    int dir = -1;         // sentido de la caminata
    float vy = 0;         // px/s (salto)
    float home_x = 0, home_y = 0;
    std::size_t ent = 0;  // índice en Scene::entities()
    float speed() const { return kBossSpeed + kBossSpeedPerHit * float(hits); }
  };
  int boss_count() const { return int(bosses_.size()); }
  const Boss& boss(int i) const { return bosses_[std::size_t(i)]; }
  bool bosses_defeated() const;  // true si no hay boss vivo
  // Índice de la plataforma que está cargando al player, o -1.
  int ridden() const { return ridden_; }
  // Gravedad del player: false = normal (cae hacia +Y), true = invertida (cae hacia -Y).
  // La cambian los orbes `gravity_flip`; checkpoint guarda el estado, reset() la normaliza.
  bool gravity_flipped() const { return flip_; }
  // +1 con gravedad normal, -1 invertida: dirección "abajo" del player en Y.
  float down() const { return flip_ ? -1.f : 1.f; }
  void set_gravity_flipped(bool f);
  int flips() const { return flips_; }  // veces que se invirtió desde reset()
  bool checkpoint_flipped() const { return ckpt_flip_; }  // gravedad guardada con el checkpoint
  // Habilidades metroidvania (bitmask kAbility*). Sobreviven a respawn/muerte; reset() las limpia.
  uint32_t abilities() const { return abilities_; }
  bool has_ability(uint32_t mask) const { return (abilities_ & mask) == mask; }
  void grant_ability(uint32_t mask);
  void set_abilities(uint32_t mask);
  int air_jumps() const { return air_jumps_; }
  int double_jump_flash() const { return double_jump_flash_; }
  float double_jump_x() const { return double_jump_x_; }
  float double_jump_y() const { return double_jump_y_; }
  int ability_flash() const { return ability_flash_; }
  // Estado visual de animación del protagonista (sin GL).
  int player_facing() const { return facing_; }  // +1 derecha, -1 izquierda
  PlayerAnim player_anim() const { return anim_; }
  int player_anim_frame() const { return anim_frame_; }  // 0..3
  // Partida guardada: reset() limpio, aplica Entity::active por entidad (puertas, switches,
  // huevos, boss vencido, reliquias), fija checkpoint + gravedad + habilidades y reaparece ahí.
  // `active` debe tener una entrada por entidad (lo valida apply_save en save.hpp).
  void load_state(int checkpoint, bool flipped, const std::vector<uint8_t>& active, uint32_t abilities = 0);
  // Bits de SFX del último step (ew::audio::kSfx*). Se limpian al leerlos.
  uint32_t take_sfx() { const uint32_t b = sfx_; sfx_ = 0; return b; }
  // Índice en Scene::dialogs() del diálogo disparado pendiente (-1 = ninguno). Se limpia al leerlo.
  int take_dialog() { const int d = pending_dialog_; pending_dialog_ = -1; return d; }
  int nearby_interact_dialog() const;

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
  int pickup_flash_ = 0;
  uint32_t sfx_ = 0;
  bool was_in_water_ = false;
  ew::physics::SolidFn solid_fn_;  // cache: evita alocar std::function en cada step
  struct MoverState : Mover {
    float prev_x = 0, prev_y = 0;
    std::vector<ew::physics::Vec2> wp;
    int seg = 0;    // índice del waypoint de salida del segmento actual
    int dir = 1;    // +1 o -1 (ping-pong)
    float dist = 0; // px recorridos en el segmento
    float wait = 0; // s de pausa restante
  };
  std::vector<MoverState> movers_;
  int ridden_ = -1;  // plataforma que carga al player
  bool flip_ = false;       // gravedad del player invertida
  bool ckpt_flip_ = false;  // gravedad al tomar el último checkpoint
  int flips_ = 0;
  std::vector<uint8_t> flip_armed_;  // por entidad: orbe gravity_flip listo para disparar
  uint32_t abilities_ = 0;
  int air_jumps_ = 0;
  int double_jump_flash_ = 0;
  float double_jump_x_ = 0, double_jump_y_ = 0;
  int ability_flash_ = 0;
  int facing_ = 1;
  PlayerAnim anim_ = PlayerAnim::Idle;
  int anim_frame_ = 0;
  int anim_ticks_ = 0;
  int air_ticks_ = 0;
  int pending_dialog_ = -1;
  std::vector<Boss> bosses_;
  void update_player_anim(const Input& in);
  void trigger_entity_dialog(std::size_t entity_idx, ew::map::DialogTrigger trig);
  void check_dialogs_proximity();
  void reset_bosses(bool keep_defeated);
  void advance_bosses();
  void collide_bosses();
  void check_gravity_flips();
  void rearm_flips();
  void check_abilities();
  void build();
  void reset_movers();
  void advance_movers();
  void carry_player();
  void collide_movers();
  void grab(int rope, int node, bool flash = true);
  void check_goal();
  void check_checkpoints();
  void check_hazards();
  void check_collectibles();
  void kill();
  void apply_flood();
  uint32_t held_index() const { return ropes_[std::size_t(held_rope_)].first + uint32_t(held_); }
  void release();
};

}  // namespace ew::game
