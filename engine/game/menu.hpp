#pragma once
// Menú de pausa (sin GL): abrir/cerrar, cursor, volumen/mute, continuar partida, acciones.
// echo_engine lo dibuja en el light pass y aplica el volumen al Mixer.
#include <algorithm>

namespace ew::game {

enum class MenuAction { None, Resume, Continue, Restart, Quit };

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
  static constexpr int kItemCount = 5;  // Seguir, Continuar, Reiniciar, Volumen, Salir
  static constexpr int kSeguir = 0, kContinuar = 1, kReiniciar = 2, kVolumen = 3, kSalir = 4;
  static constexpr float kVolStep = 0.10f;
  static constexpr float kVolDefault = 0.80f;

  bool open() const { return open_; }
  int cursor() const { return cursor_; }
  // Ganancia efectiva que debe oírse (0 si mute).
  float volume() const { return muted_ ? 0.f : volume_; }
  float volume_setting() const { return volume_; }
  bool muted() const { return muted_; }
  // Hay partida guardada: habilita "Continuar" (sin save, confirm ahí no hace nada).
  bool has_save() const { return has_save_; }
  void set_has_save(bool s) { has_save_ = s; }
  // Restaura volumen/mute (p. ej. desde el save). Clampa a [0, 1].
  void set_audio(float vol, bool muted) {
    volume_ = std::clamp(vol, 0.f, 1.f);
    muted_ = muted || volume_ <= 0.f;
  }
  // Abre el menú con el cursor en `item` (al arrancar con save: en Continuar).
  void open_at(int item) {
    open_ = true;
    cursor_ = (item >= 0 && item < kItemCount) ? item : kSeguir;
  }

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
      case kContinuar:
        if (!has_save_) return MenuAction::None;
        open_ = false;
        return MenuAction::Continue;
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
  bool has_save_ = false;
};

}  // namespace ew::game
