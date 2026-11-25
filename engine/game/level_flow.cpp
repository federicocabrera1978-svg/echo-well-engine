#include "game/level_flow.hpp"

#include "core/paths.hpp"
#include "game/campaign.hpp"
#include "game/minimap.hpp"

namespace ew::game {

LevelFlow::SaveKey LevelFlow::current_save_key(const PauseMenu& menu) const {
  if (!scene_ || !sim_) return {};
  return SaveKey{
      std::string(scene_->map().string_at(1)),
      sim_->checkpoint_index(),
      sim_->collected(),
      sim_->abilities(),
      sim_->hero(),
      sim_->boss_count() > 0 && sim_->bosses_defeated(),
      menu.muted(),
      menu.volume_setting(),
      scene_->visited_chunks(),
  };
}

bool LevelFlow::write_save(SaveData d, const PauseMenu& menu) {
  d.volume = menu.volume_setting();
  d.muted = menu.muted();
  d.bind = menu.bindings();
  last_save_ok_ = (write_save_file(save_path_, d) == SaveError::None);
  return last_save_ok_;
}

bool LevelFlow::init(const LevelFlowConfig& cfg, std::string& err) {
  auto next_scene = std::make_unique<Scene>();
  if (!next_scene->load(cfg.map_path, err)) return false;
  auto next_sim = std::make_unique<Sim>(*next_scene);
  if (cfg.has_start) {  // --start X Y: aparece en otro punto (p. ej. probar otro chunk)
    auto& p = next_sim->world().points()[next_sim->player_index()];
    p.pos = p.prev = {cfg.start_x, cfg.start_y};
    next_scene->follow(cfg.start_x, cfg.start_y);
  }
  scene_ = std::move(next_scene);
  sim_ = std::move(next_sim);
  map_path_ = cfg.map_path;
  initial_map_path_ = cfg.map_path;
  next_map_override_ = cfg.next_map_override;
  explicit_map_ = cfg.explicit_map;
  root_ = cfg.root;
  build_maps_dir_ = cfg.build_maps_dir;
  save_path_.clear();
  save_armed_ = true;
  loaded_ = {};
  announced_goal_ = false;
  tiles_dirty_ = false;
  last_save_ok_ = true;
  last_key_ = current_save_key(PauseMenu{});
  return true;
}

void LevelFlow::init_save(bool nosave, bool demo, bool has_start, int max_frames, bool explicit_dir, PauseMenu& menu,
                          std::vector<std::string>* logs) {
  save_path_.clear();
  save_armed_ = true;
  loaded_ = {};
  if (!nosave && !demo && !has_start && (!max_frames || explicit_dir)) {
    const std::string dir = save_dir(root_);
    if (!dir.empty()) save_path_ = ew::core::join(dir, kSaveFileName);
  }
  auto save_matches = [&](const SaveData& d) {
    return scene_ && d.map == scene_->map().string_at(1) && d.active.size() == scene_->entities().size();
  };
  auto can_continue_save = [&](const SaveData& d) {
    if (save_matches(d)) return true;
    if (explicit_map_) return false;
    return !resolve_map_file(d.map, map_path_, root_, build_maps_dir_).empty();
  };
  auto log = [&](std::string msg) {
    if (logs) logs->push_back(std::move(msg));
  };
  if (!save_path_.empty()) {
    const SaveError e = read_save_file(save_path_, loaded_);
    if (e == SaveError::None && can_continue_save(loaded_)) {
      menu.set_has_save(true);
      menu.set_audio(loaded_.volume, loaded_.muted);
      menu.set_bindings(loaded_.bind);
      menu.set_hero(loaded_.hero);
      if (sim_) sim_->set_hero(loaded_.hero);
      menu.open_at(PauseMenu::kContinuar);
      save_armed_ = false;
      log("echo_engine: partida guardada en " + save_path_ + " — CONTINUAR en el menú");
    } else if (e == SaveError::None) {  // save de otro mapa no disponible: solo audio + controles + héroe
      menu.set_audio(loaded_.volume, loaded_.muted);
      menu.set_bindings(loaded_.bind);
      menu.set_hero(loaded_.hero);
      if (sim_) sim_->set_hero(loaded_.hero);
      log("echo_engine: save de otro mapa (se usan volumen y controles)");
    } else if (e != SaveError::Io) {
      log(std::string("echo_engine: save ignorado (") + save_error_str(e) + ")");
    } else {
      log("echo_engine: guardado en " + save_path_);
    }
  }
  last_key_ = current_save_key(menu);
}

std::string LevelFlow::resolve_next_map() const {
  if (!scene_) return {};
  if (!next_map_override_.empty() && map_path_ == initial_map_path_ && ew::core::file_exists(next_map_override_))
    return next_map_override_;
  const auto next_name = next_campaign_map(scene_->map().string_at(1));
  if (next_name.empty()) return {};
  return resolve_map_file(next_name, map_path_, root_, build_maps_dir_);
}

bool LevelFlow::load_level(const std::string& path, uint32_t carry_abilities, EndScreen& end_screen,
                           DialogueBox& dialogue, HudTracker& hud, std::string& err, HeroKind carry_hero) {
  auto next_scene = std::make_unique<Scene>();
  if (!next_scene->load(path, err)) return false;
  auto next_sim = std::make_unique<Sim>(*next_scene);
  next_sim->set_hero(carry_hero);
  if (carry_abilities) next_sim->set_abilities(carry_abilities);
  scene_ = std::move(next_scene);
  sim_ = std::move(next_sim);
  map_path_ = path;
  tiles_dirty_ = true;
  announced_goal_ = false;
  end_screen.close();
  dialogue.close();
  hud.last_items = sim_->collected();
  hud.last_abilities = sim_->abilities();
  hud.last_max_hp = sim_->max_hp();
  hud.last_dash_up = sim_->dash_upgraded();
  return true;
}

bool LevelFlow::continue_save(PauseMenu& menu, EndScreen& end_screen, DialogueBox& dialogue, HudTracker& hud,
                              std::string& err) {
  if (save_path_.empty() || !scene_ || !sim_) {
    err = save_error_str(SaveError::Io);
    return false;
  }
  SaveData d;
  SaveError e = read_save_file(save_path_, d);
  if (e == SaveError::None) {
    if (d.map != scene_->map().string_at(1)) {
      const std::string target = resolve_map_file(d.map, map_path_, root_, build_maps_dir_);
      if (target.empty()) {
        e = SaveError::Mismatch;
      } else {
        auto next_scene = std::make_unique<Scene>();
        std::string lerr;
        if (!next_scene->load(target, lerr)) {
          e = SaveError::Io;
        } else {
          auto next_sim = std::make_unique<Sim>(*next_scene);
          e = apply_save(*next_sim, d);
          if (e == SaveError::None) {
            scene_ = std::move(next_scene);
            sim_ = std::move(next_sim);
            map_path_ = target;
            tiles_dirty_ = true;
          }
        }
      }
    } else {
      e = apply_save(*sim_, d);
    }
  }
  if (e != SaveError::None) {
    err = save_error_str(e);
    return false;
  }
  loaded_ = d;
  menu.set_audio(d.volume, d.muted);
  menu.set_bindings(d.bind);
  menu.set_hero(sim_->hero());
  save_armed_ = true;
  announced_goal_ = false;
  last_key_ = current_save_key(menu);
  hud.last_items = sim_->collected();  // el huevo cargado no se anuncia como recién agarrado
  hud.last_abilities = sim_->abilities();
  hud.last_max_hp = sim_->max_hp();
  hud.last_dash_up = sim_->dash_upgraded();
  end_screen.close();
  dialogue.close();
  return true;
}

void LevelFlow::restart_level(EndScreen& end_screen, DialogueBox& dialogue) {
  if (sim_) sim_->reset();
  end_screen.close();
  dialogue.close();
  announced_goal_ = false;
}

bool LevelFlow::next_level(const PauseMenu& menu, EndScreen& end_screen, DialogueBox& dialogue, HudTracker& hud,
                           std::string& err) {
  const std::string next_p = resolve_next_map();
  if (next_p.empty()) {
    err = "no hay siguiente nivel";
    return false;
  }
  const uint32_t carry = sim_ ? sim_->abilities() : 0u;
  const HeroKind carry_hero = sim_ ? sim_->hero() : menu.hero();
  if (!load_level(next_p, carry, end_screen, dialogue, hud, err, carry_hero)) return false;
  if (!save_path_.empty()) {
    save_armed_ = true;
    write_save(capture_save(*sim_), menu);
  }
  last_key_ = current_save_key(menu);
  return true;
}

LevelFlow::PostStepResult LevelFlow::post_step(PauseMenu& menu, EndScreen& end_screen, const DialogueBox& dialogue,
                                               bool did_restart) {
  PostStepResult res;
  if (!scene_ || !sim_) return res;
  if (sim_->finished() && !announced_goal_ && !dialogue.open()) {
    EndStats st;
    const auto mname = scene_->map().string_at(1);
    st.title = level_title(mname);
    st.items = sim_->collected();
    st.items_total = sim_->collectible_total();
    st.deaths = sim_->deaths();
    st.abilities = sim_->abilities();
    st.has_next = !resolve_next_map().empty();
    st.elapsed_sec = sim_->elapsed_sec();
    st.campaign_done = !st.has_next && find_campaign_level(mname) != nullptr;
    st.true_ending = st.campaign_done && st.items_total > 0 && st.items >= st.items_total;
    st.max_hp = sim_->max_hp();
    st.dash_upgraded = sim_->dash_upgraded();
    st.explored_pct = compute_minimap(*scene_).explored_pct;
    st.hard_mode = sim_->hard_mode();
    end_screen.open(st);
    announced_goal_ = true;
    res.opened_end = true;
  }
  if (!sim_->finished() && end_screen.open()) end_screen.close();
  if (did_restart && announced_goal_ && !sim_->finished()) {
    announced_goal_ = false;
    res.cleared_end = true;
  }
  if (!save_path_.empty()) {  // autoguardado
    const SaveKey k = current_save_key(menu);
    const bool bind_changed = menu.take_bindings_changed();
    if (!(k == last_key_) || bind_changed) {
      const bool progress = k.map != last_key_.map || k.ck != last_key_.ck || k.items != last_key_.items ||
                            k.abilities != last_key_.abilities || k.boss != last_key_.boss;
      if (progress && (k.ck >= 0 || k.items > 0 || k.abilities != 0 || k.boss)) save_armed_ = true;
      if (save_armed_) {
        if (write_save(capture_save(*sim_), menu)) menu.set_has_save(true);
        else res.save_failed = true;
      } else if (menu.has_save()) {  // solo volumen/controles: no pisar la partida
        if (write_save(loaded_, menu)) menu.set_has_save(true);
        else res.save_failed = true;
      }
      last_key_ = k;
    }
  }
  return res;
}

bool LevelFlow::consume_tiles_dirty() {
  const bool d = tiles_dirty_;
  tiles_dirty_ = false;
  return d;
}

}  // namespace ew::game
