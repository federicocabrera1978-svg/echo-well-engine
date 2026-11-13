#pragma once
// Menú de pausa (sin GL): abrir/cerrar, cursor, volumen/mute, acciones.
// echo_engine lo dibuja en el light pass y aplica el volumen al Mixer.
#include <algorithm>

namespace ew::game {

enum class MenuAction { None, Resume, Restart, Quit };

struct MenuInput {
  bool toggle = false;   // flanco Esc / Start: abre o cierra (cerrar = Resume)
  bool up = false;       // flanco
  bool down = false;
  bool left = false;     // volumen −
  bool right = false;    // volumen +
  bool confirm = false;  // flanco Enter / A
};

class PauseMenu {
 public:
  static constexpr int kItemCount = 4;  // Seguir, Reiniciar, Volumen, Salir
  static constexpr int kSeguir = 0, kReiniciar = 1, kVolumen = 2, kSalir = 3;
  static constexpr float kVolStep = 0.10f;
  static constexpr float kVolDefault = 0.80f;

  bool open() const { return open_; }
  int cursor() const { return cursor_; }
  // Ganancia efectiva que debe oírse (0 si mute).
  float volume() const { return muted_ ? 0.f : volume_; }
  float volume_setting() const { return volume_; }
  bool muted() const { return muted_; }

  MenuAction handle(const MenuInput& in) {
    if (in.toggle) {
      open_ = !open_;
      if (open_) cursor_ = kSeguir;
      return open_ ? MenuAction::None : MenuAction::Resume;
    }
    if (!open_) return MenuAction::None;

    if (in.up) cursor_ = (cursor_ + kItemCount - 1) % kItemCount;
    if (in.down) cursor_ = (cursor_ + 1) % kItemCount;

    if (cursor_ == kVolumen) {
      if (in.left) {
        volume_ = std::max(0.f, volume_ - kVolStep);
        if (volume_ <= 0.f) muted_ = true;
      }
      if (in.right) {
        muted_ = false;
        volume_ = std::min(1.f, volume_ + kVolStep);
      }
    }
    if (!in.confirm) return MenuAction::None;
    switch (cursor_) {
      case kSeguir:    open_ = false; return MenuAction::Resume;
      case kReiniciar: open_ = false; return MenuAction::Restart;
      case kVolumen:   muted_ = !muted_; return MenuAction::None;
      case kSalir:     return MenuAction::Quit;
      default:         return MenuAction::None;
    }
  }

 private:
  bool open_ = false;
  int cursor_ = 0;
  float volume_ = kVolDefault;
  bool muted_ = false;
};

}  // namespace ew::game
