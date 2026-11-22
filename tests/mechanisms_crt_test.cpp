// Test headless (sin GL/GLFW): cobertura de altares (checkpoint), palancas (switch/flood_switch),
// enemigos menores (imp) y efecto/síntesis CRT al derrotar bosses (Watcher y Wyrm).
#include "audio/sfx.hpp"
#include "game/frame_view.hpp"
#include "game/sim.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)

using namespace ew;
using game::Entity;
using game::FrameData;
using game::Input;
using game::Scene;
using game::Sim;

namespace {

bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

void place(Sim& s, float x, float y, float vy = 0.f) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = {x, y};
  p.prev = {x, y - vy};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

const Entity* find_name(const Scene& s, const char* name) {
  for (const auto& e : s.entities()) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

int find_marker(const FrameData& v, int shape, float x, float y, float tol = 1.5f) {
  for (int i = 0; i < v.marker_count; ++i) {
    const float mx = v.markers[std::size_t(i * 4 + 0)];
    const float my = v.markers[std::size_t(i * 4 + 1)];
    const int sh = int(std::lround(v.markers[std::size_t(i * 4 + 3)]));
    if (sh == shape && std::fabs(mx - x) <= tol && std::fabs(my - y) <= tol) return i;
  }
  return -1;
}

bool marker_color_eq(const FrameData& v, int idx, float r, float g, float b) {
  return idx >= 0 &&
         approx(v.marker_colors[std::size_t(idx * 3 + 0)], r, 1e-3f) &&
         approx(v.marker_colors[std::size_t(idx * 3 + 1)], g, 1e-3f) &&
         approx(v.marker_colors[std::size_t(idx * 3 + 2)], b, 1e-3f);
}

int count_imp_markers(const FrameData& v, const Sim::Imp& im) {
  int hits = 0;
  for (int i = 0; i < v.marker_count; ++i) {
    const float mx = v.markers[std::size_t(i * 4 + 0)];
    const float my = v.markers[std::size_t(i * 4 + 1)];
    const float mr = v.markers[std::size_t(i * 4 + 2)];
    const int sh = int(std::lround(v.markers[std::size_t(i * 4 + 3)]));
    const float cr = v.marker_colors[std::size_t(i * 3 + 0)];
    const float cg = v.marker_colors[std::size_t(i * 3 + 1)];
    const float cb = v.marker_colors[std::size_t(i * 3 + 2)];
    if (std::fabs(mx - im.x) > 4.f || std::fabs(my - (im.y - 7.f)) > 5.f) continue;
    const bool body = (sh == 1 && approx(mr, 4.5f, 1e-2f) && approx(cr, 0.32f, 1e-2f) &&
                       approx(cg, 0.14f, 1e-2f) && approx(cb, 0.24f, 1e-2f));
    const bool horn = (sh == 1 && approx(mr, 1.2f, 1e-2f) && approx(cr, 0.55f, 1e-2f) &&
                       approx(cg, 0.28f, 1e-2f) && approx(cb, 0.32f, 1e-2f));
    const bool eye = (sh == 0 && approx(mr, 1.2f, 1e-2f) && approx(cr, 1.0f, 1e-2f) &&
                      approx(cg, 0.42f, 1e-2f) && approx(cb, 0.15f, 1e-2f));
    if (body || horn || eye) ++hits;
  }
  return hits;
}

int test_checkpoints(const char* slice_path) {
  Scene scene;
  std::string err;
  if (!scene.load(slice_path, err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  Sim sim(scene);

  const Entity* ck_ledge = find_name(scene, "ck_ledge");
  const Entity* ck_grav = find_name(scene, "ck_grav");
  const Entity* flip_up = find_name(scene, "flip_up");
  CHECK(ck_ledge && ck_grav && flip_up);
  CHECK(!ck_ledge->active && !ck_grav->active);
  CHECK(sim.checkpoint() == nullptr);

  // Antes de tomar ck_ledge: marcador shape == 2 con color inactivo (0.25, 0.55, 0.70).
  {
    FrameData v;
    scene.follow(ck_ledge->x, ck_ledge->y);
    game::build_world_frame(v, scene, sim, 0.f, false, false);
    const int idx = find_marker(v, 2, std::round(ck_ledge->x), std::round(ck_ledge->y), 0.5f);
    CHECK(idx >= 0);
    CHECK(marker_color_eq(v, idx, 0.25f, 0.55f, 0.70f));
  }

  // Fuera de kCheckpointReach (20 px) no se activa.
  place(sim, ck_ledge->x - (Sim::kCheckpointReach + 1.5f), ck_ledge->y);
  sim.step({});
  CHECK(sim.checkpoint() == nullptr);
  CHECK(!ck_ledge->active);
  CHECK((sim.take_sfx() & audio::kSfxCheckpoint) == 0u);

  // Dentro de kCheckpointReach se activa, dispara kSfxCheckpoint y cambia a verde (0.30, 1.0, 0.55).
  place(sim, ck_ledge->x - (Sim::kCheckpointReach - 1.5f), ck_ledge->y);
  sim.step({});
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_ledge");
  CHECK(ck_ledge->active);
  CHECK((sim.take_sfx() & audio::kSfxCheckpoint) != 0u);
  {
    FrameData v;
    scene.follow(ck_ledge->x, ck_ledge->y);
    game::build_world_frame(v, scene, sim, 0.f, false, false);
    const int idx = find_marker(v, 2, std::round(ck_ledge->x), std::round(ck_ledge->y), 0.5f);
    CHECK(idx >= 0);
    CHECK(marker_color_eq(v, idx, 0.30f, 1.0f, 0.55f));
  }

  // Persiste tras R corto y tras muerte.
  {
    Input r;
    r.restart = true;
    sim.step(r);
  }
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_ledge");
  CHECK(ck_ledge->active);
  CHECK(approx(sim.player().pos.x, ck_ledge->x, 1e-2f) && approx(sim.player().pos.y, ck_ledge->y, 1e-2f));

  for (int i = 0; i < 35; ++i) sim.step({});
  CHECK(sim.invuln() == 0);
  place(sim, 640.f, 156.f);  // foso de pinchos META 4 en chunk 2
  sim.step({});
  CHECK(sim.deaths() == 1);
  CHECK((sim.take_sfx() & audio::kSfxDie) != 0u);
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_ledge");
  CHECK(ck_ledge->active);
  CHECK(approx(sim.player().pos.x, ck_ledge->x, 1e-2f) && approx(sim.player().pos.y, ck_ledge->y, 1e-2f));

  // Activar ck_grav con gravedad invertida la preserva tras R corto y muerte.
  for (int i = 0; i < 35; ++i) sim.step({});
  place(sim, flip_up->x, flip_up->y);
  sim.step({});
  CHECK(sim.gravity_flipped());
  place(sim, ck_grav->x, ck_grav->y);
  sim.step({});
  CHECK(sim.checkpoint() && sim.checkpoint()->name == "ck_grav");
  CHECK(sim.checkpoint_flipped());
  CHECK(ck_grav->active && !ck_ledge->active);
  CHECK((sim.take_sfx() & audio::kSfxCheckpoint) != 0u);

  sim.set_gravity_flipped(false);
  CHECK(!sim.gravity_flipped());
  {
    Input r;
    r.restart = true;
    sim.step(r);
  }
  CHECK(sim.gravity_flipped());
  CHECK(sim.player().gscale == -1.f);
  CHECK(approx(sim.player().pos.x, ck_grav->x, 1e-2f) && approx(sim.player().pos.y, ck_grav->y, 1e-2f));

  for (int i = 0; i < 35; ++i) sim.step({});
  sim.set_gravity_flipped(false);
  place(sim, 2000.f, 156.f);  // pinchos del piso en chunk 6
  sim.step({});
  CHECK(sim.deaths() == 2);
  CHECK(sim.gravity_flipped());
  CHECK(sim.player().gscale == -1.f);

  // R largo devuelve los altares a estado inactivo.
  {
    Input full;
    full.restart_full = true;
    sim.step(full);
  }
  CHECK(sim.checkpoint() == nullptr);
  CHECK(!sim.gravity_flipped() && !sim.checkpoint_flipped());
  CHECK(!ck_ledge->active && !ck_grav->active);
  {
    FrameData v;
    scene.follow(ck_ledge->x, ck_ledge->y);
    game::build_world_frame(v, scene, sim, 0.f, false, false);
    const int idx = find_marker(v, 2, std::round(ck_ledge->x), std::round(ck_ledge->y), 0.5f);
    CHECK(idx >= 0);
    CHECK(marker_color_eq(v, idx, 0.25f, 0.55f, 0.70f));
  }
  return 0;
}

int test_switches_and_flood(const char* slice_path) {
  Scene scene;
  std::string err;
  if (!scene.load(slice_path, err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  Sim sim(scene);

  const Entity* sw_start = find_name(scene, "switch_start");
  const Entity* door_start = find_name(scene, "door_start");
  const Entity* sw_well = find_name(scene, "switch_well");
  const Entity* door_hi = find_name(scene, "door_well_hi");
  const Entity* door_lo = find_name(scene, "door_well_lo");
  const Entity* flood = find_name(scene, "flood");
  CHECK(sw_start && door_start && sw_well && door_hi && door_lo && flood);

  // switch_start fuera de kSwitchReach (14 px): no activa y emite shape 3 ámbar + flecha shape 4 encima.
  place(sim, sw_start->x - (game::kSwitchReach + 1.5f), sw_start->y);
  {
    FrameData v;
    scene.follow(sw_start->x, sw_start->y);
    game::build_world_frame(v, scene, sim, 0.2f, false, false);
    const int sw_idx = find_marker(v, 3, std::round(sw_start->x), std::round(sw_start->y), 0.5f);
    CHECK(sw_idx >= 0);
    CHECK(marker_color_eq(v, sw_idx, 1.0f, 0.55f, 0.12f));
    const int arr_idx = find_marker(v, 4, std::round(sw_start->x), std::round(sw_start->y - 17.f), 3.0f);
    CHECK(arr_idx >= 0);
    CHECK(marker_color_eq(v, arr_idx, 0.f, 1.f, 0.f));
  }
  {
    Input in;
    in.interact = true;
    sim.step(in);
  }
  CHECK(!sw_start->active && !door_start->active);
  CHECK((sim.take_sfx() & audio::kSfxSwitch) == 0u);

  // Dentro de kSwitchReach: in.interact activa, dispara kSfxSwitch, vuelve verde y quita flecha encima.
  place(sim, sw_start->x - (game::kSwitchReach - 1.5f), sw_start->y);
  {
    Input in;
    in.interact = true;
    sim.step(in);
  }
  CHECK(sw_start->active && door_start->active);
  CHECK((sim.take_sfx() & audio::kSfxSwitch) != 0u);
  {
    FrameData v;
    scene.follow(sw_start->x, sw_start->y);
    game::build_world_frame(v, scene, sim, 0.2f, false, false);
    const int sw_idx = find_marker(v, 3, std::round(sw_start->x), std::round(sw_start->y), 0.5f);
    CHECK(sw_idx >= 0);
    CHECK(marker_color_eq(v, sw_idx, 0.25f, 1.0f, 0.50f));
    const int arr_idx = find_marker(v, 4, std::round(sw_start->x), std::round(sw_start->y - 17.f), 3.5f);
    CHECK(arr_idx < 0);
  }

  // Múltiples puertas linkeadas (switch_well -> door_well_hi, door_well_lo) y anim 0 -> 1.
  CHECK(!sw_well->active && !door_hi->active && !door_lo->active);
  CHECK(approx(door_hi->anim, 0.f) && approx(door_lo->anim, 0.f));
  CHECK(scene.door_blocks(141, 14) && scene.door_blocks(141, 17));
  place(sim, sw_well->x, sw_well->y);
  {
    Input in;
    in.interact = true;
    sim.step(in);
  }
  CHECK(sw_well->active && door_hi->active && door_lo->active);
  CHECK((sim.take_sfx() & audio::kSfxSwitch) != 0u);
  for (int i = 0; i < 4; ++i) sim.step({});
  CHECK(door_hi->anim > 0.f && door_hi->anim < 1.f);
  CHECK(door_lo->anim > 0.f && door_lo->anim < 1.f);
  CHECK(scene.door_blocks(141, 14) && scene.door_blocks(141, 17));
  for (int i = 0; i < 20; ++i) sim.step({});
  CHECK(approx(door_hi->anim, 1.f) && approx(door_lo->anim, 1.f));
  CHECK(!scene.door_blocks(141, 14) && !scene.door_blocks(141, 17));

  // flood_switch sube solo agua x >= 400 hasta Sim::kFloodRestY y la baja al apagarlo.
  CHECK(sim.water().size() == 2u);
  CHECK(sim.water()[0].x() < 400.f && sim.water()[1].x() >= 400.f);
  const float w0_y0 = sim.water()[0].rest_y();
  const float w1_y0 = sim.water()[1].rest_y();
  CHECK(w1_y0 > Sim::kFloodRestY + 10.f);

  place(sim, flood->x, flood->y);
  {
    Input in;
    in.interact = true;
    sim.step(in);
  }
  CHECK(flood->active);
  CHECK((sim.take_sfx() & audio::kSfxSwitch) != 0u);
  for (int i = 0; i < 90; ++i) sim.step({});
  CHECK(approx(sim.water()[0].rest_y(), w0_y0));
  CHECK(approx(sim.water()[1].rest_y(), Sim::kFloodRestY, 0.1f));

  place(sim, flood->x, flood->y);
  {
    Input in;
    in.interact = true;
    sim.step(in);
  }
  CHECK(!flood->active);
  for (int i = 0; i < 90; ++i) sim.step({});
  CHECK(approx(sim.water()[0].rest_y(), w0_y0));
  CHECK(approx(sim.water()[1].rest_y(), w1_y0, 0.1f));
  return 0;
}

int test_imps(const char* level3_path) {
  Scene scene;
  std::string err;
  if (!scene.load(level3_path, err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  Sim sim(scene);
  CHECK(sim.imp_count() == 1 && !sim.imp(0).defeated);

  // Patrulla acotada en home_x ± Sim::kImpRange y rebote en ambos extremos.
  const float hx = sim.imp(0).home_x;
  float min_x = sim.imp(0).x, max_x = sim.imp(0).x;
  bool saw_pos = false, saw_neg = false;
  for (int i = 0; i < 240; ++i) {
    sim.step({});
    const auto& im = sim.imp(0);
    CHECK(im.x >= hx - Sim::kImpRange - 1e-4f && im.x <= hx + Sim::kImpRange + 1e-4f);
    if (im.x < min_x) min_x = im.x;
    if (im.x > max_x) max_x = im.x;
    if (im.dir > 0) saw_pos = true;
    if (im.dir < 0) saw_neg = true;
  }
  CHECK(min_x <= hx - Sim::kImpRange + 1.f && max_x >= hx + Sim::kImpRange - 1.f);
  CHECK(saw_pos && saw_neg);

  // build_world_frame centrado en el imp emite sus 4 marcadores mientras !im.defeated.
  {
    FrameData v;
    scene.follow(sim.imp(0).x, sim.imp(0).y);
    game::build_world_frame(v, scene, sim, 0.25f, false, false);
    CHECK(count_imp_markers(v, sim.imp(0)) == 4);
  }

  // Muerte por contacto lateral.
  place(sim, sim.imp(0).x, sim.imp(0).y - 4.f);
  sim.step({});
  CHECK(sim.deaths() == 1);
  CHECK((sim.take_sfx() & audio::kSfxDie) != 0u);

  // Derrota por pisotón: rebote + recarga de Doble Salto y Dash + kSfxHit + sin marcadores.
  sim.grant_ability(game::kAbilityDoubleJump | game::kAbilityDash);
  for (int i = 0; i < 35; ++i) sim.step({});
  CHECK(sim.invuln() == 0);

  // Agotar Doble Salto y Dash en el aire antes de pisar al imp.
  place(sim, hx, 285.f);
  sim.step({});
  {
    Input j;
    j.jump = true;
    sim.step(j);
  }
  {
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
  }
  for (int i = 0; i < 12; ++i) sim.step({});
  CHECK(sim.dash_ticks() == 0 && sim.air_jumps() == 0 && !sim.dash_ready());
  sim.take_sfx();

  {
    const auto& im = sim.imp(0);
    place(sim, im.x, im.y - Sim::kImpH - Sim::kPlayerRadius + 1.5f, 2.f);
  }
  sim.step({});
  CHECK(sim.imp(0).defeated);
  CHECK(sim.deaths() == 1);
  CHECK(sim.player().velocity().y < 0.f);
  CHECK(sim.air_jumps() == 1 && sim.dash_ready());
  CHECK((sim.take_sfx() & audio::kSfxHit) != 0u);
  {
    FrameData v;
    scene.follow(sim.imp(0).x, sim.imp(0).y);
    game::build_world_frame(v, scene, sim, 0.25f, false, false);
    CHECK(count_imp_markers(v, sim.imp(0)) == 0);
  }

  // R corto lo restaura.
  {
    Input r;
    r.restart = true;
    sim.step(r);
  }
  CHECK(!sim.imp(0).defeated);
  {
    FrameData v;
    scene.follow(sim.imp(0).x, sim.imp(0).y);
    game::build_world_frame(v, scene, sim, 0.25f, false, false);
    CHECK(count_imp_markers(v, sim.imp(0)) == 4);
  }

  // Derrota por Dash + desaparición de marcadores.
  for (int i = 0; i < 35; ++i) sim.step({});
  place(sim, sim.imp(0).x - 10.f, sim.imp(0).y - 4.f);
  sim.take_sfx();
  {
    Input d;
    d.move_x = 1.f;
    d.dash = true;
    sim.step(d);
  }
  uint32_t sfx = sim.take_sfx();
  for (int i = 0; i < 8 && !sim.imp(0).defeated; ++i) {
    sim.step({});
    sfx |= sim.take_sfx();
  }
  CHECK(sim.imp(0).defeated && sim.deaths() == 1);
  CHECK((sfx & audio::kSfxHit) != 0u);
  {
    FrameData v;
    scene.follow(sim.imp(0).x, sim.imp(0).y);
    game::build_world_frame(v, scene, sim, 0.25f, false, false);
    CHECK(count_imp_markers(v, sim.imp(0)) == 0);
  }
  return 0;
}

int test_boss_crt_audio() {
  using namespace ew::audio;
  Mixer mixer;
  mixer.play(SfxId::BossCrt);
  CHECK(mixer.active() == 1);

  std::vector<float> pcm(std::size_t(kRate), 0.f);  // 1.0 s (BossCrt dura 0.85 s)
  mixer.render(pcm.data(), kRate, kRate);
  for (float s : pcm) CHECK(std::isfinite(s) && s >= -1.f && s <= 1.f);

  const int split = kRate / 4;      // 0.25 s
  const int sweep_end = (kRate * 4) / 5;  // 0.80 s
  double e_start = 0.0, e_sweep = 0.0;
  for (int i = 0; i < split; ++i) e_start += double(pcm[std::size_t(i)]) * double(pcm[std::size_t(i)]);
  for (int i = split; i < sweep_end; ++i) e_sweep += double(pcm[std::size_t(i)]) * double(pcm[std::size_t(i)]);
  CHECK(e_start > 1.0 && e_sweep > 1.0);
  CHECK(mixer.active() == 0);

  std::vector<float> tail(256, 99.f);
  mixer.render(tail.data(), int(tail.size()), kRate);
  for (float s : tail) CHECK(s == 0.f);
  return 0;
}

int test_boss_crt_level(const char* map_path, const char* ckpt_name, float ckpt_x, float ckpt_y,
                        float safe_x, float safe_y) {
  Scene scene;
  std::string err;
  if (!scene.load(map_path, err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  Sim sim(scene);
  CHECK(sim.boss_count() == 1 && !sim.bosses_defeated());

  place(sim, ckpt_x, ckpt_y);
  sim.step({});
  CHECK(sim.checkpoint() && sim.checkpoint()->name == ckpt_name);
  sim.take_sfx();

  const int max_hits = sim.boss(0).max_hits();
  for (int k = 0; k < max_hits && !sim.bosses_defeated(); ++k) {
    for (int i = 0; i < 300 && (sim.boss(0).hurt > 0.f || sim.boss(0).phase == 2); ++i) {
      place(sim, safe_x, safe_y);
      sim.step({});
    }
    const auto& b = sim.boss(0);
    place(sim, b.x, b.y - b.height() - Sim::kPlayerRadius + 1.5f, 2.f);
    sim.step({});
    CHECK(sim.boss(0).hits == k + 1);
    CHECK(sim.deaths() == 0);
  }

  CHECK(sim.bosses_defeated());
  CHECK((sim.take_sfx() & audio::kSfxBossCrt) != 0u);
  CHECK(sim.boss_burst() > 0);
  CHECK(sim.victory_seq().in_defeat_anim());

  float prev_crt = sim.victory_seq().crt_progress();
  CHECK(prev_crt > 0.95f && prev_crt <= 1.0f);

  bool saw_even = false, saw_odd = false, saw_particles = false;
  float time_s = 0.f;
  int guard = 0;
  while (sim.victory_seq().in_defeat_anim() && guard++ < 400) {
    const float cur_crt = sim.victory_seq().crt_progress();
    CHECK(cur_crt <= prev_crt + 1e-6f);
    prev_crt = cur_crt;

    FrameData v;
    scene.follow(sim.boss(0).x, sim.boss(0).y);
    game::build_world_frame(v, scene, sim, time_s, false, false);
    CHECK(v.boss_burst_t > 0.f);
    CHECK(v.boss_flash > 0.f);

    const bool even_tick = (sim.boss_burst() % 2 == 0);
    CHECK(v.boss_count == (even_tick ? 0 : 1));
    if (even_tick) saw_even = true;
    else saw_odd = true;

    int crt_markers = 0;
    const auto& vic = sim.victory_seq();
    for (int i = 0; i < v.marker_count; ++i) {
      const float mx = v.markers[std::size_t(i * 4 + 0)];
      const float my = v.markers[std::size_t(i * 4 + 1)];
      const int sh = int(std::lround(v.markers[std::size_t(i * 4 + 3)]));
      if (sh != 1) continue;
      for (const auto& pt : vic.particles()) {
        if (pt.life > 0.f && approx(mx, pt.x, 1e-2f) && approx(my, pt.y, 1e-2f)) {
          ++crt_markers;
          break;
        }
      }
    }
    if (guard == 1) CHECK(crt_markers > 0);
    if (crt_markers > 0) saw_particles = true;

    sim.step({});
    time_s += Sim::kDt;
  }

  CHECK(guard > 150 && guard < 400);
  CHECK(saw_even && saw_odd && saw_particles);
  CHECK(sim.boss_burst() == 0);
  CHECK(sim.victory_seq().crt_progress() == 0.f);

  FrameData v_end;
  scene.follow(sim.boss(0).x, sim.boss(0).y);
  game::build_world_frame(v_end, scene, sim, time_s, false, false);
  CHECK(v_end.boss_burst_t == 0.f);
  CHECK(v_end.boss_flash == 0.f);
  CHECK(v_end.boss_count == 0);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  if (test_checkpoints(argv[1]) != 0) return 1;
  if (test_switches_and_flood(argv[1]) != 0) return 1;
  if (test_imps(argv[2]) != 0) return 1;
  if (test_boss_crt_audio() != 0) return 1;
  if (test_boss_crt_level(argv[1], "ck_boss", 2264.f, 152.f, 2264.f, 152.f) != 0) return 1;
  if (test_boss_crt_level(argv[2], "ck_wyrm_arena", 664.f, 152.f, 648.f, 88.f) != 0) return 1;
  std::puts("mechanisms_crt_test: OK");
  return 0;
}
