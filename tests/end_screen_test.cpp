// Test sin GL: pantalla de final de nivel, glifos de puntuación, campaña y transición multi-mapa.
#include "game/campaign.hpp"
#include "game/end_screen.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"

#include <cstdio>
#include <memory>
#include <string>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)
using namespace ew;
using namespace ew::game;

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  const std::string slice_path = argv[1];
  const std::string ability_path = argv[2];

  // --- 1) Máquina de estados de EndScreen ---
  {
    EndScreen es;
    CHECK(!es.open());
    MenuInput in;
    in.confirm = in.down = in.up = true;
    CHECK(es.handle(in) == EndAction::None);

    // Con siguiente nivel disponible: arranca en kNext (0).
    EndStats st{"NIVEL 1: EL POZO", 1, 1, 2, kAbilityDoubleJump, true, ""};
    es.open(st);
    CHECK(es.open() && es.cursor() == EndScreen::kNext);
    in = {}; in.toggle = true;
    CHECK(es.handle(in) == EndAction::OpenMenu && es.open());

    in = {}; in.confirm = true;
    CHECK(es.handle(in) == EndAction::NextLevel && !es.open());

    // Navegación con wrap y acciones Restart, OpenMenu, Quit.
    es.open(st);
    in = {}; in.down = true;
    es.handle(in);
    CHECK(es.cursor() == EndScreen::kRestart);
    in = {}; in.confirm = true;
    CHECK(es.handle(in) == EndAction::RestartLevel && !es.open());

    es.open(st);
    in = {}; in.down = true; es.handle(in); es.handle(in);
    CHECK(es.cursor() == EndScreen::kMenu);
    in = {}; in.confirm = true;
    CHECK(es.handle(in) == EndAction::OpenMenu && es.open());

    in = {}; in.down = true; es.handle(in);
    CHECK(es.cursor() == EndScreen::kQuit);
    in = {}; in.confirm = true;
    CHECK(es.handle(in) == EndAction::Quit && es.open());

    in = {}; in.down = true; es.handle(in);  // wrap a kNext
    CHECK(es.cursor() == EndScreen::kNext);
    in = {}; in.up = true; es.handle(in);    // wrap hacia atrás a kQuit
    CHECK(es.cursor() == EndScreen::kQuit);

    // Sin siguiente nivel disponible: arranca en kRestart y kNext queda deshabilitado.
    st.has_next = false;
    es.open(st);
    CHECK(es.open() && es.cursor() == EndScreen::kRestart);
    in = {}; in.up = true; es.handle(in);
    CHECK(es.cursor() == EndScreen::kNext);
    in = {}; in.confirm = true;
    CHECK(es.handle(in) == EndAction::None && es.open());
    es.close();
    CHECK(!es.open());
  }

  // --- 2) Glifos de puntuación y empaquetado de texto de EndScreen ---
  {
    CHECK(kGlyphCount == 47);
    CHECK(glyph_index(':') == 43);
    CHECK(glyph_index('!') == 44);
    CHECK(glyph_index(',') == 45);
    CHECK(glyph_index('\'') == 46);

    EndScreen es;
    es.open({"NIVEL 1: EL POZO", 1, 1, 0, kAbilityDoubleJump, true, ""});
    const auto txt = end_screen_text(es);
    CHECK(txt[0] == "NIVEL COMPLETADO");
    CHECK(txt[1] == "NIVEL 1: EL POZO");
    CHECK(txt[2] == "HUEVOS:  1/1");
    CHECK(txt[3] == "MUERTES: 0");
    CHECK(txt[4] == "HABILIDADES: DOBLE SALTO");
    CHECK(txt[6] == "> SIGUIENTE NIVEL");
    CHECK(txt[7] == "  REINICIAR NIVEL");
    CHECK(txt[8] == "  MENU DE PAUSA");
    CHECK(txt[9] == "  SALIR");
    const auto packed = pack_text(txt);
    CHECK((packed[0] & 63) == glyph_index('N'));

    // Fin de campaña (!has_next), habilidad FAROL y formato compacto con 4 habilidades + DASH II + +1HP.
    EndScreen es_rev;
    es_rev.open({"FIN", 0, 1, 0, kAbilityReveal, false, ""});
    const auto t_rev = end_screen_text(es_rev);
    CHECK(t_rev[4] == "HABILIDADES: FAROL");
    CHECK(t_rev[6] == "  FIN DE LA AVENTURA");

    EndScreen es_all;
    EndStats st_all{"EL CALABOZO DEL WYRM", 8, 8, 0, kAbilityAll, false, "", 120.f, true, true, 3, true};
    es_all.open(st_all);
    const auto t_all = end_screen_text(es_all);
    CHECK(t_all[4] == "HAB: DJ/DASH II/ESC/FAROL/+2HP");
    CHECK(t_all[4].size() <= std::size_t(kTxtCols));
    CHECK(t_all[6] == "  FIN DE LA AVENTURA");
  }

  // --- 3) Campaña y resolución segura de archivos de mapa ---
  {
    CHECK(next_campaign_map("slice") == "level2");
    CHECK(next_campaign_map("level2") == "level3");
    CHECK(next_campaign_map("level3") == "");
    CHECK(next_campaign_map("otro") == "");
    CHECK(level_title("slice") == "NIVEL 1: EL POZO");
    CHECK(level_title("level2") == "NIVEL 2: LAS CATACUMBAS");
    CHECK(level_title("level3") == "NIVEL 3: LA GUARIDA DEL WYRM");
    CHECK(level_title("mi_sala") == "MI SALA");

    CHECK(valid_map_name("slice") && valid_map_name("level_2-a"));
    CHECK(!valid_map_name("") && !valid_map_name("../slice") && !valid_map_name("a/b") && !valid_map_name("a\\b"));
    CHECK(!valid_map_name(std::string(65, 'a')));

    // Resuelve ability.ewmap desde la carpeta hermana de slice.ewmap.
    const std::string resolved = resolve_map_file("ability", slice_path, "");
    CHECK(!resolved.empty() && ew::core::file_exists(resolved));
    CHECK(resolve_map_file("no_existe_mapa", slice_path, "").empty());
    CHECK(resolve_map_file("../ability", slice_path, "").empty());
  }

  // --- 4) Transición headless entre niveles + CONTINUAR multi-mapa ---
  {
    std::string err;
    auto scene = std::make_unique<Scene>();
    CHECK(scene->load(slice_path, err));
    auto sim = std::make_unique<Sim>(*scene);
    sim->grant_ability(kAbilityDoubleJump);
    const auto* g = scene->find_type("goal");
    CHECK(g);
    // Vencer al boss de slice para habilitar la meta y tocar goal.
    for (std::size_t i = 0; i < scene->entities().size(); ++i)
      if (scene->entities()[i].type == "boss") scene->set_linked(i, true);
    sim->load_state(-1, false, [&] {
      std::vector<uint8_t> a(scene->entities().size(), 0);
      for (std::size_t i = 0; i < scene->entities().size(); ++i)
        if (scene->entities()[i].type == "boss") a[i] = 1;
      return a;
    }(), kAbilityDoubleJump);
    auto& pl = sim->world().points()[sim->player_index()];
    pl.pos = pl.prev = {g->x, g->y};
    sim->step({});
    CHECK(sim->finished());

    const std::string next_p = resolve_map_file("ability", slice_path, "");
    CHECK(!next_p.empty());
    EndScreen es;
    es.open({level_title(scene->map().string_at(1)), sim->collected(), sim->collectible_total(),
             sim->deaths(), sim->abilities(), true, ""});
    MenuInput in; in.confirm = true;
    CHECK(es.handle(in) == EndAction::NextLevel);

    // Transición atómica al nivel siguiente transfiriendo habilidades.
    const uint32_t carry = sim->abilities();
    auto next_scene = std::make_unique<Scene>();
    CHECK(next_scene->load(next_p, err));
    auto next_sim = std::make_unique<Sim>(*next_scene);
    next_sim->set_abilities(carry);
    scene = std::move(next_scene);
    sim = std::move(next_sim);
    CHECK(scene->map().string_at(1) == "ability");
    CHECK(scene->chunk_version() >= 1);
    CHECK(!sim->finished());
    CHECK(sim->has_ability(kAbilityDoubleJump) && sim->air_jumps() == 1);

    // Guardar en el segundo mapa y simular CONTINUAR desde un arranque en slice.
    const auto saved_bytes = encode_save(capture_save(*sim));
    SaveData loaded;
    CHECK(decode_save(saved_bytes.data(), saved_bytes.size(), loaded) == SaveError::None);
    CHECK(loaded.map == "ability" && loaded.abilities == kAbilityDoubleJump);

    auto boot_scene = std::make_unique<Scene>();
    CHECK(boot_scene->load(slice_path, err));
    CHECK(loaded.map != boot_scene->map().string_at(1));
    const std::string target = resolve_map_file(loaded.map, slice_path, "");
    CHECK(!target.empty());
    auto cont_scene = std::make_unique<Scene>();
    CHECK(cont_scene->load(target, err));
    auto cont_sim = std::make_unique<Sim>(*cont_scene);
    CHECK(apply_save(*cont_sim, loaded) == SaveError::None);
    CHECK(cont_scene->map().string_at(1) == "ability");
    CHECK(cont_sim->has_ability(kAbilityDoubleJump));
  }

  std::puts("end_screen ok");
  return 0;
}
