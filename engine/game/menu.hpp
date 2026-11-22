#pragma once
// Menú de pausa (sin GL): abrir/cerrar, cursor, volumen/mute, continuar partida, controles
// (remapeo de teclado y pad) y acciones. echo_engine lo dibuja en el light pass, aplica el
// volumen al Mixer y lee las teclas/botones con bindings().
#include "game/bindings.hpp"
#include "game/hero.hpp"

#include <algorithm>

namespace ew::game {

enum class MenuAction { None, Resume, Continue, Restart, Quit };

struct MenuInput {
  bool toggle = false;   // flanco Esc / Start (/ B): abre o cierra; en Controles vuelve atrás
  bool up = false;       // flanco
  bool down = false;
  bool left = false;     // volumen −
  bool right = false;    // volumen +
  bool confirm = false;  // flanco Enter / A
  int key = -1;          // tecla recién apretada (GLFW_KEY_*), para remapear
  int pad_button = -1;   // botón de pad recién apretado (GLFW_GAMEPAD_BUTTON_*)
};

class PauseMenu {
 public:
  static constexpr int kItemCount = 9;  // Seguir, Continuar, Reiniciar, Volumen, Controles, Brisa, Impacto, Héroe, Salir
  static constexpr int kSeguir = 0, kContinuar = 1, kReiniciar = 2, kVolumen = 3, kControles = 4, kBrisa = 5,
                       kImpact = 6, kHero = 7, kSalir = 8;
  // Página Controles: una fila por acción + POR DEFECTO + VOLVER.
  static constexpr int kCtlDefaults = kBindCount, kCtlBack = kBindCount + 1, kCtlRows = kBindCount + 2;
  static constexpr float kVolStep = 0.10f;
  static constexpr float kVolDefault = 0.80f;

  bool open() const { return open_; }
  int cursor() const { return cursor_; }
  bool in_controls() const { return open_ && page_ == 1; }
  int ctl_cursor() const { return ctl_cursor_; }
  bool in_hero_select() const { return open_ && page_ == 2; }
  int hero_cursor() const { return hero_cursor_; }
  bool waiting() const { return open_ && waiting_; }  // esperando la tecla/botón nuevo
  // Ganancia efectiva que debe oírse (0 si mute).
  float volume() const { return muted_ ? 0.f : volume_; }
  float volume_setting() const { return volume_; }
  bool muted() const { return muted_; }
  // Hay partida guardada: habilita "Continuar" (sin save, confirm ahí no hace nada).
  bool has_save() const { return has_save_; }
  void set_has_save(bool s) { has_save_ = s; }
  // Brisa y vegetación animada.
  bool foliage() const { return foliage_; }
  void set_foliage(bool f) { foliage_ = f; }
  // Impacto visual (hitstop + sacudida de cámara).
  bool impact_fx() const { return impact_fx_; }
  void set_impact_fx(bool e) { impact_fx_ = e; }
  // Héroe seleccionado.
  HeroKind hero() const { return hero_; }
  void set_hero(HeroKind h) {
    hero_ = sanitize_hero(uint8_t(h));
    hero_cursor_ = int(hero_);
  }
  // Abre el selector de héroe (con start_only = true, ESC entra directo al juego).
  void open_hero_select(bool start_only = false) {
    open_ = true;
    page_ = 2;
    waiting_ = false;
    hero_cursor_ = int(hero_);
    hero_start_only_ = start_only;
  }
  // Restaura volumen/mute (p. ej. desde el save). Clampa a [0, 1].
  void set_audio(float vol, bool muted) {
    volume_ = std::clamp(vol, 0.f, 1.f);
    muted_ = muted || volume_ <= 0.f;
  }
  const Bindings& bindings() const { return bind_; }
  void set_bindings(const Bindings& b) { if (b.valid()) bind_ = b; }
  // true una vez después de cada cambio de controles (para guardarlos).
  bool take_bindings_changed() { const bool c = bind_changed_; bind_changed_ = false; return c; }
  // Abre el menú con el cursor en `item` (al arrancar con save: en Continuar).
  void open_at(int item) {
    open_ = true;
    page_ = 0;
    waiting_ = false;
    hero_start_only_ = false;
    cursor_ = (item >= 0 && item < kItemCount) ? item : kSeguir;
  }

  MenuAction handle(const MenuInput& in) {
    if (open_ && waiting_) return handle_wait(in);
    if (in.toggle) {
      if (open_ && page_ == 1) {  // Esc en Controles: vuelve al menú
        page_ = 0;
        return MenuAction::None;
      }
      if (open_ && page_ == 2) {  // Esc en selector de héroe: vuelve al menú o entra al juego si es inicio
        if (hero_start_only_) {
          open_ = false;
          page_ = 0;
          hero_start_only_ = false;
          return MenuAction::Resume;
        }
        page_ = 0;
        cursor_ = kHero;
        return MenuAction::None;
      }
      open_ = !open_;
      if (open_) { cursor_ = kSeguir; page_ = 0; hero_start_only_ = false; }
      return open_ ? MenuAction::None : MenuAction::Resume;
    }
    if (!open_) return MenuAction::None;
    if (page_ == 1) return handle_controls(in);
    if (page_ == 2) return handle_hero_select(in);

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
    if (cursor_ == kBrisa && (in.left || in.right || in.confirm)) {
      foliage_ = !foliage_;
      return MenuAction::None;
    }
    if (cursor_ == kImpact && (in.left || in.right || in.confirm)) {
      impact_fx_ = !impact_fx_;
      return MenuAction::None;
    }
    if (cursor_ == kHero) {
      if (in.left) hero_ = cycle_hero(hero_, -1);
      if (in.right) hero_ = cycle_hero(hero_, +1);
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
      case kControles: page_ = 1; ctl_cursor_ = 0; return MenuAction::None;
      case kBrisa:     foliage_ = !foliage_; return MenuAction::None;
      case kImpact:    impact_fx_ = !impact_fx_; return MenuAction::None;
      case kHero:
        page_ = 2;
        hero_cursor_ = int(hero_);
        hero_start_only_ = false;
        return MenuAction::None;
      case kSalir:     return MenuAction::Quit;
      default:         return MenuAction::None;
    }
  }

 private:
  MenuAction handle_controls(const MenuInput& in) {
    if (in.up) ctl_cursor_ = (ctl_cursor_ + kCtlRows - 1) % kCtlRows;
    if (in.down) ctl_cursor_ = (ctl_cursor_ + 1) % kCtlRows;
    if (!in.confirm) return MenuAction::None;
    if (ctl_cursor_ < kBindCount) {
      waiting_ = true;
    } else if (ctl_cursor_ == kCtlDefaults) {
      if (!(bind_ == Bindings::defaults())) { bind_ = Bindings::defaults(); bind_changed_ = true; }
    } else {
      page_ = 0;
      cursor_ = kControles;
    }
    return MenuAction::None;
  }
  // Esperando: la próxima tecla o botón asignable queda para la acción; Esc / Start cancela.
  MenuAction handle_wait(const MenuInput& in) {
    if (in.toggle) { waiting_ = false; return MenuAction::None; }
    if (in.key >= 0 && bind_.bind_key(ctl_cursor_, in.key)) {
      waiting_ = false;
      bind_changed_ = true;
    } else if (in.pad_button >= 0 && bind_.bind_pad(ctl_cursor_, in.pad_button)) {
      waiting_ = false;
      bind_changed_ = true;
    }
    return MenuAction::None;
  }

  MenuAction handle_hero_select(const MenuInput& in) {
    if (in.up) hero_cursor_ = (hero_cursor_ + kHeroCount - 1) % kHeroCount;
    if (in.down) hero_cursor_ = (hero_cursor_ + 1) % kHeroCount;
    if (!in.confirm) return MenuAction::None;
    hero_ = sanitize_hero(uint8_t(hero_cursor_));
    open_ = false;
    page_ = 0;
    hero_start_only_ = false;
    return MenuAction::Resume;
  }

  bool open_ = false;
  int page_ = 0;  // 0 = principal, 1 = controles, 2 = elegir héroe
  int cursor_ = 0;
  int ctl_cursor_ = 0;
  int hero_cursor_ = 0;
  bool waiting_ = false;
  bool hero_start_only_ = false;
  float volume_ = kVolDefault;
  bool muted_ = false;
  bool has_save_ = false;
  bool foliage_ = true;
  bool impact_fx_ = true;
  HeroKind hero_ = HeroKind::Lantern;
  Bindings bind_ = Bindings::defaults();
  bool bind_changed_ = false;
};

}  // namespace ew::game
