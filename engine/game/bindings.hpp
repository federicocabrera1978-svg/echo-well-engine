#pragma once
// Controles configurables (sin GLFW, testeable). Códigos de tecla = GLFW_KEY_*; botones de
// pad = GLFW_GAMEPAD_BUTTON_* (0..14). echo_engine traduce el estado de GLFW con esto; el
// menú de pausa remapea y el save (v2) lo persiste.
#include <array>
#include <cstdint>
#include <string>

namespace ew::game {

enum BindAction : int { kBindLeft, kBindRight, kBindUp, kBindDown, kBindJump, kBindAct, kBindGrab, kBindRestart, kBindCount };

// Códigos GLFW usados (copiados para no depender de GLFW en tests/headless).
namespace key {
inline constexpr int kNone = -1, kSpace = 32, kEscape = 256, kEnter = 257, kTab = 258, kBackspace = 259,
                     kRight = 262, kLeft = 263, kDown = 264, kUp = 265, kF1 = 290, kF25 = 314, kKp0 = 320,
                     kKpEqual = 336, kLShift = 340, kLCtrl = 341, kLAlt = 342, kLSuper = 343,
                     kRShift = 344, kRCtrl = 345, kRAlt = 346, kRSuper = 347, kMenu = 348, kLast = 348;
}
namespace pad {
inline constexpr int kNone = -1, kA = 0, kB = 1, kX = 2, kY = 3, kLB = 4, kRB = 5, kBack = 6, kStart = 7,
                     kGuide = 8, kL3 = 9, kR3 = 10, kDUp = 11, kDRight = 12, kDDown = 13, kDLeft = 14, kLast = 14;
}

inline const char* bind_action_name(int a) {
  static const char* const n[kBindCount] = {"IZQUIERDA", "DERECHA", "ARRIBA", "ABAJO", "SALTO", "ACCION", "CUERDA", "REINICIO"};
  return (a >= 0 && a < kBindCount) ? n[a] : "?";
}
// Solo salto / acción / cuerda se remapean en el pad (mover = stick o d-pad; Start = menú).
inline bool pad_bindable_action(int a) { return a == kBindJump || a == kBindAct || a == kBindGrab; }
// Teclas que se pueden asignar: imprimibles y especiales GLFW, salvo Esc/Enter (menú).
inline bool key_bindable(int k) { return k >= key::kSpace && k <= key::kLast && k != key::kEscape && k != key::kEnter; }
// Botones asignables: caras, bumpers y sticks (no Back/Start/Guide ni d-pad).
inline bool pad_bindable(int b) { return (b >= pad::kA && b <= pad::kRB) || b == pad::kL3 || b == pad::kR3; }

struct Bindings {
  std::array<std::array<int16_t, 2>, kBindCount> key{};  // 2 teclas por acción (−1 = nada)
  std::array<std::array<int8_t, 2>, kBindCount> pad{};   // 2 botones (solo acciones pad_bindable)

  static Bindings defaults() {
    Bindings b;
    auto k = [&](int a, int k0, int k1) { b.key[std::size_t(a)] = {int16_t(k0), int16_t(k1)}; };
    k(kBindLeft, key::kLeft, 'A');
    k(kBindRight, key::kRight, 'D');
    k(kBindUp, key::kUp, 'W');
    k(kBindDown, key::kDown, 'S');
    k(kBindJump, key::kSpace, 'Z');
    k(kBindAct, 'E', 'X');
    k(kBindGrab, 'C', key::kLShift);
    k(kBindRestart, 'R', key::kNone);
    for (auto& p : b.pad) p = {int8_t(pad::kNone), int8_t(pad::kNone)};
    b.pad[kBindJump] = {int8_t(pad::kA), int8_t(pad::kRB)};
    b.pad[kBindAct] = {int8_t(pad::kX), int8_t(pad::kB)};
    b.pad[kBindGrab] = {int8_t(pad::kLB), int8_t(pad::kY)};
    return b;
  }

  bool operator==(const Bindings&) const = default;

  // Válido: cada acción con al menos su tecla principal, teclas/botones asignables y sin
  // repetidos entre acciones (lo usa el loader del save).
  bool valid() const {
    for (int a = 0; a < kBindCount; ++a) {
      const auto& ks = key[std::size_t(a)];
      if (!key_bindable(ks[0])) return false;
      if (ks[1] != key::kNone && (!key_bindable(ks[1]) || ks[1] == ks[0])) return false;
      const auto& ps = pad[std::size_t(a)];
      for (int s = 0; s < 2; ++s) {
        if (ps[std::size_t(s)] == pad::kNone) continue;
        if (!pad_bindable_action(a) || !pad_bindable(ps[std::size_t(s)])) return false;
      }
      if (ps[0] != pad::kNone && ps[0] == ps[1]) return false;
      if (ps[0] == pad::kNone && ps[1] != pad::kNone) return false;
      for (int o = a + 1; o < kBindCount; ++o)
        for (int s = 0; s < 2; ++s)
          for (int t = 0; t < 2; ++t) {
            const int k1 = ks[std::size_t(s)], k2 = key[std::size_t(o)][std::size_t(t)];
            if (k1 != key::kNone && k1 == k2) return false;
            const int p1 = ps[std::size_t(s)], p2 = pad[std::size_t(o)][std::size_t(t)];
            if (p1 != pad::kNone && p1 == p2) return false;
          }
    }
    return true;
  }

  bool key_down(int a, bool (*down)(int, void*), void* ctx) const {
    for (int k : key[std::size_t(a)])
      if (k != key::kNone && down(k, ctx)) return true;
    return false;
  }
  // `buttons`: bit i = GLFW_GAMEPAD_BUTTON i apretado.
  bool pad_down(int a, uint32_t buttons) const {
    for (int b : pad[std::size_t(a)])
      if (b != pad::kNone && (buttons >> unsigned(b)) & 1u) return true;
    return false;
  }

  // Remapea: la acción queda con SOLO esa tecla. Si otra acción la usaba, recibe la tecla
  // principal anterior de esta (intercambio), así nunca queda una acción sin tecla.
  bool bind_key(int a, int k) {
    if (a < 0 || a >= kBindCount || !key_bindable(k)) return false;
    auto& mine = key[std::size_t(a)];
    const int16_t old = mine[0];
    for (int o = 0; o < kBindCount; ++o) {
      if (o == a) continue;
      auto& ks = key[std::size_t(o)];
      for (int s = 0; s < 2; ++s)
        if (ks[std::size_t(s)] == k) ks[std::size_t(s)] = (old == ks[std::size_t(1 - s)]) ? int16_t(key::kNone) : old;
      if (ks[0] == key::kNone) std::swap(ks[0], ks[1]);
    }
    mine = {int16_t(k), int16_t(key::kNone)};
    return true;
  }
  // Igual para el pad (solo salto / acción / cuerda).
  bool bind_pad(int a, int b) {
    if (a < 0 || a >= kBindCount || !pad_bindable_action(a) || !pad_bindable(b)) return false;
    auto& mine = pad[std::size_t(a)];
    const int8_t old = mine[0];
    for (int o = 0; o < kBindCount; ++o) {
      if (o == a) continue;
      auto& ps = pad[std::size_t(o)];
      for (int s = 0; s < 2; ++s)
        if (ps[std::size_t(s)] == b) ps[std::size_t(s)] = (old == ps[std::size_t(1 - s)]) ? int8_t(pad::kNone) : old;
      if (ps[0] == pad::kNone) std::swap(ps[0], ps[1]);
    }
    mine = {int8_t(b), int8_t(pad::kNone)};
    return true;
  }
};

// Nombre corto para el menú (A-Z, 0-9, espacio, '-', '/', '.').
inline std::string key_name(int k) {
  if (k == key::kNone) return "-";
  if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) return std::string(1, char(k));
  switch (k) {
    case key::kSpace: return "ESPACIO";
    case key::kLeft: return "IZQ";
    case key::kRight: return "DER";
    case key::kUp: return "ARRIBA";
    case key::kDown: return "ABAJO";
    case key::kTab: return "TAB";
    case key::kBackspace: return "BORRAR";
    case key::kLShift: return "SHIFT";
    case key::kRShift: return "SHIFT D";
    case key::kLCtrl: return "CTRL";
    case key::kRCtrl: return "CTRL D";
    case key::kLAlt: return "ALT";
    case key::kRAlt: return "ALT D";
    case '-': return "-";
    case '/': return "/";
    case '.': return ".";
    default: break;
  }
  if (k >= key::kF1 && k <= key::kF25) return "F" + std::to_string(k - key::kF1 + 1);
  if (k >= key::kKp0 && k <= key::kKp0 + 9) return "KP" + std::to_string(k - key::kKp0);
  return "T" + std::to_string(k);
}
inline std::string pad_name(int b) {
  static const char* const n[] = {"A", "B", "X", "Y", "LB", "RB", "BACK", "START", "GUIDE", "L3", "R3", "D ARR", "D DER", "D ABA", "D IZQ"};
  return (b >= 0 && b <= pad::kLast) ? n[b] : "-";
}

}  // namespace ew::game
