#pragma once
// Caja de diálogo y registros de lore/NPC sin GL. Se empaqueta en u_txt y se dibuja en light.frag.
#include "game/menu_text.hpp"
#include "map/ewmap_format.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ew::game {

struct DialogPage {
  std::string line0, line1;
};

struct Dialog {
  std::size_t entity = 0;
  std::string speaker, text;
  ew::map::DialogTrigger trigger = ew::map::DialogTrigger::Interact;
  bool once = false;
  bool triggered = false;
};

namespace detail {
inline std::string normalize_dlg_str(std::string_view sv) {
  std::size_t b = 0, e = sv.size();
  while (b < e && (sv[b] == ' ' || sv[b] == '\t')) ++b;
  while (e > b && (sv[e - 1] == ' ' || sv[e - 1] == '\t')) --e;
  std::string out;
  out.reserve(e - b);
  for (std::size_t i = b; i < e; ++i) {
    char c = sv[i];
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    out.push_back(c);
  }
  return out;
}
}  // namespace detail

class DialogueBox {
 public:
  static constexpr std::size_t kLineMax = std::size_t(kTxtCols - 2);  // 30 glifos por línea

  bool open() const { return open_; }
  int page() const { return page_; }
  int page_count() const { return int(pages_.size()); }
  bool has_more() const { return open_ && (page_ + 1 < int(pages_.size())); }
  const std::string& speaker() const { return speaker_; }
  const DialogPage& current_page() const {
    static const DialogPage kEmpty{};
    if (!open_ || page_ < 0 || std::size_t(page_) >= pages_.size()) return kEmpty;
    return pages_[std::size_t(page_)];
  }

  void open(std::string speaker, std::string_view text) {
    speaker_ = detail::normalize_dlg_str(speaker);
    if (speaker_.size() > std::size_t(kTxtCols)) speaker_.resize(std::size_t(kTxtCols));
    pages_.clear();
    std::size_t pos = 0;
    while (pos <= text.size()) {
      const std::size_t pipe = text.find('|', pos);
      const std::string_view chunk = (pipe == std::string_view::npos)
                                         ? text.substr(pos)
                                         : text.substr(pos, pipe - pos);
      DialogPage pg;
      const std::size_t slash = chunk.find('/');
      if (slash != std::string_view::npos) {
        pg.line0 = detail::normalize_dlg_str(chunk.substr(0, slash));
        pg.line1 = detail::normalize_dlg_str(chunk.substr(slash + 1));
      } else {
        std::string all = detail::normalize_dlg_str(chunk);
        if (all.size() > kLineMax) {
          std::size_t sp = all.rfind(' ', kLineMax);
          if (sp != std::string::npos && sp > 0) {
            pg.line0 = detail::normalize_dlg_str(std::string_view(all).substr(0, sp));
            pg.line1 = detail::normalize_dlg_str(std::string_view(all).substr(sp + 1));
          } else {
            pg.line0 = all.substr(0, kLineMax);
            pg.line1 = all.substr(kLineMax);
          }
        } else {
          pg.line0 = std::move(all);
        }
      }
      if (pg.line0.size() > kLineMax) pg.line0.resize(kLineMax);
      if (pg.line1.size() > kLineMax) pg.line1.resize(kLineMax);
      if (!pg.line0.empty() || !pg.line1.empty()) pages_.push_back(std::move(pg));
      if (pipe == std::string_view::npos) break;
      pos = pipe + 1;
    }
    if (pages_.empty()) pages_.push_back({});
    page_ = 0;
    open_ = true;
  }

  // Avanza a la página siguiente; en la última página cierra la caja y devuelve false.
  bool advance() {
    if (!open_) return false;
    if (page_ + 1 < int(pages_.size())) {
      ++page_;
      return true;
    }
    open_ = false;
    return false;
  }

  void close() { open_ = false; }

 private:
  bool open_ = false;
  int page_ = 0;
  std::string speaker_;
  std::vector<DialogPage> pages_;
};

inline std::array<std::string, kTxtRows> dialogue_text(const DialogueBox& d) {
  std::array<std::string, kTxtRows> t{};
  if (!d.open()) return t;
  t[0] = d.speaker();
  const auto& pg = d.current_page();
  t[1] = pg.line0;
  t[2] = pg.line1;
  t[3] = d.has_more()
             ? ("PAG " + std::to_string(d.page() + 1) + "/" + std::to_string(d.page_count()) + "  ENTER/E SIGUIENTE")
             : "ENTER/E CERRAR";
  for (auto& line : t)
    if (line.size() > std::size_t(kTxtCols)) line.resize(std::size_t(kTxtCols));
  return t;
}

}  // namespace ew::game
