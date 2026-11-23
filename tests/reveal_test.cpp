// Test sin GL (Etapa 3): Farol Revelador (`ability_reveal`, bit 3) + bloque espectral `reveal_block`.
// Verifica que el bloque es fantasma (no sólido) sin la reliquia, se revela de forma permanente
// al acercarse con la habilidad, sostiene al player, persiste en save.ewsv y se limpia con R largo.
#include "audio/sfx.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"
#include "game/story.hpp"

#include <cstdio>
#include <string>

#define CHECK(c)                                                                \
  do {                                                                          \
    if (!(c)) {                                                                 \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
      return 1;                                                                 \
    }                                                                           \
  } while (0)

using namespace ew;
using game::Input;
using game::Scene;
using game::Sim;

static void place(Sim& s, float x, float y) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = p.prev = {x, y};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

static const game::Entity* find_name(const Scene& s, const char* n) {
  for (const auto& e : s.entities())
    if (e.name == n) return &e;
  return nullptr;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: reveal_test <reveal.ewmap>\n");
    return 2;
  }

  Scene scene;
  std::string err;
  CHECK(scene.load(argv[1], err));

  const auto* rel = find_name(scene, "rel_reveal");
  const auto* rb = find_name(scene, "rb_bridge");
  const auto* goal = find_name(scene, "goal");
  CHECK(rel && rel->type == "ability_reveal");
  CHECK(rb && rb->type == "reveal_block");
  CHECK(goal && goal->type == "goal");

  // El diálogo de la reliquia respeta el tono y la fuente bitmap.
  CHECK(scene.dialogs().size() == 1);
  for (const auto& d : scene.dialogs()) {
    std::string why;
    CHECK(game::story_speaker_valid(d.speaker, &why));
    CHECK(game::story_text_valid(d.text, &why));
  }

  Sim sim(scene);
  CHECK(!sim.has_ability(game::kAbilityReveal));

  // 1) Sin la reliquia el bloque es fantasma: no bloquea el hueco.
  CHECK(!scene.solid_tile(16, 20) && !scene.door_blocks(16, 20));
  CHECK(!rb->active);

  // 2) Sin la reliquia el player cae al foso con pinchos y muere.
  place(sim, 140.f, 140.f);
  for (int i = 0; i < 60 && sim.deaths() == 0; ++i) sim.step({});
  CHECK(sim.deaths() > 0);

  // 3) Recoger la reliquia desbloquea ability_reveal.
  sim.reset();
  place(sim, rel->x, rel->y);
  sim.step({});
  CHECK(sim.has_ability(game::kAbilityReveal) && rel->active);

  // 4) Al acercarse al hueco, el bloque se revela de forma permanente y queda sólido.
  place(sim, rb->x - 40.f, rb->y - 8.f);
  for (int i = 0; i < 10; ++i) sim.step({});
  CHECK(rb->active);
  CHECK(scene.door_blocks(16, 20) && scene.solid_tile(16, 20));

  // 5) El bloque revelado sostiene al player (queda grounded, sin caer).
  place(sim, 140.f, 156.f);
  for (int i = 0; i < 20; ++i) sim.step({});
  CHECK(sim.player().grounded);
  CHECK(sim.deaths() == 0);
  CHECK(std::fabs(sim.player().pos.y - 156.f) < 4.f);

  // 6) Persiste en save.ewsv (bit 3) y se restaura en una escena nueva.
  const auto bytes = game::encode_save(game::capture_save(sim));
  game::SaveData decoded;
  CHECK(game::decode_save(bytes.data(), bytes.size(), decoded) == game::SaveError::None);
  CHECK((decoded.abilities & game::kAbilityReveal) != 0);

  Scene s2;
  CHECK(s2.load(argv[1], err));
  Sim sim2(s2);
  CHECK(game::apply_save(sim2, decoded) == game::SaveError::None);
  CHECK(sim2.has_ability(game::kAbilityReveal));
  CHECK(find_name(s2, "rb_bridge")->active && s2.door_blocks(16, 20));

  // 7) R largo limpia la reliquia y el bloque vuelve a ser fantasma.
  sim2.reset();
  CHECK(!sim2.has_ability(game::kAbilityReveal));
  CHECK(!find_name(s2, "rb_bridge")->active && !s2.door_blocks(16, 20));

  // 8) Con la habilidad se puede cruzar el puente revelado hasta la meta sin morir.
  {
    Sim sim3(scene);
    sim3.grant_ability(game::kAbilityReveal);
    place(sim3, rel->x, rel->y);
    sim3.step({});
    bool crossed = false;
    for (int i = 0; i < 220 && !sim3.finished(); ++i) {
      Input in;
      in.move_x = 1.f;
      sim3.step(in);
      if (sim3.player().pos.x > 170.f) crossed = true;
    }
    CHECK(crossed && sim3.deaths() == 0);
    CHECK(sim3.finished());
  }

  std::puts("reveal_test OK");
  return 0;
}
