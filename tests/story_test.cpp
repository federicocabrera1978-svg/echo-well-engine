// Test sin GL (Etapa 16 / Prioridad 5 Opción A): consistencia de historia y tono
// "calabozos y dragones" en todos los mapas, diálogos de bosses, títulos y pantalla final.
#include "game/boss_victory.hpp"
#include "game/campaign.hpp"
#include "game/end_screen.hpp"
#include "game/scene.hpp"
#include "game/story.hpp"

#include <cstdio>
#include <string>
#include <vector>

#define CHECK(c)                                                                \
  do {                                                                          \
    if (!(c)) {                                                                 \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
      return 1;                                                                 \
    }                                                                           \
  } while (0)

using namespace ew::game;

static bool contains_any(std::string_view text, const std::vector<std::string_view>& words) {
  for (std::string_view w : words)
    if (text.find(w) != std::string_view::npos) return true;
  return false;
}

int main(int argc, char** argv) {
  if (argc < 5) {
    std::fprintf(stderr, "uso: story_test <slice> <level2> <level3> <world>\n");
    return 2;
  }

  struct MapSpec {
    const char* name;
    const char* path;
    std::size_t expected_dialogs;
  };
  const MapSpec maps[] = {
      {"slice", argv[1], 2},
      {"level2", argv[2], 6},
      {"level3", argv[3], 4},
      {"world", argv[4], 19},
  };

  for (const auto& m : maps) {
    Scene sc;
    std::string err;
    CHECK(sc.load(m.path, err));
    CHECK(sc.dialogs().size() == m.expected_dialogs);
    bool has_dragon_lore = false;
    for (const auto& d : sc.dialogs()) {
      std::string why;
      if (!story_speaker_valid(d.speaker, &why)) {
        std::fprintf(stderr, "Hablante invalido en %s: %s (%s)\n", m.name, d.speaker.c_str(), why.c_str());
        return 1;
      }
      if (!story_text_valid(d.text, &why)) {
        std::fprintf(stderr, "Texto invalido en %s (%s): %s (%s)\n", m.name, d.speaker.c_str(), d.text.c_str(), why.c_str());
        return 1;
      }
      if (contains_any(d.speaker, {"DRAGON", "WYRM", "HUEVO"}) ||
          contains_any(d.text, {"DRAGON", "WYRM", "NIDADA", "HUEVO", "SIERPE"})) {
        has_dragon_lore = true;
      }
    }
    CHECK(has_dragon_lore);
  }

  // Diálogos de victoria de los 3 bosses
  const struct {
    BossKind kind;
    std::string_view speaker;
  } bosses[] = {
      {BossKind::Watcher, kVictoryWatcherSpeaker},
      {BossKind::Guardian2, kVictoryGuardian2Speaker},
      {BossKind::Wyrm, kVictoryWyrmSpeaker},
  };
  for (const auto& b : bosses) {
    const BossVictoryDialog dlg = boss_victory_dialog(b.kind);
    CHECK(dlg.speaker == b.speaker);
    std::string why;
    CHECK(story_speaker_valid(dlg.speaker, &why));
    CHECK(story_text_valid(dlg.text, &why));
  }

  // Títulos y pantalla final (Opción A)
  for (std::string_view sv : {kEndTitleVictory, kEndTitleTrue, kEndSubtitleTrue, kWorldTitle}) {
    CHECK(story_speaker_valid(sv));
    CHECK(sv.size() <= std::size_t(kTxtCols));
  }
  CHECK(level_title("world") == kWorldTitle);
  for (const char* lvl_name : {"world", "slice", "level2", "level3"}) {
    const std::string t = level_title(lvl_name);
    CHECK(!t.empty());
    CHECK(story_find_forbidden(t) == nullptr);
  }

  // Casos negativos del validador de tono y formato
  CHECK(story_text_valid("EL DRAGON GUARDA SU NIDADA.") == true);
  CHECK(story_text_valid("EL SELLO DE LA PUREZA") == false);
  CHECK(story_text_valid("CAMINA CON CORAZON PURO") == false);
  CHECK(story_text_valid("LA ASCENSION DEL POZO") == false);
  CHECK(story_text_valid("SIN IMPEDIMENTO EN EL PASO") == false);
  CHECK(story_speaker_valid("SELLO DE LA ASCENSION") == false);
  CHECK(story_text_valid("LINEA QUE EXCEDE LOS TREINTA GLIFOS PERMITIDOS") == false);
  CHECK(story_text_valid("UNA LINEA/DOS LINEAS/TRES LINEAS") == false);
  CHECK(story_text_valid("P1|P2|P3|P4") == false);
  CHECK(story_text_valid("SIMBOLO [PROHIBIDO]") == false);

  std::printf("story_tone OK (maps=%zu, bosses=3)\n", sizeof(maps) / sizeof(maps[0]));
  return 0;
}
