// Test sin GL: Boss intermedio Centinela del Archivo (boss_sentinel).
// Arena cerrada de 1 chunk, patrones deterministas (patrulla, aviso, salto con abanico triple
// de proyectiles y embestida rasa), pisotón x 3, contacto lateral letal, apertura de puerta linkeada,
// secuencia de rayos catódicos y diálogo de victoria (SELLO DEL ARCHIVO) en story.hpp.
#include "audio/sfx.hpp"
#include "game/boss_victory.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"
#include "game/story.hpp"

#include <cmath>
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

using namespace ew;
using game::BossKind;
using game::Input;
using game::Scene;
using game::Sim;

static void place(Sim& s, float x, float y, float vy = 0.f) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = {x, y};
  p.prev = {x, y - vy};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

static const game::Entity* find_name(const Scene& s, const char* name) {
  for (const auto& e : s.entities())
    if (e.name == name) return &e;
  return nullptr;
}

// Deja caer al jugador sobre el centro del boss desde arriba para asestar un pisotón.
static void stomp_boss(Sim& s, int boss_idx = 0) {
  const auto& b = s.boss(boss_idx);
  const float top = b.y - b.height();
  place(s, b.x, top - Sim::kPlayerRadius + 1.5f, 2.f);
  s.step({});
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: sentinel_boss_test <sentinel.ewmap>\n");
    return 2;
  }

  Scene scene;
  std::string err;
  CHECK(scene.load(argv[1], err));

  // --- 1) Invariantes de mapa y entidades ---
  CHECK(scene.map().chunk_count() == 1);
  CHECK(scene.map().backdrops().size() == 1);
  CHECK(scene.backdrop(0, 0).theme == uint16_t(ew::map::BackdropTheme::Aqueduct));

  const auto* boss_ent = find_name(scene, "boss_sentinel");
  const auto* door = find_name(scene, "door_exit");
  const auto* ck = find_name(scene, "ck_sentinel");
  const auto* goal = find_name(scene, "goal");
  CHECK(boss_ent && door && ck && goal);
  CHECK(boss_ent->type == "boss_sentinel");
  CHECK(!boss_ent->active && !door->active);

  // Puerta de salida cerrada de inicio (tile 34, ty 17..19)
  CHECK(scene.door_blocks(34, 17) && scene.door_blocks(34, 19));

  Sim sim(scene);
  CHECK(sim.boss_count() == 1 && !sim.bosses_defeated());
  const auto& b0 = sim.boss(0);
  CHECK(b0.sentinel && !b0.dragon && !b0.guardian2);
  CHECK(b0.width() == Sim::kSentinelW);
  CHECK(b0.height() == Sim::kSentinelH);
  CHECK(b0.max_hits() == Sim::kSentinelHits);
  CHECK(b0.hits == 0 && !b0.defeated);
  CHECK(b0.home_x == 176.f && b0.home_y == 160.f);

  // --- 2) Determinismo y ciclo de patrones (patrulla, aviso y abanico de proyectiles) ---
  {
    Scene s2;
    CHECK(s2.load(argv[1], err));
    Sim a(scene), b(s2);
    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f;
    bool saw_tell = false, saw_hop = false, saw_fireballs = false;

    for (int i = 0; i < 600; ++i) {
      a.step({});
      b.step({});
      CHECK(a.boss(0).x == b.boss(0).x && a.boss(0).y == b.boss(0).y && a.boss(0).phase == b.boss(0).phase);
      min_x = std::fmin(min_x, a.boss(0).x);
      max_x = std::fmax(max_x, a.boss(0).x);
      min_y = std::fmin(min_y, a.boss(0).y);
      if (a.boss(0).phase == 1) saw_tell = true;
      if (a.boss(0).phase == 2) saw_hop = true;
      if (a.fireballs().size() == 3) saw_fireballs = true;
    }
    CHECK(saw_tell && saw_hop && saw_fireballs);
    CHECK(min_x >= 176.f - Sim::kSentinelRange - 0.1f);
    CHECK(max_x <= 176.f + Sim::kSentinelRange + 0.1f);
    CHECK(min_y < 160.f - 20.f);  // Salto efectivo
    CHECK(a.deaths() == 0);
  }

  // --- 3) Contacto lateral letal y reaparición en checkpoint ---
  {
    sim.reset();
    place(sim, ck->x, ck->y);
    sim.step({});
    CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_sentinel");

    // Acercarse de lado al boss mientras está vulnerable causa daño
    for (int i = 0; i < 30; ++i) sim.step({});
    const auto& b = sim.boss(0);
    place(sim, b.x - Sim::kSentinelW * 0.5f - 2.f, b.y - 4.f);
    sim.step({});
    CHECK(sim.deaths() == 1);
    CHECK(std::fabs(sim.player().pos.x - ck->x) < 2.f);
  }

  // --- 4) Dash con i-frames permite esquivar proyectiles ---
  {
    sim.reset();
    sim.grant_ability(game::kAbilityDash);
    // Esperar a que el boss lance proyectiles (fase 2)
    for (int i = 0; i < 200 && sim.fireballs().size() < 3; ++i) {
      place(sim, ck->x, ck->y);
      sim.step({});
    }
    CHECK(sim.fireballs().size() >= 3);
    // Dejar que el proyectil viaje alejándose del cuerpo del boss
    for (int i = 0; i < 15; ++i) {
      place(sim, ck->x, ck->y);
      sim.step({});
    }
    CHECK(!sim.fireballs().empty());
    const auto fb = sim.fireballs()[0];
    // Colocar jugador fuera de la caja del boss y hacer Dash a través del proyectil
    place(sim, fb.x + 4.f, fb.y);
    Input d;
    d.move_x = -1.f;
    d.dash = true;
    sim.step(d);
    CHECK(sim.deaths() == 0);
  }

  // --- 5) Combate completo: 3 golpes, embestida en fase 3, victoria, diálogo y meta ---
  {
    sim.reset();
    place(sim, ck->x, ck->y);
    sim.step({});

    // Golpe 1
    stomp_boss(sim);
    CHECK(sim.boss(0).hits == 1);
    CHECK(sim.boss(0).hurt > 0.5f);
    CHECK(!sim.boss(0).defeated);
    const float speed_after_hit1 = sim.boss(0).speed();
    CHECK(speed_after_hit1 > Sim::kSentinelSpeed);

    // Esperar a que pase la invulnerabilidad
    while (sim.boss(0).hurt > 0.f || sim.boss(0).phase == 2) {
      place(sim, ck->x, ck->y);
      sim.step({});
    }

    // Verificar que en ciclo impar ejecuta fase 3 (embestida rasa)
    bool saw_charge = false;
    for (int i = 0; i < 300 && !saw_charge; ++i) {
      place(sim, ck->x, ck->y);
      sim.step({});
      if (sim.boss(0).phase == 3) saw_charge = true;
    }
    CHECK(saw_charge);

    // Esperar a que complete la embestida y esté en el suelo para el golpe 2
    while (sim.boss(0).phase == 3 || sim.boss(0).phase == 2 || sim.boss(0).hurt > 0.f) {
      place(sim, ck->x, ck->y);
      sim.step({});
    }

    // Golpe 2
    stomp_boss(sim);
    CHECK(sim.boss(0).hits == 2);
    CHECK(sim.boss(0).hurt > 0.5f);
    CHECK(!sim.boss(0).defeated);
    CHECK(sim.boss(0).speed() > speed_after_hit1);

    // Esperar a que pase la invulnerabilidad
    while (sim.boss(0).hurt > 0.f || sim.boss(0).phase == 2 || sim.boss(0).phase == 3) {
      place(sim, ck->x, ck->y);
      sim.step({});
    }

    // Golpe 3 (definitivo)
    stomp_boss(sim);
    CHECK(sim.boss(0).hits == 3);
    CHECK(sim.boss(0).defeated);
    CHECK(sim.bosses_defeated());
    CHECK(boss_ent->active);
    CHECK(door->active);

    // Secuencia de victoria CRT y diálogo
    const auto& vic = sim.victory_seq();
    CHECK(vic.active());
    CHECK(vic.kind() == BossKind::Sentinel);
    CHECK(vic.in_defeat_anim());

    // Esperar animación de desintegración CRT (la puerta se abre por completo durante este tiempo)
    int ticks = 0;
    game::BossVictoryDialog dlg;
    while (sim.victory_seq().in_defeat_anim() && ticks < 400) {
      CHECK(!sim.take_victory_dialog(dlg));
      sim.step({});
      ++ticks;
    }
    CHECK(ticks >= 155 && ticks <= 185);
    CHECK(!scene.door_blocks(34, 17) && !scene.door_blocks(34, 19));

    // Esperar AftermathPause
    while (sim.victory_seq().in_aftermath_pause()) {
      CHECK(!sim.take_victory_dialog(dlg));
      sim.step({});
    }

    // Entrega de diálogo al llegar a ReadyForDialog
    CHECK(sim.victory_seq().phase() == game::VictoryPhase::ReadyForDialog);
    CHECK(sim.take_victory_dialog(dlg));
    CHECK(dlg.speaker == game::kVictorySentinelSpeaker);
    CHECK(dlg.text == game::kVictorySentinelText);
    CHECK(sim.victory_seq().phase() == game::VictoryPhase::Done);

    // Cruzar puerta abierta y tocar la meta
    place(sim, goal->x, goal->y);
    sim.step({});
    CHECK(sim.finished());
  }

  // --- 6) Persistencia save.ewsv con el boss vencido ---
  const auto bytes = game::encode_save(game::capture_save(sim));
  game::SaveData decoded;
  CHECK(game::decode_save(bytes.data(), bytes.size(), decoded) == game::SaveError::None);
  CHECK(decoded.map == "sentinel");

  Scene s_load;
  CHECK(s_load.load(argv[1], err));
  Sim sim_load(s_load);
  CHECK(game::apply_save(sim_load, decoded) == game::SaveError::None);
  CHECK(sim_load.bosses_defeated());
  CHECK(find_name(s_load, "boss_sentinel")->active);
  CHECK(find_name(s_load, "door_exit")->active);
  CHECK(!s_load.door_blocks(34, 17));

  std::puts("sentinel_boss_test: OK");
  return 0;
}
