// Sim: simulación de juego sin GL. Une Scene (mapa) + física Verlet.
// Player = círculo Verlet controlado (caminar/saltar/nadar) + cuerdas colgantes (sección ROPE)
// + cuerpos de agua Hooke (sección WATER del mapa).
#pragma once
#include "game/boss_victory.hpp"
#include "game/hero.hpp"
#include "game/scene.hpp"
#include "game/wind.hpp"
#include "physics/verlet.hpp"
#include "physics/water.hpp"

#include <vector>

namespace ew::game {

// Bitmask de habilidades permanentes del jugador (se persisten en save.ewsv v3).
inline constexpr uint32_t kAbilityDoubleJump = 1u << 0;
inline constexpr uint32_t kAbilityDash = 1u << 1;
inline constexpr uint32_t kAbilityWallClimb = 1u << 2;
inline constexpr uint32_t kAbilityReveal = 1u << 3;  // Farol Revelador: solidifica reveal_block cercano
inline constexpr uint32_t kAbilityAll = kAbilityDoubleJump | kAbilityDash | kAbilityWallClimb | kAbilityReveal;

// Estado de animación del protagonista (mapea a filas 0..5 de assets/atlas/sprites.png).
enum class PlayerAnim : uint8_t {
  Idle = 0,
  Walk = 1,
  Jump = 2,
  DoubleJump = 3,
  Climb = 4,
  Swim = 5,
};

// Superficie bajo los pies del protagonista (para SFX de pasos).
enum class SurfaceKind : uint8_t { Stone = 0, Moss = 1, Metal = 2, Water = 3 };

struct Input {
  float move_x = 0;  // -1..1
  bool jump = false;  // flanco: true el frame en que se aprieta
  bool interact = false;  // flanco: activar switch cercano (E / X / ↓)
  bool grab = false;      // flanco: agarrar/soltar la cuerda (C / Shift)
  float climb = 0;        // -1 sube, +1 baja (solo colgado; ↑/↓ o W/S)
  bool restart = false;      // flanco: R corto → último checkpoint (o spawn)
  bool restart_full = false; // flanco: R largo → spawn inicial, estado limpio
  bool dash = false;         // flanco: embestida rápida (E / X / K / Shift der. o pad X/B)
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
  static constexpr float kDoubleJumpSpeed = 245.f;  // px/s (segundo salto en el aire)
  static constexpr float kAbilityReach = 12.f;      // px: agarrar una reliquia de habilidad
  static constexpr float kRevealReach = 96.f;       // px: revelar un reveal_block cercano
  static constexpr float kTimedSwitchSec = 6.f;     // s: puertas abiertas por un timed_switch
  static constexpr int kCrystalBreakFlashFrames = 18;  // frames de escombros al romper un crystal
  static constexpr float kDialogReach = 16.f;       // px: alcance para hablar/leer NPC o estela
  static constexpr int kDoubleJumpFlashFrames = 14; // ~0.23 s de aro bajo los pies
  static constexpr int kAbilityFlashFrames = 45;    // ~0.75 s de destello del HUD al desbloquear
  static constexpr float kDashSpeed = 240.f;        // px/s durante la embestida (Dash)
  static constexpr int kDashFrames = 10;            // ~0.17 s de embestida sin gravedad
  static constexpr int kDashCooldownFrames = 18;    // ~0.30 s de enfriamiento mínimo
  static constexpr int kDashFlashFrames = 14;       // frames de estela visual
  // Recompensas por hitos de huevos (collectibles) y daño no letal con +1/+2 HP.
  static constexpr int kEggsForExtraHp = 2;
  static constexpr int kEggsForDashUpgrade = 4;
  static constexpr int kEggsForThirdHp = 6;
  static constexpr int kDashUpgradedFrames = 14;
  static constexpr int kDashUpgradedCooldown = 10;
  static constexpr int kDashUpgradedCharges = 2;
  static constexpr int kHitInvulnFrames = 60;
  // Ventajas jugables pasivas de cada héroe (Lantern / Opossum / Beaver).
  static constexpr float kLanternLightRadius = 150.f;
  static constexpr float kBaseLightRadius = 125.f;
  static constexpr float kLanternRevealReach = 128.f;
  static constexpr float kOpossumWalkSpeed = 98.f;
  static constexpr int kOpossumCoyoteFrames = 8;
  static constexpr float kBeaverWaterJumpFactor = 0.94f;
  static constexpr float kBaseWaterJumpFactor = 0.80f;
  static constexpr float kBeaverWallSlideSpeed = 26.f;
  static constexpr int kWallBreakFlashFrames = 18;  // frames de escombros al romper break_wall
  static constexpr float kWallSlideSpeed = 36.f;    // px/s caída máxima al deslizar en pared
  static constexpr float kWallJumpVx = 138.f;       // px/s impulso horizontal al saltar de pared
  static constexpr float kWallJumpVy = 232.f;       // px/s impulso vertical al saltar de pared
  static constexpr int kWallCoyoteFrames = 6;       // frames de coyote lateral tras soltar pared
  static constexpr int kWallLockFrames = 8;         // frames de mezcla horizontal suave tras wall-jump
  static constexpr int kCoyoteFrames = 6;           // frames de coyote time al dejar el suelo
  static constexpr int kJumpBufferFrames = 6;       // frames de buffer al apretar salto antes de aterrizar
  static constexpr std::size_t kMaxDust = 24;       // pool máximo de partículas de polvo/estela
  static constexpr int kHitstopEnemyFrames = 4;     // frames de freeze al golpear boss / romper muro / recibir daño
  static constexpr int kHitstopDefeatFrames = 6;    // frames de freeze al derrotar un boss
  static constexpr float kShakeHitAmp = 3.0f;       // amplitud px sacudida por golpe
  static constexpr float kShakeBreakAmp = 2.5f;     // amplitud px sacudida al romper break_wall
  static constexpr float kShakeBossAmp = 4.5f;      // amplitud px sacudida al derrotar un boss
  static constexpr int kBossBurstFrames = 180;      // ~3.0 s de rayos catódicos al vencer un boss
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
  // Boss dragón multipatrón (type "boss_dragon"): 4 golpes, vuelo + brasas + embestida.
  static constexpr float kDragonW = 32.f, kDragonH = 22.f;
  static constexpr int kDragonHits = 4;
  static constexpr float kDragonRange = 84.f;
  static constexpr float kDragonWalkSec = 1.3f;
  static constexpr float kDragonTellSec = 0.55f;
  static constexpr float kDragonHopSpeed = 240.f;
  static constexpr float kDragonChargeSpeed = 145.f;
  static constexpr float kDragonChargeSec = 0.70f;
  // Boss centinela intermedio (type "boss_sentinel"): guardián del archivo, 3 golpes, salto con abanico triple y carga.
  static constexpr float kSentinelW = 26.f, kSentinelH = 18.f;
  static constexpr int kSentinelHits = 3;
  static constexpr float kSentinelRange = 72.f;
  static constexpr float kSentinelSpeed = 40.f;
  static constexpr float kSentinelSpeedPerHit = 12.f;
  static constexpr float kSentinelWalkSec = 1.4f;
  static constexpr float kSentinelTellSec = 0.50f;
  static constexpr float kSentinelHopSpeed = 220.f;
  static constexpr float kSentinelChargeSpeed = 125.f;
  static constexpr float kSentinelChargeSec = 0.55f;
  static constexpr float kFireballRadius = 3.f;
  static constexpr float kFireballSpeed = 88.f;
  static constexpr float kFireballLife = 0.90f;
  // Enemigo menor (type "imp"): patrulla terrestre, cae con pisotón o embestida (Dash).
  static constexpr float kImpW = 10.f, kImpH = 10.f;
  static constexpr float kImpRange = 32.f, kImpSpeed = 34.f;
  // Enemigo volador (type "bat"): patrulla ondulante aérea, cae con pisotón o Dash.
  static constexpr float kBatW = 10.f, kBatH = 8.f;
  static constexpr float kBatRange = 40.f, kBatSpeed = 38.f;
  // Enemigo acorazado (type "crustacean"): patrulla terrestre con caparazón resistente.
  static constexpr float kCrustaceanW = 14.f, kCrustaceanH = 10.f;
  static constexpr float kCrustaceanRange = 36.f, kCrustaceanSpeed = 24.f;
  static constexpr float kCrustaceanHeavyStompVy = 1.35f;

  // Plataforma sólida que recorre waypoints (sección MOVER / type "mover" o "elevator").
  // (x, y) es la esquina superior izquierda actual, en px; w/h en px (tiles × 8).
  struct Mover {
    float x = 0, y = 0, w = 0, h = 0;
    float speed = 40.f, pause = 0.5f;
    uint16_t tile = 6;
    bool is_elevator = false;
    std::size_t ent = 0;
    float wp0_x = 0, wp0_y = 0, wp1_x = 0, wp1_y = 0;
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
  SurfaceKind surface_under_feet() const;
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
  bool all_eggs_collected() const { const int t = collectible_total(); return t > 0 && collected() >= t; }
  int hp() const { return hp_; }
  int max_hp() const { return collected() >= kEggsForThirdHp ? 3 : (collected() >= kEggsForExtraHp ? 2 : 1); }
  bool dash_upgraded() const { return collected() >= kEggsForDashUpgrade; }
  float hero_walk_speed() const { return hero_ == HeroKind::Opossum ? kOpossumWalkSpeed : kWalkSpeed; }
  int hero_coyote_frames() const { return hero_ == HeroKind::Opossum ? kOpossumCoyoteFrames : kCoyoteFrames; }
  float hero_reveal_reach() const { return hero_ == HeroKind::Lantern ? kLanternRevealReach : kRevealReach; }
  float hero_lantern_radius() const { return hero_ == HeroKind::Lantern ? kLanternLightRadius : kBaseLightRadius; }
  float hero_water_jump_factor() const { return hero_ == HeroKind::Beaver ? kBeaverWaterJumpFactor : kBaseWaterJumpFactor; }
  float hero_wall_slide_speed() const { return hero_ == HeroKind::Beaver ? kBeaverWallSlideSpeed : kWallSlideSpeed; }
  int dash_charges() const { return dash_charges_; }
  int max_dash_charges() const { return has_ability(kAbilityDash) ? (dash_upgraded() ? kDashUpgradedCharges : 1) : 0; }
  int pickup_flash() const { return pickup_flash_; }
  int invuln() const { return invuln_; }
  int mover_count() const { return int(movers_.size()); }
  const Mover& mover(int i) const { return movers_[std::size_t(i)]; }
  // Llama al ascensor desbloqueado cercano hacia el extremo más próximo a (px, py).
  bool call_elevator(float px, float py);
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
    int phase = 0;        // 0 camina, 1 aviso, 2 salto, 3 embestida (boss_dragon)
    float t = 0;          // s en la fase actual
    int dir = -1;         // sentido de la caminata
    float vy = 0;         // px/s (salto)
    float home_x = 0, home_y = 0;
    std::size_t ent = 0;  // índice en Scene::entities()
    bool dragon = false;  // true si es "boss_dragon" multipatrón
    bool guardian2 = false;  // true si es boss_catacomb (Nivel 2, coloso ascendido)
    bool sentinel = false;   // true si es boss_sentinel (Centinela del Archivo)
    int cycle = 0;
    int max_hits() const { return dragon ? kDragonHits : (sentinel ? kSentinelHits : kBossHits); }
    float width() const { return dragon ? kDragonW : (sentinel ? kSentinelW : kBossW); }
    float height() const { return dragon ? kDragonH : (sentinel ? kSentinelH : kBossH); }
    float speed() const {
      const float base = sentinel ? kSentinelSpeed : (guardian2 ? (kBossSpeed + 12.f) : kBossSpeed);
      const float step = sentinel ? kSentinelSpeedPerHit : (kBossSpeedPerHit + (guardian2 ? 7.f : 0.f));
      return base + step * float(hits);
    }
  };
  struct Fireball {
    float x = 0, y = 0, vx = 0, vy = 0, life = 0;
  };
  struct Imp {
    float x = 0, y = 0, home_x = 0, home_y = 0;
    int dir = 1;
    bool defeated = false;
    std::size_t ent = 0;
  };
  struct Bat {
    float x = 0, y = 0, home_x = 0, home_y = 0;
    int dir = 1;
    float t = 0.f;
    bool defeated = false;
    std::size_t ent = 0;
  };
  struct Crustacean {
    float x = 0, y = 0, home_x = 0, home_y = 0;
    int dir = 1;
    bool defeated = false;
    std::size_t ent = 0;
  };
  struct DustParticle {
    float x = 0, y = 0, vx = 0, vy = 0, life = 0, max_life = 0, radius = 1.4f, r = 0.8f, g = 0.78f, b = 0.70f;
  };
  int boss_count() const { return int(bosses_.size()); }
  const Boss& boss(int i) const { return bosses_[std::size_t(i)]; }
  const std::vector<Fireball>& fireballs() const { return fireballs_; }
  int imp_count() const { return int(imps_.size()); }
  const Imp& imp(int i) const { return imps_[std::size_t(i)]; }
  int bat_count() const { return int(bats_.size()); }
  const Bat& bat(int i) const { return bats_[std::size_t(i)]; }
  int crustacean_count() const { return int(crustaceans_.size()); }
  const Crustacean& crustacean(int i) const { return crustaceans_[std::size_t(i)]; }
  float elapsed_sec() const { return elapsed_sec_; }
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
  int dash_ticks() const { return dash_ticks_; }
  bool dash_ready() const { return dash_charges_ > 0; }
  int dash_dir() const { return dash_dir_; }
  int dash_flash() const { return dash_flash_; }
  int wall_break_flash() const { return wall_break_flash_; }
  float wall_break_x() const { return wall_break_x_; }
  float wall_break_y() const { return wall_break_y_; }
  int crystal_break_flash() const { return crystal_break_flash_; }
  float crystal_break_x() const { return crystal_break_x_; }
  float crystal_break_y() const { return crystal_break_y_; }
  // Segundos restantes de un timed_switch (0 = cerrado/disponible). Índice en Scene::entities().
  float timed_switch_left(int entity_idx) const {
    return (entity_idx >= 0 && std::size_t(entity_idx) < timed_left_.size()) ? timed_left_[std::size_t(entity_idx)] : 0.f;
  }
  bool wall_sliding() const { return wall_sliding_; }
  int wall_dir() const { return wall_dir_; }
  int boss_burst() const { return boss_burst_; }
  float boss_burst_x() const { return boss_burst_x_; }
  float boss_burst_y() const { return boss_burst_y_; }
  const BossVictorySeq& victory_seq() const { return victory_; }
  BossVictorySeq& victory_seq() { return victory_; }
  void set_victory_config(const BossVictoryConfig& cfg) { victory_.set_config(cfg); }
  bool take_victory_dialog(BossVictoryDialog& out) { return victory_.poll_dialog(out); }
  bool take_epilogue_dialog(BossVictoryDialog& out);
  // Brisa global y ráfagas locales (vegetación / cuerdas).
  const WindField& wind() const { return wind_; }
  WindField& wind() { return wind_; }
  bool wind_enabled() const { return wind_.enabled(); }
  void set_wind_enabled(bool e) { wind_.set_enabled(e); }
  // Estado visual de animación del protagonista (sin GL).
  HeroKind hero() const { return hero_; }
  void set_hero(HeroKind h) { hero_ = sanitize_hero(uint8_t(h)); }
  int player_facing() const { return facing_; }  // +1 derecha, -1 izquierda
  PlayerAnim player_anim() const { return anim_; }
  int player_anim_frame() const { return anim_frame_; }  // 0..3
  // Game feel (coyote time, jump buffer, squash & stretch, partículas de polvo).
  int coyote_ticks() const { return coyote_ticks_; }
  int jump_buffer_ticks() const { return jump_buf_; }
  float squash_x() const { return squash_x_; }
  float squash_y() const { return squash_y_; }
  const std::vector<DustParticle>& dust() const { return dust_; }
  // Impacto visual (hitstop + sacudida de cámara; desactivable desde pausa).
  bool impact_fx_enabled() const { return impact_fx_; }
  void set_impact_fx_enabled(bool e);
  int hitstop_ticks() const { return hitstop_ticks_; }
  float shake_x() const { return shake_x_; }
  float shake_y() const { return shake_y_; }
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
  int hp_ = 1;
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
    int elev_at = 0;      // waypoint actual del ascensor (0 o 1)
    int elev_target = 0;  // waypoint destino del ascensor (0 o 1)
    float elev_wait = 0;  // s de pausa tras llegar a un extremo
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
  int dash_ticks_ = 0;
  int dash_cd_ = 0;
  int dash_dir_ = 1;
  int dash_charges_ = 0;
  int dash_flash_ = 0;
  int wall_break_flash_ = 0;
  float wall_break_x_ = 0, wall_break_y_ = 0;
  int crystal_break_flash_ = 0;
  float crystal_break_x_ = 0, crystal_break_y_ = 0;
  std::vector<float> timed_left_;  // por entidad: segundos restantes de un timed_switch
  int wall_coyote_ = 0;
  int wall_lock_ticks_ = 0;
  bool wall_sliding_ = false;
  int wall_dir_ = 0;
  int coyote_ticks_ = 0;
  int jump_buf_ = 0;
  float squash_x_ = 1.f;
  float squash_y_ = 1.f;
  std::vector<DustParticle> dust_;
  bool impact_fx_ = true;
  int hitstop_ticks_ = 0;
  int shake_ticks_ = 0;
  int shake_max_ = 0;
  float shake_amp_ = 0.f;
  float shake_x_ = 0.f;
  float shake_y_ = 0.f;
  int boss_burst_ = 0;
  float boss_burst_x_ = 0, boss_burst_y_ = 0;
  BossVictorySeq victory_;
  WindField wind_;
  HeroKind hero_ = HeroKind::Lantern;
  bool was_grounded_ = false;
  ew::physics::Vec2 last_pos_{};
  int facing_ = 1;
  PlayerAnim anim_ = PlayerAnim::Idle;
  int anim_frame_ = 0;
  int anim_ticks_ = 0;
  int air_ticks_ = 0;
  int step_cd_ = 2;
  int pending_dialog_ = -1;
  bool pending_epilogue_ = false;
  float elapsed_sec_ = 0.f;
  std::vector<Boss> bosses_;
  std::vector<Fireball> fireballs_;
  std::vector<Imp> imps_;
  std::vector<Bat> bats_;
  std::vector<Crustacean> crustaceans_;
  void reset_imps();
  void advance_imps();
  void collide_imps();
  void reset_bats();
  void advance_bats();
  void collide_bats();
  void reset_crustaceans();
  void advance_crustaceans();
  void collide_crustaceans(float pre_vy);
  void break_walls_on_dash();
  void spawn_dust(float x, float y, float vx, float vy, float life, float radius, float r, float g, float b);
  void step_game_feel();
  void clear_impact_fx();
  void trigger_impact(int hitstop_frames, float shake_amp, int shake_frames);
  void step_shake();
  void update_player_anim(const Input& in);
  void trigger_entity_dialog(std::size_t entity_idx, ew::map::DialogTrigger trig);
  void check_dialogs_proximity();
  void reset_bosses(bool keep_defeated);
  void advance_bosses();
  void collide_bosses();
  void check_gravity_flips();
  void rearm_flips();
  void check_abilities();
  void reveal_blocks();
  void break_crystals_on_fall(ew::physics::Vec2 pre_pos, float pre_vy);
  void advance_timed_switches();
  void reset_timed_switches();
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
  void take_hit(float src_x, float src_y, bool allow_survive = true);
  void kill();
  void apply_flood();
  uint32_t held_index() const { return ropes_[std::size_t(held_rope_)].first + uint32_t(held_); }
  void release();
};

}  // namespace ew::game
