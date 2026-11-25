#pragma once
// Pantalla de final de nivel (sin GL): estadísticas, siguiente nivel, reinicio, pausa y salida.
// Se dibuja en el light pass con u_txt (menu_text.hpp) y estética pixel-art en light.frag.
#include "game/menu.hpp"
#include "game/menu_text.hpp"
#include "game/sim.hpp"
#include "game/story.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace ew::game {

enum class EndAction { None, NextLevel, RestartLevel, OpenMenu, Quit };

struct EndStats {
  std::string title;
  int items = 0, items_total = 0, deaths = 0;
  uint32_t abilities = 0;
  bool has_next = false;
  std::string next_label;
  float elapsed_sec = 0.f;
  bool campaign_done = false;
  bool true_ending = false;
  int max_hp = 1;
  bool dash_upgraded = false;
  int explored_pct = 0;
  bool hard_mode = false;
};

inline char end_rank(const EndStats& s) {
  const bool all_items = (s.items_total == 0) || (s.items >= s.items_total);
  if (all_items && s.deaths == 0) return 'S';
  if ((all_items && s.deaths <= 3) || s.deaths <= 1) return 'A';
  return 'B';
}

class EndScreen {
 public:
  static constexpr int kNext = 0, kRestart = 1, kMenu = 2, kQuit = 3, kItemCount = 4;

  bool open() const { return open_; }
  int cursor() const { return cursor_; }
  const EndStats& stats() const { return stats_; }

  void open(const EndStats& stats) {
    open_ = true;
    stats_ = stats;
    cursor_ = stats_.has_next ? kNext : kRestart;
  }
  void close() { open_ = false; }

  EndAction handle(const MenuInput& in) {
    if (!open_) return EndAction::None;
    if (in.toggle) return EndAction::OpenMenu;
    if (in.up) cursor_ = (cursor_ + kItemCount - 1) % kItemCount;
    if (in.down) cursor_ = (cursor_ + 1) % kItemCount;
    if (!in.confirm) return EndAction::None;
    switch (cursor_) {
      case kNext:
        if (!stats_.has_next) return EndAction::None;
        open_ = false;
        return EndAction::NextLevel;
      case kRestart:
        open_ = false;
        return EndAction::RestartLevel;
      case kMenu:
        return EndAction::OpenMenu;
      case kQuit:
        return EndAction::Quit;
      default:
        return EndAction::None;
    }
  }

 private:
  bool open_ = false;
  int cursor_ = kRestart;
  EndStats stats_{};
};

inline std::array<std::string, kTxtRows> end_screen_text(const EndScreen& e) {
  std::array<std::string, kTxtRows> t{};
  const auto& s = e.stats();
  t[0] = s.true_ending ? std::string(kEndTitleTrue) : (s.campaign_done ? std::string(kEndTitleVictory) : std::string("NIVEL COMPLETADO"));
  t[1] = s.true_ending ? std::string(kEndSubtitleTrue) : (s.title.empty() ? "CAMINO SUPERADO" : s.title);
  const std::string egg_str =
      s.items_total > 0 ? std::to_string(s.items) + "/" + std::to_string(s.items_total) : std::string("-");
  t[2] = (s.explored_pct > 0)
             ? ("HUEVOS: " + egg_str + "   MAPA: " + std::to_string(s.explored_pct))
             : ("HUEVOS:  " + egg_str);
  t[3] = "MUERTES: " + std::to_string(s.deaths) + (s.hard_mode ? "   DESAFIO" : "");
  auto build_abs = [&](bool compact) {
    const std::string sep = compact ? "/" : " + ";
    std::string out;
    auto add = [&](std::string_view tag) {
      if (!out.empty()) out += sep;
      out += tag;
    };
    if (s.abilities & kAbilityDoubleJump) add(compact ? "DJ" : "DOBLE SALTO");
    if ((s.abilities & kAbilityDash) || s.dash_upgraded) add(s.dash_upgraded ? "DASH II" : "DASH");
    if (s.abilities & kAbilityWallClimb) add(compact ? "ESC" : "ESCALADA");
    if (s.abilities & kAbilityReveal) add("FAROL");
    if (s.max_hp > 1) add("+" + std::to_string(s.max_hp - 1) + "HP");
    return out;
  };
  const std::string abs_full = build_abs(false);
  if (abs_full.empty()) {
    t[4] = "HABILIDADES: NINGUNA";
  } else if (std::string("HABILIDADES: " + abs_full).size() <= std::size_t(kTxtCols)) {
    t[4] = "HABILIDADES: " + abs_full;
  } else {
    t[4] = "HAB: " + build_abs(true);
  }
  if (s.elapsed_sec > 0.f || s.campaign_done) {
    const int secs = s.elapsed_sec > 0.f ? int(s.elapsed_sec) : 0;
    const int mm = secs / 60 > 99 ? 99 : secs / 60;
    const int ss = secs % 60;
    const std::string tm = std::to_string(mm) + ":" + (ss < 10 ? "0" : "") + std::to_string(ss);
    t[5] = "TIEMPO:  " + tm + "   RANGO: " + std::string(1, end_rank(s));
  }
  auto row = [&](int idx, const std::string& label) {
    return std::string(e.cursor() == idx ? "> " : "  ") + label;
  };
  const std::string next_lbl = s.has_next
                                   ? (s.next_label.empty() ? std::string("SIGUIENTE NIVEL") : s.next_label)
                                   : std::string("FIN DE LA AVENTURA");
  t[6] = row(EndScreen::kNext, next_lbl);
  t[7] = row(EndScreen::kRestart, "REINICIAR NIVEL");
  t[8] = row(EndScreen::kMenu, "MENU DE PAUSA");
  t[9] = row(EndScreen::kQuit, "SALIR");
  t[11] = "ENTER CONFIRMA   R CHECKPOINT";
  for (auto& line : t)
    if (line.size() > std::size_t(kTxtCols)) line.resize(std::size_t(kTxtCols));
  return t;
}

}  // namespace ew::game
