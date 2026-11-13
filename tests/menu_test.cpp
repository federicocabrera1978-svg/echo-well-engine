// Menú de pausa headless: abrir/cerrar, cursor, volumen/mute, acciones.
#include "game/menu.hpp"
#include <cmath>
#include <cstdio>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main() {
  using namespace ew::game;
  PauseMenu m;
  CHECK(!m.open());
  CHECK(m.cursor() == PauseMenu::kSeguir);
  CHECK(std::fabs(m.volume() - PauseMenu::kVolDefault) < 1e-6f);
  CHECK(!m.muted());

  // Input con el menú cerrado se ignora (salvo toggle).
  MenuInput in;
  in.confirm = in.down = in.up = true;
  CHECK(m.handle(in) == MenuAction::None);
  CHECK(!m.open() && m.cursor() == PauseMenu::kSeguir);

  // Esc/Start abre; otro toggle cierra y Resume.
  in = {}; in.toggle = true;
  CHECK(m.handle(in) == MenuAction::None);
  CHECK(m.open() && m.cursor() == PauseMenu::kSeguir);
  in = {}; in.toggle = true;
  CHECK(m.handle(in) == MenuAction::Resume);
  CHECK(!m.open());

  // Navegación con wrap y confirm: Seguir, Reiniciar, Salir.
  in = {}; in.toggle = true; m.handle(in);
  in = {}; in.confirm = true;
  CHECK(m.handle(in) == MenuAction::Resume);
  CHECK(!m.open());

  in = {}; in.toggle = true; m.handle(in);
  in = {}; in.down = true; m.handle(in);
  CHECK(m.cursor() == PauseMenu::kReiniciar);
  in = {}; in.confirm = true;
  CHECK(m.handle(in) == MenuAction::Restart);
  CHECK(!m.open());

  in = {}; in.toggle = true; m.handle(in);
  in = {}; in.up = true; m.handle(in);  // wrap a Salir
  CHECK(m.cursor() == PauseMenu::kSalir);
  in = {}; in.confirm = true;
  CHECK(m.handle(in) == MenuAction::Quit);
  CHECK(m.open());  // Quit no cierra (el host decide apagar)

  // Volumen: derecha sube, izquierda baja; confirm mutea; mute volume()=0.
  PauseMenu v;
  in = {}; in.toggle = true; v.handle(in);
  in = {}; in.down = true; v.handle(in); v.handle(in);
  CHECK(v.cursor() == PauseMenu::kVolumen);
  const float mid = v.volume_setting();
  in = {}; in.right = true; v.handle(in);
  CHECK(std::fabs(v.volume_setting() - (mid + PauseMenu::kVolStep)) < 1e-5f);
  CHECK(!v.muted() && v.volume() == v.volume_setting());
  in = {}; in.confirm = true;
  CHECK(v.handle(in) == MenuAction::None);
  CHECK(v.muted() && v.volume() == 0.f);
  CHECK(v.volume_setting() > 0.f);  // el ajuste se conserva
  in = {}; in.confirm = true; v.handle(in);
  CHECK(!v.muted() && v.volume() == v.volume_setting());
  // Bajar hasta 0 mutea; subir desmutea.
  in = {};
  for (int i = 0; i < 20; ++i) { in.left = true; v.handle(in); }
  CHECK(v.volume_setting() == 0.f && v.muted() && v.volume() == 0.f);
  in = {}; in.right = true; v.handle(in);
  CHECK(!v.muted() && v.volume_setting() > 0.f);
  // Tope 1.
  for (int i = 0; i < 20; ++i) { in.right = true; v.handle(in); }
  CHECK(std::fabs(v.volume_setting() - 1.f) < 1e-5f);

  // Al reabrir, el cursor vuelve a Seguir (volumen se conserva).
  in = {}; in.toggle = true; v.handle(in);  // cierra
  const float kept = v.volume_setting();
  in = {}; in.toggle = true; v.handle(in);
  CHECK(v.open() && v.cursor() == PauseMenu::kSeguir);
  CHECK(std::fabs(v.volume_setting() - kept) < 1e-6f);

  std::puts("menu ok");
  return 0;
}
