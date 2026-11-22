// Menú de pausa headless: abrir/cerrar, cursor, volumen/mute, héroe, acciones y texto.
#include "game/menu.hpp"
#include "game/menu_text.hpp"
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
  in = {}; in.down = true; m.handle(in); m.handle(in);
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
  in = {}; in.down = true; v.handle(in); v.handle(in); v.handle(in);
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

  // Continuar: sin save no hace nada (queda abierto); con save cierra y devuelve Continue.
  PauseMenu c;
  CHECK(!c.has_save());
  in = {}; in.toggle = true; c.handle(in);
  in = {}; in.down = true; c.handle(in);
  CHECK(c.cursor() == PauseMenu::kContinuar);
  in = {}; in.confirm = true;
  CHECK(c.handle(in) == MenuAction::None && c.open());
  c.set_has_save(true);
  CHECK(c.handle(in) == MenuAction::Continue && !c.open());
  // Arranque con save: abre directo en Continuar.
  c.open_at(PauseMenu::kContinuar);
  CHECK(c.open() && c.cursor() == PauseMenu::kContinuar);
  c.open_at(99);
  CHECK(c.cursor() == PauseMenu::kSeguir);
  // set_audio clampa y 0 = mute.
  c.set_audio(0.3f, false);
  CHECK(std::fabs(c.volume_setting() - 0.3f) < 1e-6f && !c.muted());
  c.set_audio(7.f, true);
  CHECK(c.volume_setting() == 1.f && c.muted() && c.volume() == 0.f);
  c.set_audio(-1.f, false);
  CHECK(c.volume_setting() == 0.f && c.muted());

  // Brisa: arranca en true, confirm/left/right alternan sin cerrar el menú, y kSalir sigue al final con wrap.
  PauseMenu b;
  CHECK(b.foliage());
  CHECK(PauseMenu::kSalir == PauseMenu::kItemCount - 1);
  in = {}; in.toggle = true; b.handle(in);
  CHECK(b.open() && b.cursor() == PauseMenu::kSeguir);
  in = {}; in.up = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kSalir);
  in = {}; in.down = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kSeguir);
  for (int i = 0; i < PauseMenu::kBrisa; ++i) { in = {}; in.down = true; b.handle(in); }
  CHECK(b.cursor() == PauseMenu::kBrisa);
  in = {}; in.down = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kImpact);
  in = {}; in.down = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kHero);
  in = {}; in.down = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kSalir);
  in = {}; in.up = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kHero);
  in = {}; in.up = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kImpact);
  in = {}; in.up = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kBrisa);

  in = {}; in.confirm = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && !b.foliage());
  in = {}; in.left = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && b.foliage());
  in = {}; in.right = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && !b.foliage());
  in = {}; in.confirm = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && b.foliage());

  // Impacto: arranca en true, confirm/left/right alternan sin cerrar el menú.
  CHECK(b.impact_fx());
  in = {}; in.down = true; b.handle(in);
  CHECK(b.cursor() == PauseMenu::kImpact);
  in = {}; in.confirm = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && !b.impact_fx());
  in = {}; in.left = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && b.impact_fx());
  in = {}; in.right = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && !b.impact_fx());
  in = {}; in.confirm = true;
  CHECK(b.handle(in) == MenuAction::None);
  CHECK(b.open() && b.impact_fx());

  // Héroes: helpers + ciclado en kHero + subpágina de selector + start_only.
  {
    HeroKind h = HeroKind::Lantern;
    CHECK(hero_sprite_col_offset(HeroKind::Lantern) == 0);
    CHECK(hero_sprite_col_offset(HeroKind::Opossum) == 4);
    CHECK(hero_sprite_col_offset(HeroKind::Beaver) == 8);
    CHECK(sanitize_hero(99) == HeroKind::Lantern);
    CHECK(cycle_hero(HeroKind::Lantern, -1) == HeroKind::Beaver);
    CHECK(cycle_hero(HeroKind::Beaver, +1) == HeroKind::Lantern);
    CHECK(hero_from_arg("ZARIGUEYA", h) && h == HeroKind::Opossum);
    CHECK(hero_from_arg("Beaver", h) && h == HeroKind::Beaver);
    CHECK(hero_from_arg("sr_farol", h) && h == HeroKind::Lantern);
    CHECK(!hero_from_arg("dragon", h));

    PauseMenu hm;
    CHECK(hm.hero() == HeroKind::Lantern && !hm.in_hero_select());
    hm.open_at(PauseMenu::kHero);
    in = {}; in.right = true; CHECK(hm.handle(in) == MenuAction::None);
    CHECK(hm.hero() == HeroKind::Opossum);
    in = {}; in.left = true; CHECK(hm.handle(in) == MenuAction::None);
    CHECK(hm.hero() == HeroKind::Lantern);

    // Confirmar en kHero abre page_ == 2; ESC vuelve a page_ == 0.
    in = {}; in.confirm = true; CHECK(hm.handle(in) == MenuAction::None);
    CHECK(hm.in_hero_select() && hm.hero_cursor() == 0);
    in = {}; in.down = true; hm.handle(in);
    CHECK(hm.hero_cursor() == 1);
    in = {}; in.toggle = true; CHECK(hm.handle(in) == MenuAction::None);
    CHECK(hm.open() && !hm.in_hero_select() && hm.hero() == HeroKind::Lantern);

    // Confirmar dentro de page_ == 2 aplica héroe y cierra menú (Resume).
    in = {}; in.confirm = true; hm.handle(in);
    in = {}; in.up = true; hm.handle(in);
    CHECK(hm.hero_cursor() == 2);
    const auto htxt = hero_select_text(hm);
    CHECK(htxt[0] == "ELIGE TU HEROE");
    CHECK(htxt[5] == "> EL CASTOR");
    CHECK(htxt[11] == "ARRIBA/ABAJO  ENTER JUEGA");
    in = {}; in.confirm = true;
    CHECK(hm.handle(in) == MenuAction::Resume);
    CHECK(!hm.open() && !hm.in_hero_select() && hm.hero() == HeroKind::Beaver);

    // En modo inicio directo, ESC cierra directo al juego manteniendo el héroe actual.
    hm.open_hero_select(true);
    CHECK(hm.in_hero_select() && hm.hero_cursor() == 2);
    in = {}; in.toggle = true;
    CHECK(hm.handle(in) == MenuAction::Resume);
    CHECK(!hm.open() && hm.hero() == HeroKind::Beaver);

    // pause_text incluye los 9 ítems, héroe corto, brisa, impacto y versión.
    hm.open_at(PauseMenu::kHero);
    auto ptxt = pause_text(hm, "0.1.0");
    CHECK(ptxt[0] == "PAUSA");
    CHECK(ptxt[6] == "  BRISA: SI");
    CHECK(ptxt[7] == "  IMPACTO: SI");
    CHECK(ptxt[8] == "> HEROE: CASTOR");
    CHECK(ptxt[9] == "  SALIR");
    CHECK(ptxt[11] == "V0.1.0");
    hm.set_impact_fx(false);
    ptxt = pause_text(hm, "0.1.0");
    CHECK(ptxt[7] == "  IMPACTO: NO");
  }

  std::puts("menu ok");
  return 0;
}
