// Test sin GL: diálogos .ewmap (DIALOG = 10), parser YAML con comillas, DialogueBox
// y narrativa de los huevos de dragón en slice y level2 (Opción C: NPC/estelas + ecos).
#include "game/dialogue.hpp"
#include "game/sim.hpp"
#include "writer.hpp"
#include "yaml_lite.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

using namespace ew;
using namespace ew::game;

static void place(Sim& s, float x, float y) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = p.prev = {x, y};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

static const Entity* find_name(const Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

int main(int argc, char** argv) {
  if (argc < 4) return 2;

  // --- 1) yaml_lite con comillas (# , : adentro) + validación de dialogs en ewmapc ---
  {
    const std::string src = R"(name: dlg_test
size_chunks: [1, 1]
entities:
  - {name: spawn, type: player_spawn, x: 32, y: 152}
  - {name: sabio, type: npc, x: 72, y: 152}
  - {name: aviso, type: lore, x: 120, y: 152}
dialogs:
  - {entity: sabio, trigger: interact, speaker: "VIGIA #1: EL POZO", text: "HUEVOS, PIEDRAS Y FUEGO:/GUARDA EL SECRETO.|DOS LINEAS/SEGUNDA PAGINA."} # comentario externo
  - {entity: aviso, trigger: proximity, speaker: 'runa antigua', text: 'Paso cerrado: usa alas'}
)";
    ewmapc::YamlDoc doc;
    std::string err;
    CHECK(ewmapc::parse_yaml_lite(src, doc, err));
    CHECK(doc.lists["dialogs"].size() == 2);
    CHECK(doc.lists["dialogs"][0]["speaker"] == "VIGIA #1: EL POZO");
    CHECK(doc.lists["dialogs"][0]["text"] == "HUEVOS, PIEDRAS Y FUEGO:/GUARDA EL SECRETO.|DOS LINEAS/SEGUNDA PAGINA.");
    CHECK(doc.lists["dialogs"][1]["speaker"] == "runa antigua");

    std::vector<std::byte> bin;
    CHECK(ewmapc::build_ewmap(doc, {true}, bin, err));
    map::MapLoader m;
    CHECK(m.load_bytes(std::move(bin)) == map::MapError::None);
    CHECK(m.dialogs().size() == 2);
    CHECK(m.string_at(m.dialogs()[0].speaker_str) == "VIGIA #1: EL POZO");
    CHECK(m.dialogs()[0].trigger == uint16_t(map::DialogTrigger::Interact));
    CHECK((m.dialogs()[0].flags & map::kDialogFlagOnce) == 0);
    CHECK(m.dialogs()[1].trigger == uint16_t(map::DialogTrigger::Proximity));
    CHECK((m.dialogs()[1].flags & map::kDialogFlagOnce) != 0);

    // Comilla sin cerrar en flow map → rechazo limpio
    ewmapc::YamlDoc bad_doc;
    CHECK(!ewmapc::parse_yaml_lite("dialogs:\n  - {entity: sabio, text: \"sin cerrar}\n", bad_doc, err));

    // Rechazos en writer: entidad inexistente, trigger desconocido, speaker vacío
    auto check_reject = [&](const char* yaml) {
      ewmapc::YamlDoc d2;
      std::string e2;
      if (!ewmapc::parse_yaml_lite(yaml, d2, e2)) return false;
      std::vector<std::byte> b2;
      return !ewmapc::build_ewmap(d2, {false}, b2, e2);
    };
    CHECK(check_reject("name: x\nentities:\n  - {name: sp, type: player_spawn, x: 32, y: 152}\ndialogs:\n  - {entity: fantasma, speaker: A, text: B}\n"));
    CHECK(check_reject("name: x\nentities:\n  - {name: sp, type: player_spawn, x: 32, y: 152}\ndialogs:\n  - {entity: sp, trigger: boom, speaker: A, text: B}\n"));
    CHECK(check_reject("name: x\nentities:\n  - {name: sp, type: player_spawn, x: 32, y: 152}\ndialogs:\n  - {entity: sp, trigger: interact, speaker: \"\", text: B}\n"));
  }

  // --- 2) Máquina de estados headless DialogueBox y empaquetado u_txt ---
  {
    DialogueBox box;
    CHECK(!box.open() && !box.has_more());
    CHECK(!box.advance());

    box.open("cronista", "Linea uno de prueba/Linea dos|Segunda pagina/Final del eco");
    CHECK(box.open() && box.page_count() == 2 && box.page() == 0 && box.has_more());
    CHECK(box.speaker() == "CRONISTA");
    CHECK(box.current_page().line0 == "LINEA UNO DE PRUEBA");
    CHECK(box.current_page().line1 == "LINEA DOS");
    auto txt = dialogue_text(box);
    CHECK(txt[0] == "CRONISTA");
    CHECK(txt[3].find("PAG 1/2") != std::string::npos);
    const auto packed = pack_text(txt);
    CHECK((packed[0] & 63) == glyph_index('C'));

    CHECK(box.advance());
    CHECK(box.open() && box.page() == 1 && !box.has_more());
    CHECK(box.current_page().line0 == "SEGUNDA PAGINA");
    CHECK(box.current_page().line1 == "FINAL DEL ECO");
    CHECK(!box.advance());
    CHECK(!box.open());

    box.open("A", "uno|dos");
    CHECK(box.open());
    box.close();
    CHECK(!box.open());
  }

  // --- 3) Mapa slice: Vigía del Farol (NPC) + Eco dracónico del huevo del pozo ---
  {
    Scene s;
    std::string err;
    CHECK(s.load(argv[1], err));
    CHECK(s.dialogs().size() == 2);
    Sim sim(s);

    const auto* vigia = find_name(s, "npc_vigia");
    const auto* egg = find_name(s, "egg_well");
    CHECK(vigia && egg && vigia->type == "npc");

    // Caminar hasta el Vigía del Farol y hablar con E (once == false → repetible).
    place(sim, vigia->x, vigia->y);
    sim.step({});
    CHECK(sim.nearby_interact_dialog() >= 0);
    { Input act; act.interact = true; sim.step(act); }
    const int d0 = sim.take_dialog();
    CHECK(d0 >= 0 && s.dialogs()[std::size_t(d0)].speaker == "VIGIA DEL FAROL");
    CHECK(vigia->active);

    // Se puede volver a conversar con él tras cerrar.
    { Input act; act.interact = true; sim.step(act); }
    CHECK(sim.take_dialog() == d0);

    // Agarrar el huevo del pozo dispara el eco automático una sola vez.
    place(sim, egg->x, egg->y);
    sim.step({});
    CHECK(egg->active && sim.collected() == 1);
    const int d1 = sim.take_dialog();
    CHECK(d1 >= 0 && s.dialogs()[std::size_t(d1)].speaker.find("HUEVO") != std::string::npos);
    CHECK(sim.take_dialog() == -1);
    sim.step({});
    CHECK(sim.take_dialog() == -1);

    // R corto conserva el huevo recogido; R largo limpia diálogos triggered.
    { Input rst; rst.restart = true; sim.step(rst); }
    CHECK(egg->active);
    { Input full; full.restart_full = true; sim.step(full); }
    CHECK(!egg->active && !s.dialogs()[std::size_t(d1)].triggered);
  }

  // --- 4) Nivel 2 (Las Catacumbas): Cronista, Estela, Alas y ambos huevos ---
  {
    Scene s2;
    std::string err;
    CHECK(s2.load(argv[2], err));
    CHECK(s2.dialogs().size() == 5);
    Sim sim2(s2);

    const auto* cronista = find_name(s2, "npc_cronista");
    const auto* estela = find_name(s2, "lore_santuario");
    const auto* rel = find_name(s2, "rel_double_jump");
    const auto* egg1 = find_name(s2, "egg_catacomb_1");
    const auto* egg2 = find_name(s2, "egg_catacomb_2");
    CHECK(cronista && estela && rel && egg1 && egg2);
    CHECK(estela->type == "lore");

    // NPC cronista (interact)
    place(sim2, cronista->x, cronista->y);
    sim2.step({});
    { Input act; act.interact = true; sim2.step(act); }
    const int dc = sim2.take_dialog();
    CHECK(dc >= 0 && s2.dialogs()[std::size_t(dc)].speaker == "CRONISTA DE LAS CATACUMBAS");

    // Reliquia alada (pickup)
    place(sim2, rel->x, rel->y);
    sim2.step({});
    CHECK(sim2.has_ability(kAbilityDoubleJump));
    const int dr = sim2.take_dialog();
    CHECK(dr >= 0 && s2.dialogs()[std::size_t(dr)].speaker.find("ALAS") != std::string::npos);

    // Estela del santuario (interact)
    place(sim2, estela->x, estela->y);
    sim2.step({});
    { Input act; act.interact = true; sim2.step(act); }
    const int dl = sim2.take_dialog();
    CHECK(dl >= 0 && s2.dialogs()[std::size_t(dl)].speaker == "ESTELA DEL SANTUARIO");

    // Ecos de ambos huevos (pickup)
    place(sim2, egg2->x, egg2->y);
    sim2.step({});
    const int de2 = sim2.take_dialog();
    CHECK(de2 >= 0 && s2.dialogs()[std::size_t(de2)].speaker == "ECO: HUEVO DE BRASA");

    place(sim2, egg1->x, egg1->y);
    sim2.step({});
    const int de1 = sim2.take_dialog();
    CHECK(de1 >= 0 && s2.dialogs()[std::size_t(de1)].speaker == "ECO: HUEVO DEL NIDO");
    CHECK(sim2.collected() == 2);
  }

  // --- 5) Retrocompatibilidad: hello.ewmap sin sección DIALOG ---
  {
    Scene sh;
    std::string err;
    CHECK(sh.load(argv[3], err));
    CHECK(sh.dialogs().empty());
    Sim simh(sh);
    CHECK(simh.nearby_interact_dialog() == -1 && simh.take_dialog() == -1);
  }

  std::puts("dialogue_test: OK");
  return 0;
}
