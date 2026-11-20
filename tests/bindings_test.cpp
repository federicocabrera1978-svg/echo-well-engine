// Controles configurables sin GLFW: defaults, remapeo con intercambio, validación, pad,
// menú de Controles (esperar tecla, cancelar, por defecto) y apply_pad con remapeo.
#include "game/bindings.hpp"
#include "game/gamepad.hpp"
#include "game/menu.hpp"
#include "game/menu_text.hpp"
#include <cstdio>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

static bool g_keys[400];
static bool down(int k, void*) { return k >= 0 && k < 400 && g_keys[k]; }

int main() {
  using namespace ew::game;
  Bindings b = Bindings::defaults();
  CHECK(b.valid());
  CHECK(b.key[kBindJump][0] == key::kSpace && b.key[kBindJump][1] == 'Z');
  CHECK(b.pad[kBindJump][0] == pad::kA && b.pad[kBindLeft][0] == pad::kNone);

  // Teclas.
  g_keys['Z'] = true;
  CHECK(b.key_down(kBindJump, down, nullptr) && !b.key_down(kBindAct, down, nullptr));
  g_keys['Z'] = false;

  // Remapeo simple: salto = K (solo K).
  CHECK(b.bind_key(kBindJump, 'K'));
  CHECK(b.key[kBindJump][0] == 'K' && b.key[kBindJump][1] == key::kNone && b.valid());
  // Intercambio: salto = E (que era de ACCION) → ACCION recibe K.
  CHECK(b.bind_key(kBindJump, 'E'));
  CHECK(b.key[kBindJump][0] == 'E' && b.key[kBindAct][0] == 'K' && b.key[kBindAct][1] == 'X' && b.valid());
  // Tecla secundaria de otra acción: DERECHA = A (secundaria de IZQUIERDA) → IZQUIERDA recibe ←... →.
  CHECK(b.bind_key(kBindRight, 'A'));
  CHECK(b.key[kBindLeft][0] == key::kLeft && b.key[kBindLeft][1] == key::kRight && b.valid());
  // Reservadas / inválidas.
  CHECK(!b.bind_key(kBindJump, key::kEscape) && !b.bind_key(kBindJump, key::kEnter));
  CHECK(!b.bind_key(kBindJump, 5) && !b.bind_key(kBindJump, 999) && !b.bind_key(99, 'Q'));
  CHECK(b.valid());

  // Pad: salto = Y (era de CUERDA) → CUERDA recibe A.
  CHECK(b.bind_pad(kBindJump, pad::kY));
  CHECK(b.pad[kBindJump][0] == pad::kY && b.pad[kBindJump][1] == pad::kNone);
  CHECK(b.pad[kBindGrab][0] == pad::kLB && b.pad[kBindGrab][1] == pad::kA && b.valid());
  CHECK(!b.bind_pad(kBindLeft, pad::kX));                       // mover no se remapea en pad
  CHECK(!b.bind_pad(kBindJump, pad::kStart) && !b.bind_pad(kBindJump, pad::kDUp));
  CHECK(b.pad_down(kBindJump, 1u << pad::kY) && !b.pad_down(kBindJump, 1u << pad::kA));

  // valid() detecta repetidos y huecos.
  Bindings bad = Bindings::defaults();
  bad.key[kBindAct][0] = bad.key[kBindJump][0];
  CHECK(!bad.valid());
  bad = Bindings::defaults(); bad.key[kBindRestart][0] = key::kNone;
  CHECK(!bad.valid());
  bad = Bindings::defaults(); bad.pad[kBindUp][0] = pad::kL3;
  CHECK(!bad.valid());
  bad = Bindings::defaults(); bad.pad[kBindJump] = {int8_t(pad::kNone), int8_t(pad::kA)};
  CHECK(!bad.valid());

  // Nombres para el menú.
  CHECK(key_name('Q') == "Q" && key_name(key::kSpace) == "ESPACIO" && key_name(key::kLeft) == "IZQ");
  CHECK(key_name(key::kF1 + 4) == "F5" && key_name(key::kNone) == "-" && pad_name(pad::kRB) == "RB");

  // apply_pad con remapeo: el salto sale de Y, no de A.
  {
    Input in; PadRaw p; PadEdges e;
    p.buttons = 1u << pad::kA; p.a = true;
    apply_pad(p, in, false, e, 0, Sim::kRestartFullHold, &b);
    CHECK(!in.jump && in.grab);  // A ahora es cuerda
    in = {}; p = {}; e = {};
    p.buttons = 1u << pad::kY; p.y = true;
    apply_pad(p, in, false, e, 0, Sim::kRestartFullHold, &b);
    CHECK(in.jump && !in.grab);
  }

  // Menú: Controles → remapear salto con una tecla; Esc cancela la espera; por defecto.
  PauseMenu m;
  MenuInput in;
  in.toggle = true; m.handle(in);
  for (int i = 0; i < PauseMenu::kControles; ++i) { in = {}; in.down = true; m.handle(in); }
  CHECK(m.cursor() == PauseMenu::kControles);
  in = {}; in.confirm = true; m.handle(in);
  CHECK(m.in_controls() && m.ctl_cursor() == 0);
  for (int i = 0; i < kBindJump; ++i) { in = {}; in.down = true; m.handle(in); }
  CHECK(m.ctl_cursor() == kBindJump);
  in = {}; in.confirm = true; m.handle(in);
  CHECK(m.waiting());
  in = {}; in.key = key::kEnter; m.handle(in);  // reservada: sigue esperando
  CHECK(m.waiting());
  in = {}; in.pad_button = pad::kStart; m.handle(in);
  CHECK(m.waiting());
  in = {}; in.key = 'J'; m.handle(in);
  CHECK(!m.waiting() && m.bindings().key[kBindJump][0] == 'J' && m.take_bindings_changed() && !m.take_bindings_changed());
  in = {}; in.confirm = true; m.handle(in);
  CHECK(m.waiting());
  in = {}; in.toggle = true; m.handle(in);  // Esc cancela sin cerrar ni cambiar
  CHECK(!m.waiting() && m.in_controls() && m.bindings().key[kBindJump][0] == 'J' && !m.take_bindings_changed());
  in = {}; in.confirm = true; m.handle(in);
  in = {}; in.pad_button = pad::kR3; m.handle(in);
  CHECK(!m.waiting() && m.bindings().pad[kBindJump][0] == pad::kR3 && m.take_bindings_changed());
  // Por defecto.
  for (int i = kBindJump; i < PauseMenu::kCtlDefaults; ++i) { in = {}; in.down = true; m.handle(in); }
  CHECK(m.ctl_cursor() == PauseMenu::kCtlDefaults);
  in = {}; in.confirm = true; m.handle(in);
  CHECK(m.bindings() == Bindings::defaults() && m.take_bindings_changed());
  // Volver (fila) y Esc (vuelve a la página principal sin cerrar el menú).
  in = {}; in.down = true; m.handle(in);
  CHECK(m.ctl_cursor() == PauseMenu::kCtlBack);
  in = {}; in.confirm = true; m.handle(in);
  CHECK(!m.in_controls() && m.open() && m.cursor() == PauseMenu::kControles);
  in = {}; in.confirm = true; m.handle(in);
  in = {}; in.toggle = true; m.handle(in);
  CHECK(!m.in_controls() && m.open());
  in = {}; in.toggle = true;
  CHECK(m.handle(in) == MenuAction::Resume && !m.open());
  // set_bindings ignora controles inválidos.
  m.set_bindings(bad);
  CHECK(m.bindings() == Bindings::defaults());
  // Texto de la página Controles: filas, cursor, espera, empaquetado de glifos.
  {
    PauseMenu t;
    MenuInput ti; ti.toggle = true; t.handle(ti);
    t.open_at(PauseMenu::kControles);
    ti = {}; ti.confirm = true; t.handle(ti);
    auto txt = controls_text(t);
    CHECK(txt[0] == "CONTROLES");
    CHECK(txt[1].rfind("> IZQUIERDA", 0) == 0 && txt[1].find("IZQ/A") != std::string::npos);
    CHECK(txt[5].find("ESPACIO/Z") != std::string::npos && txt[5].find("A/RB") != std::string::npos);
    CHECK(txt[9] == "  POR DEFECTO" && txt[10] == "  VOLVER");
    ti = {}; ti.confirm = true; t.handle(ti);
    txt = controls_text(t);
    CHECK(txt[1].find("APRETA UNA TECLA") != std::string::npos && txt[11] == "ESC CANCELA");
    for (const auto& r : txt) {
      CHECK(r.size() <= std::size_t(kTxtCols));
      for (char c : r) CHECK(glyph_index(c) != kGlyphNone);  // todo dibujable
    }
    const auto pk = pack_text(txt);
    CHECK((pk[0] & 63) == glyph_index('C') && ((pk[0] >> 6) & 63) == glyph_index('O'));
    CHECK(((pk[kTxtInts - 1] >> 18) & 63) == kGlyphNone);
  }
  std::puts("bindings ok");
  return 0;
}
