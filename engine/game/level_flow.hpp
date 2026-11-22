#pragma once
// Ciclo de mapa, pase de nivel, guardado y estado de meta sin dependencias de GL/GLFW.
#include "game/dialogue.hpp"
#include "game/end_screen.hpp"
#include "game/hud.hpp"
#include "game/menu.hpp"
#include "game/save.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ew::game {

struct LevelFlowConfig {
  std::string map_path;
  std::string next_map_override;
  bool explicit_map = false;
  std::string root;
  std::string build_maps_dir;
  bool has_start = false;
  float start_x = 0.f;
  float start_y = 0.f;
};

// Coordina carga/transición de niveles, autoguardado y detección de meta.
class LevelFlow {
 public:
  struct PostStepResult {
    bool opened_end = false;
    bool cleared_end = false;
    bool save_failed = false;
  };

  bool init(const LevelFlowConfig& cfg, std::string& err);
  void init_save(bool nosave, bool demo, bool has_start, int max_frames, bool explicit_dir, PauseMenu& menu,
                 std::vector<std::string>* logs = nullptr);

  std::string resolve_next_map() const;
  bool load_level(const std::string& path, uint32_t carry_abilities, EndScreen& end_screen, DialogueBox& dialogue,
                  HudTracker& hud, std::string& err, HeroKind carry_hero = HeroKind::Lantern);
  bool continue_save(PauseMenu& menu, EndScreen& end_screen, DialogueBox& dialogue, HudTracker& hud,
                     std::string& err);
  void restart_level(EndScreen& end_screen, DialogueBox& dialogue);
  bool next_level(const PauseMenu& menu, EndScreen& end_screen, DialogueBox& dialogue, HudTracker& hud,
                  std::string& err);
  PostStepResult post_step(PauseMenu& menu, EndScreen& end_screen, const DialogueBox& dialogue, bool did_restart);
  bool consume_tiles_dirty();

  Scene& scene() { return *scene_; }
  const Scene& scene() const { return *scene_; }
  Sim& sim() { return *sim_; }
  const Sim& sim() const { return *sim_; }
  bool ready() const { return scene_ != nullptr && sim_ != nullptr; }

  const std::string& map_path() const { return map_path_; }
  const std::string& initial_map_path() const { return initial_map_path_; }
  const std::string& save_path() const { return save_path_; }
  bool save_armed() const { return save_armed_; }
  bool announced_goal() const { return announced_goal_; }
  bool last_save_ok() const { return last_save_ok_; }

 private:
  struct SaveKey {
    std::string map;
    int ck = -1;
    int items = 0;
    uint32_t abilities = 0;
    HeroKind hero = HeroKind::Lantern;
    bool boss = false;
    bool muted = false;
    float vol = 0.f;
    bool operator==(const SaveKey&) const = default;
  };

  SaveKey current_save_key(const PauseMenu& menu) const;
  bool write_save(SaveData d, const PauseMenu& menu);

  std::unique_ptr<Scene> scene_;
  std::unique_ptr<Sim> sim_;
  std::string map_path_;
  std::string initial_map_path_;
  std::string next_map_override_;
  bool explicit_map_ = false;
  std::string root_;
  std::string build_maps_dir_;
  std::string save_path_;
  bool save_armed_ = true;
  SaveData loaded_{};
  SaveKey last_key_{};
  bool announced_goal_ = false;
  bool tiles_dirty_ = false;
  bool last_save_ok_ = true;
};

}  // namespace ew::game
