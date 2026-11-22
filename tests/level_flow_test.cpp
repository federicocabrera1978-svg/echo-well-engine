// Test sin GL: frame_view.hpp, hud.hpp y level_flow.{hpp,cpp}.
#include "core/paths.hpp"
#include "game/frame_view.hpp"
#include "game/hud.hpp"
#include "game/level_flow.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)
using namespace ew::game;

static void set_env(const char* k, const std::string& v) {
#ifdef _WIN32
  _putenv_s(k, v.c_str());
#else
  setenv(k, v.c_str(), 1);
#endif
}

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  const std::string slice_path = argv[1];
  const std::string level2_path = argv[2];

  const auto tmp_dir = std::filesystem::temp_directory_path() / "ew_level_flow_test";
  std::filesystem::remove_all(tmp_dir);
  std::filesystem::create_directories(tmp_dir);
  set_env("EW_SAVE_DIR", tmp_dir.string());

  // --- 1) LevelFlow::init + --start + build_world_frame ---
  {
    LevelFlow flow;
    LevelFlowConfig cfg;
    cfg.map_path = slice_path;
    cfg.build_maps_dir = ew::core::parent_path(slice_path);
    cfg.has_start = true;
    cfg.start_x = 120.f;
    cfg.start_y = 80.f;
    std::string err;
    CHECK(flow.init(cfg, err));
    CHECK(flow.ready());
    CHECK(std::fabs(flow.sim().player().pos.x - 120.f) < 1e-3f);
    CHECK(std::fabs(flow.sim().player().pos.y - 80.f) < 1e-3f);
    CHECK(flow.resolve_next_map() == level2_path ||
          std::filesystem::path(flow.resolve_next_map()).filename() == "level2.ewmap");

    FrameData v;
    build_world_frame(v, flow.scene(), flow.sim(), 1.25f, false, false);
    CHECK(v.tiles != nullptr);
    CHECK(v.tiles_version == flow.scene().chunk_version());
    CHECK(v.sprite_count >= 1);
    CHECK(v.marker_count > 0);
    // Sin flecha guía orbitando al lado del jugador (solo switches tienen su propio indicador ↓ arriba).
    const auto ga = compute_guide_arrow(flow.scene(), flow.sim(), 1.25f);
    CHECK(ga.valid);
    bool has_player_orbit_arrow = false;
    for (int i = 0; i < v.marker_count; ++i) {
      if (int(v.markers[std::size_t(i * 4 + 3)]) == 4 &&
          std::fabs(v.markers[std::size_t(i * 4 + 0)] - ga.x) < 1.f &&
          std::fabs(v.markers[std::size_t(i * 4 + 1)] - ga.y) < 1.f) {
        has_player_orbit_arrow = true;
      }
    }
    CHECK(!has_player_orbit_arrow);
    CHECK(std::fabs(v.light_x - flow.sim().player().pos.x) < 1e-3f);
    CHECK(std::fabs(v.light_y - flow.sim().player().pos.y) < 1e-3f);
    CHECK(v.foliage_enabled == 1);

    // Culling fuera de vista y tope kMaxMarkers / kMaxSprites
    const int base_m = v.marker_count;
    push_marker(v, v.cam_x - 200.f, v.cam_y, 2.f, 0, 1.f, 1.f, 1.f);
    CHECK(v.marker_count == base_m);
    for (int i = 0; i < kMaxMarkers + 8; ++i)
      push_marker(v, v.cam_x + 16.f, v.cam_y + 16.f, 2.f, 0, 1.f, 1.f, 1.f);
    CHECK(v.marker_count == kMaxMarkers);

    const int base_s = v.sprite_count;
    push_sprite(v, v.cam_x - 200.f, v.cam_y, 0, 0);
    CHECK(v.sprite_count == base_s);
    for (int i = 0; i < kMaxSprites + 8; ++i)
      push_sprite(v, v.cam_x + 16.f, v.cam_y + 16.f, 0, 0);
    CHECK(v.sprite_count == kMaxSprites);
  }

  // --- 2) populate_hud_and_overlays + HudTracker ---
  {
    LevelFlow flow;
    LevelFlowConfig cfg;
    cfg.map_path = slice_path;
    cfg.build_maps_dir = ew::core::parent_path(slice_path);
    std::string err;
    CHECK(flow.init(cfg, err));

    PauseMenu menu;
    EndScreen end_screen;
    DialogueBox dialogue;
    HudTracker hud;

    menu.open_at(PauseMenu::kSeguir);
    menu.set_audio(0.6f, false);
    menu.set_has_save(true);
    FrameData v;
    auto ev = populate_hud_and_overlays(v, flow.scene(), flow.sim(), menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(v.pause == 1 && v.pause_sel == PauseMenu::kSeguir && v.pause_has_save == 1);
    CHECK(std::fabs(v.pause_vol - 0.6f) < 1e-5f);
    CHECK(v.ctl == 0 && v.end_screen == 0 && v.dlg == 0);
    CHECK(v.ctl_txt == pack_text([&] {
            std::array<std::string, kTxtRows> t{};
            t[kTxtRows - 1] = "V0.1.0";
            return t;
          }()));
    CHECK(!ev.new_item && !ev.new_double_jump && !ev.new_dash);

    // Desbloqueo de habilidades y collectible
    flow.sim().grant_ability(kAbilityDoubleJump);
    flow.sim().grant_ability(kAbilityDash);
    for (std::size_t i = 0; i < flow.scene().entities().size(); ++i) {
      if (flow.scene().entities()[i].type == "collectible") {
        flow.scene().set_active(i, true);
        break;
      }
    }
    menu.handle(MenuInput{.toggle = true});  // cierra pausa
    dialogue.open("CRONISTA", "HOLA|MUNDO");
    ev = populate_hud_and_overlays(v, flow.scene(), flow.sim(), menu, end_screen, dialogue, hud, "0.1.0");
    CHECK(ev.new_item && ev.new_double_jump && ev.new_dash);
    CHECK(v.pause == 0 && v.dlg == 1 && v.dlg_more == 1);
    CHECK(v.items == 1 && v.abilities == int(kAbilityDoubleJump | kAbilityDash));
  }

  // --- 3) Guardado, post_step, next_level, continue_save y restart_level ---
  {
    LevelFlow flow;
    LevelFlowConfig cfg;
    cfg.map_path = slice_path;
    cfg.build_maps_dir = ew::core::parent_path(slice_path);
    std::string err;
    CHECK(flow.init(cfg, err));

    PauseMenu menu;
    EndScreen end_screen;
    DialogueBox dialogue;
    HudTracker hud;
    std::vector<std::string> logs;
    flow.init_save(false, false, false, 0, true, menu, &logs);
    CHECK(!flow.save_path().empty() && !logs.empty());

    // Alcanzar checkpoint + habilidad dispara autoguardado en post_step
    for (std::size_t i = 0; i < flow.scene().entities().size(); ++i) {
      const auto& e = flow.scene().entities()[i];
      if (e.type == "checkpoint") {
        auto& p = flow.sim().world().points()[flow.sim().player_index()];
        p.pos = p.prev = {e.x, e.y - 4.f};
        for (int k = 0; k < 5; ++k) flow.sim().step({});
        break;
      }
    }
    flow.sim().grant_ability(kAbilityDoubleJump);
    const auto post1 = flow.post_step(menu, end_screen, dialogue, false);
    CHECK(!post1.save_failed && menu.has_save() && flow.save_armed());

    // Vencer bosses (si hay) y llegar a la meta abre EndScreen en post_step
    for (std::size_t i = 0; i < flow.scene().entities().size(); ++i) {
      if (flow.scene().entities()[i].type == "boss") flow.scene().set_active(i, true);
    }
    flow.sim().load_state(flow.sim().checkpoint_index(), false, [&] {
      std::vector<uint8_t> act(flow.scene().entities().size(), 0);
      for (std::size_t i = 0; i < act.size(); ++i) act[i] = flow.scene().entities()[i].active ? 1 : 0;
      return act;
    }(), flow.sim().abilities());
    if (const auto* g = flow.scene().find_type("goal")) {
      auto& p = flow.sim().world().points()[flow.sim().player_index()];
      p.pos = p.prev = {g->x, g->y};
      flow.sim().step({});
    }
    CHECK(flow.sim().finished());
    const auto post2 = flow.post_step(menu, end_screen, dialogue, false);
    CHECK(post2.opened_end && end_screen.open() && flow.announced_goal());

    // Pasar al siguiente nivel conserva habilidades y marca tiles_dirty
    CHECK(flow.next_level(menu, end_screen, dialogue, hud, err));
    CHECK(flow.consume_tiles_dirty());
    CHECK(!flow.consume_tiles_dirty());
    CHECK(flow.scene().map().string_at(1) == "level2");
    CHECK(flow.sim().has_ability(kAbilityDoubleJump));
    CHECK(!end_screen.open() && !flow.announced_goal());

    // restart_level resetea estado y cierra overlays
    end_screen.open(EndStats{});
    dialogue.open("A", "B");
    flow.restart_level(end_screen, dialogue);
    CHECK(!end_screen.open() && !dialogue.open() && !flow.announced_goal());

    // Nueva sesión con LevelFlow: init_save ofrece CONTINUAR hacia level2
    LevelFlow flow2;
    CHECK(flow2.init(cfg, err));
    PauseMenu menu2;
    EndScreen end2;
    DialogueBox dlg2;
    HudTracker hud2;
    flow2.init_save(false, false, false, 0, true, menu2);
    CHECK(menu2.has_save() && menu2.open() && menu2.cursor() == PauseMenu::kContinuar);
    CHECK(flow2.continue_save(menu2, end2, dlg2, hud2, err));
    CHECK(flow2.consume_tiles_dirty());
    CHECK(flow2.scene().map().string_at(1) == "level2");
    CHECK(flow2.sim().has_ability(kAbilityDoubleJump));
  }

  std::filesystem::remove_all(tmp_dir);
  std::printf("level_flow_test OK\n");
  return 0;
}
