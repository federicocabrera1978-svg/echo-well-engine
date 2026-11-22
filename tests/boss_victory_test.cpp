// Test sin GL: secuencia ordenada de victoria al derrotar un boss (BossVictorySeq + Sim)
// sobre slice.ewmap (Watcher), level2.ewmap (Guardian2) y level3.ewmap (Wyrm).
#include "audio/sfx.hpp"
#include "game/boss_victory.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)

using namespace ew;
using game::BossKind;
using game::BossVictoryConfig;
using game::BossVictoryDialog;
using game::BossVictorySeq;
using game::Input;
using game::Sim;
using game::VictoryPhase;

static void place(Sim& s, float x, float y, float vy = 0.f) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = {x, y};
  p.prev = {x, y - vy};
  p.force = {};
  p.grounded = false;
}

static int test_unit_sequence() {
  // --- Caso A1: config custom (anim_sec=2.8, pause_sec=0.6, slow_mo=0.5, shake=2.5, particles=14) ---
  {
    BossVictoryConfig cfg;
    cfg.anim_sec = 2.8f;
    cfg.pause_sec = 0.6f;
    cfg.slow_mo_scale = 0.5f;
    cfg.shake_amp = 2.5f;
    cfg.particle_count = 14;

    BossVictorySeq seq(cfg);
    CHECK(seq.phase() == VictoryPhase::Idle);
    CHECK(!seq.active());
    CHECK(seq.time_scale() == 1.0f);

    seq.start(120.f, 80.f, BossKind::Watcher);
    CHECK(seq.phase() == VictoryPhase::DefeatAnim);
    CHECK(seq.active());
    CHECK(seq.in_defeat_anim());
    CHECK(!seq.in_aftermath_pause());
    CHECK(seq.flash_alpha() > 0.f);
    CHECK(seq.crt_progress() > 0.9f);
    CHECK(seq.particles().size() == 14u);

    BossVictoryDialog dlg;
    CHECK(!seq.poll_dialog(dlg));

    bool saw_shake = false;
    {
      float sx = 0.f, sy = 0.f;
      seq.camera_shake(sx, sy);
      if (std::fabs(sx) > 0.f || std::fabs(sy) > 0.f) saw_shake = true;
    }

    int anim_ticks = 0;
    while (seq.in_defeat_anim() && anim_ticks < 600) {
      CHECK(seq.in_defeat_anim());
      CHECK(!seq.poll_dialog(dlg));
      seq.step(Sim::kDt);
      ++anim_ticks;
      if (seq.in_defeat_anim()) {
        float sx = 0.f, sy = 0.f;
        seq.camera_shake(sx, sy);
        if (std::fabs(sx) > 0.f || std::fabs(sy) > 0.f) saw_shake = true;
        CHECK(!seq.poll_dialog(dlg));
      }
    }
    CHECK(saw_shake);
    CHECK(anim_ticks >= 167 && anim_ticks <= 169);

    // Transición exacta a AftermathPause.
    CHECK(seq.phase() == VictoryPhase::AftermathPause);
    CHECK(seq.in_aftermath_pause());
    CHECK(seq.active());
    CHECK(seq.time_scale() < 1.0f);
    CHECK(std::fabs(seq.time_scale() - 0.5f) < 1e-4f);
    CHECK(seq.crt_progress() == 0.f);
    CHECK(seq.flash_alpha() == 0.f);
    {
      float sx = 1.f, sy = 1.f;
      seq.camera_shake(sx, sy);
      CHECK(sx == 0.f && sy == 0.f);
    }

    int pause_ticks = 0;
    while (seq.in_aftermath_pause() && pause_ticks < 300) {
      CHECK(seq.in_aftermath_pause());
      CHECK(!seq.poll_dialog(dlg));
      CHECK(seq.time_scale() < 1.0f);
      CHECK(seq.crt_progress() == 0.f);
      float sx = 1.f, sy = 1.f;
      seq.camera_shake(sx, sy);
      CHECK(sx == 0.f && sy == 0.f);
      seq.step(Sim::kDt);
      ++pause_ticks;
      if (seq.in_aftermath_pause()) CHECK(!seq.poll_dialog(dlg));
    }
    CHECK(pause_ticks >= 35 && pause_ticks <= 37);

    // Fin de pausa -> ReadyForDialog -> poll_dialog UNA sola vez -> Done.
    CHECK(seq.phase() == VictoryPhase::ReadyForDialog);
    CHECK(!seq.active());
    CHECK(seq.time_scale() == 1.0f);
    CHECK(seq.poll_dialog(dlg));
    const auto expected = game::boss_victory_dialog(BossKind::Watcher);
    CHECK(dlg.speaker == expected.speaker);
    CHECK(dlg.text == expected.text);
    CHECK(seq.phase() == VictoryPhase::Done);
    CHECK(!seq.poll_dialog(dlg));
  }

  // --- Caso A2: 3.4 s + pausa 0.f (transición directa DefeatAnim -> ReadyForDialog) ---
  {
    BossVictoryConfig cfg;
    cfg.anim_sec = 3.4f;
    cfg.pause_sec = 0.f;
    cfg.slow_mo_scale = 0.5f;
    cfg.shake_amp = 2.0f;
    cfg.particle_count = 10;

    BossVictorySeq seq(cfg);
    seq.start(200.f, 100.f, BossKind::Wyrm);
    CHECK(seq.phase() == VictoryPhase::DefeatAnim);
    CHECK(seq.particles().size() == 10u);

    BossVictoryDialog dlg;
    int anim_ticks = 0;
    while (seq.in_defeat_anim() && anim_ticks < 600) {
      CHECK(!seq.poll_dialog(dlg));
      seq.step(Sim::kDt);
      ++anim_ticks;
      if (seq.in_defeat_anim()) CHECK(!seq.poll_dialog(dlg));
    }
    CHECK(anim_ticks >= 203 && anim_ticks <= 205);
    CHECK(!seq.in_aftermath_pause());
    CHECK(seq.phase() == VictoryPhase::ReadyForDialog);
    CHECK(!seq.active());
    CHECK(seq.time_scale() == 1.0f);
    CHECK(seq.poll_dialog(dlg));
    const auto expected = game::boss_victory_dialog(BossKind::Wyrm);
    CHECK(dlg.speaker == expected.speaker);
    CHECK(dlg.text == expected.text);
    CHECK(seq.phase() == VictoryPhase::Done);
    CHECK(!seq.poll_dialog(dlg));
  }

  return 0;
}

struct LevelBossSpec {
  const char* path;
  const char* ckpt_name;
  float ckpt_x;
  float ckpt_y;
  float safe_x;
  float safe_y;
  BossKind kind;
  const char* expected_speaker;
};

static int test_sim_boss_victory(const LevelBossSpec& spec) {
  std::string err;
  game::Scene scene;
  if (!scene.load(spec.path, err)) {
    std::fprintf(stderr, "No se pudo cargar %s: %s\n", spec.path, err.c_str());
    return 1;
  }

  Sim sim(scene);
  CHECK(sim.boss_count() == 1 && !sim.bosses_defeated());

  BossVictoryConfig cfg;
  cfg.anim_sec = 2.6f;
  cfg.pause_sec = 0.5f;
  cfg.slow_mo_scale = 0.55f;
  cfg.shake_amp = 2.5f;
  cfg.particle_count = 14;
  sim.set_victory_config(cfg);

  // Activar el checkpoint de la arena del boss.
  place(sim, spec.ckpt_x, spec.ckpt_y);
  sim.step({});
  CHECK(sim.checkpoint() && sim.checkpoint()->name == spec.ckpt_name);
  sim.take_sfx();

  BossVictoryDialog dlg;
  const int max_hits = sim.boss(0).max_hits();
  for (int k = 0; k < max_hits && !sim.bosses_defeated(); ++k) {
    for (int i = 0; i < 300 && (sim.boss(0).hurt > 0.f || sim.boss(0).phase == 2); ++i) {
      place(sim, spec.safe_x, spec.safe_y);
      sim.step({});
      CHECK(!sim.take_victory_dialog(dlg));
    }
    const auto& b = sim.boss(0);
    place(sim, b.x, b.y - b.height() - Sim::kPlayerRadius + 1.5f, 2.f);
    sim.step({});
    CHECK(sim.boss(0).hits == k + 1);
    CHECK(sim.deaths() == 0);
  }

  // Justo al derrotar al boss.
  CHECK(sim.bosses_defeated());
  CHECK(sim.boss_burst() > 0);
  const uint32_t sfx = sim.take_sfx();
  CHECK((sfx & audio::kSfxBossCrt) != 0u);
  CHECK(sim.victory_seq().in_defeat_anim());
  CHECK(!sim.take_victory_dialog(dlg));

  // Fase 1: DefeatAnim (~2.6 s). El diálogo NO se dispara todavía.
  int anim_ticks = 0;
  while (sim.victory_seq().in_defeat_anim() && anim_ticks < 600) {
    CHECK(!sim.take_victory_dialog(dlg));
    sim.step({});
    ++anim_ticks;
    if (sim.victory_seq().in_defeat_anim()) {
      CHECK(sim.boss_burst() > 0);
      CHECK(!sim.take_victory_dialog(dlg));
    }
  }
  CHECK(anim_ticks >= 155 && anim_ticks <= 157);
  CHECK(sim.boss_burst() == 0);

  // Fase 2: AftermathPause (~0.5 s). Jugador libre para moverse y diálogo sigue bloqueado.
  CHECK(sim.victory_seq().in_aftermath_pause());
  CHECK(sim.victory_seq().time_scale() < 1.0f);
  CHECK(!sim.take_victory_dialog(dlg));

  const float move_start_x = sim.player().pos.x;
  const float move_dir = (move_start_x >= spec.ckpt_x + 30.f) ? -1.f : 1.f;
  int pause_ticks = 0;
  while (sim.victory_seq().in_aftermath_pause() && pause_ticks < 300) {
    CHECK(!sim.take_victory_dialog(dlg));
    Input move_in;
    move_in.move_x = move_dir;
    sim.step(move_in);
    ++pause_ticks;
    CHECK(!sim.finished());
    if (sim.victory_seq().in_aftermath_pause()) CHECK(!sim.take_victory_dialog(dlg));
  }
  CHECK(pause_ticks >= 29 && pause_ticks <= 31);
  CHECK(std::fabs(sim.player().pos.x - move_start_x) > 5.0f);

  // Fase 3: ReadyForDialog -> entrega el diálogo esperado una sola vez y pasa a Done.
  CHECK(sim.victory_seq().phase() == VictoryPhase::ReadyForDialog);
  CHECK(sim.victory_seq().time_scale() == 1.0f);
  CHECK(sim.take_victory_dialog(dlg));
  const auto expected = game::boss_victory_dialog(spec.kind);
  CHECK(dlg.speaker == spec.expected_speaker);
  CHECK(dlg.speaker == expected.speaker);
  CHECK(dlg.text == expected.text);
  CHECK(sim.victory_seq().phase() == VictoryPhase::Done);
  CHECK(!sim.take_victory_dialog(dlg));

  // respawn() con el boss ya vencido deja la secuencia en Done y no repite diálogo.
  sim.respawn();
  CHECK(sim.bosses_defeated());
  CHECK(sim.boss_burst() == 0);
  CHECK(sim.victory_seq().phase() == VictoryPhase::Done);
  CHECK(!sim.take_victory_dialog(dlg));
  for (int i = 0; i < 10; ++i) {
    sim.step({});
    CHECK(!sim.take_victory_dialog(dlg));
  }
  CHECK(sim.victory_seq().phase() == VictoryPhase::Done);

  // load_state() con el boss ya vencido deja la secuencia en Done y no repite diálogo.
  std::vector<uint8_t> active;
  active.reserve(scene.entities().size());
  for (const auto& e : scene.entities()) active.push_back(e.active ? 1u : 0u);
  sim.load_state(sim.checkpoint_index(), sim.gravity_flipped(), active, sim.abilities());
  CHECK(sim.bosses_defeated());
  CHECK(sim.boss_burst() == 0);
  CHECK(sim.victory_seq().phase() == VictoryPhase::Done);
  CHECK(!sim.take_victory_dialog(dlg));
  for (int i = 0; i < 10; ++i) {
    sim.step({});
    CHECK(!sim.take_victory_dialog(dlg));
  }
  CHECK(sim.victory_seq().phase() == VictoryPhase::Done);

  // Save/load completo también conserva la secuencia en Done.
  const auto saved = game::capture_save(sim);
  game::Scene scene2;
  CHECK(scene2.load(spec.path, err));
  Sim sim2(scene2);
  CHECK(game::apply_save(sim2, saved) == game::SaveError::None);
  CHECK(sim2.bosses_defeated());
  CHECK(sim2.boss_burst() == 0);
  CHECK(sim2.victory_seq().phase() == VictoryPhase::Done);
  CHECK(!sim2.take_victory_dialog(dlg));

  std::printf("boss_victory (%s -> %s): OK\n", spec.path, spec.expected_speaker);
  return 0;
}

int main(int argc, char** argv) {
  if (argc < 4) return 2;

  if (test_unit_sequence() != 0) return 1;

  const LevelBossSpec specs[] = {
      {argv[1], "ck_boss", 2264.f, 152.f, 2264.f, 152.f, BossKind::Watcher, "SELLO DE LA ASCENSION"},
      {argv[2], "ck_boss2", 984.f, 152.f, 984.f, 152.f, BossKind::Guardian2, "SELLO DE LAS CATACUMBAS"},
      {argv[3], "ck_wyrm_arena", 664.f, 152.f, 648.f, 88.f, BossKind::Wyrm, "CORONA DE LA PUREZA"},
  };
  for (const auto& spec : specs) {
    if (test_sim_boss_victory(spec) != 0) return 1;
  }

  std::puts("boss_victory_test: OK");
  return 0;
}
