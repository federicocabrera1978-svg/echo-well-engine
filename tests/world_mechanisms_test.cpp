// Test sin GL (Etapa 9 Fase A): puertas unidireccionales (`one_way_door`) y ascensores (`elevator`).
#include "audio/sfx.hpp"
#include "game/frame_view.hpp"
#include "game/save.hpp"
#include "game/sim.hpp"
#include "writer.hpp"
#include "yaml_lite.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

static int find_idx(const Scene& s, const char* name) {
  const auto& ents = s.entities();
  for (std::size_t i = 0; i < ents.size(); ++i)
    if (ents[i].name == name) return int(i);
  return -1;
}

int main() {
  const std::string src = R"(name: world_mech_test
size_chunks: [1, 1]
autotile: false
rects:
  - {layer: COL, x: 0, y: 20, w: 40, h: 3, tile: 1}
  - {layer: COL, x: 10, y: 0, w: 1, h: 17, tile: 1}
  - {layer: COL, x: 16, y: 0, w: 1, h: 17, tile: 1}
  - {layer: COL, x: 20, y: 9, w: 4, h: 1, tile: 1}
entities:
  - {name: sp, type: player_spawn, x: 40, y: 152}
  - {name: gate_r, type: one_way_door, x: 80, y: 136}
  - {name: gate_l, type: one_way_door, x: 128, y: 136}
  - {name: sw_elev, type: switch, x: 164, y: 68}
  - {name: elev_locked, type: elevator, x: 192, y: 72, w: 3, h: 1, to_x: 192, to_y: 152, speed: 60, pause: 0.2}
  - {name: elev_free, type: elevator, x: 248, y: 72, w: 3, h: 1, to_x: 248, to_y: 152, speed: 60, pause: 0.2}
links:
  - {from: sw_elev, to: elev_locked}
)";

  ewmapc::YamlDoc doc;
  std::string err;
  CHECK(ewmapc::parse_yaml_lite(src, doc, err));
  std::vector<std::byte> bin;
  CHECK(ewmapc::build_ewmap(doc, {true}, bin, err));

  const auto tmp_path = std::filesystem::temp_directory_path() / "ew_world_mechanisms_test.ewmap";
  {
    std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
    CHECK(bool(f));
    f.write(reinterpret_cast<const char*>(bin.data()), std::streamsize(bin.size()));
    CHECK(bool(f));
  }
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() { std::error_code ec; std::filesystem::remove(p, ec); }
  } cleanup{tmp_path};

  Scene scene;
  CHECK(scene.load(tmp_path, err));
  Sim sim(scene);

  const int idx_r = find_idx(scene, "gate_r");
  const int idx_l = find_idx(scene, "gate_l");
  const int idx_sw = find_idx(scene, "sw_elev");
  const int idx_el = find_idx(scene, "elev_locked");
  const int idx_ef = find_idx(scene, "elev_free");
  CHECK(idx_r >= 0 && idx_l >= 0 && idx_sw >= 0 && idx_el >= 0 && idx_ef >= 0);

  // --- 1) one_way_door: bloqueo desde lado cerrado, apertura desde lado opuesto, respawn y save ---
  CHECK(!scene.entities()[std::size_t(idx_r)].active);
  CHECK(!scene.entities()[std::size_t(idx_l)].active);
  CHECK(scene.solid_tile(10, 17) && scene.solid_tile(10, 19));
  CHECK(scene.solid_tile(16, 17) && scene.solid_tile(16, 19));

  // Empujar gate_r desde el lado izquierdo (bloqueado) no la abre.
  place(sim, 40.f, 156.f);
  for (int i = 0; i < 60; ++i) sim.step({1.f});
  CHECK(sim.player().pos.x <= 80.f - Sim::kPlayerRadius + 0.6f);
  CHECK(!scene.entities()[std::size_t(idx_r)].active);

  // Interactuar en el lado bloqueado emite clink (kSfxHit) y sigue cerrada.
  (void)sim.take_sfx();
  { Input act; act.interact = true; sim.step(act); }
  CHECK((sim.take_sfx() & ew::audio::kSfxHit) != 0);
  CHECK(!scene.entities()[std::size_t(idx_r)].active);

  // Interactuar desde el lado derecho (desbloqueable) abre gate_r permanentemente.
  place(sim, 96.f, 156.f);
  { Input act; act.interact = true; sim.step(act); }
  CHECK((sim.take_sfx() & ew::audio::kSfxSwitch) != 0);
  CHECK(scene.entities()[std::size_t(idx_r)].active);
  for (int i = 0; i < 20; ++i) sim.step({});
  CHECK(scene.entities()[std::size_t(idx_r)].anim == 1.f);
  CHECK(!scene.solid_tile(10, 17) && !scene.solid_tile(10, 19));

  // gate_l (termina en _l): bloqueada desde derecha, pero se abre al empujar desde izquierda.
  place(sim, 140.f, 156.f);
  { Input act; act.interact = true; sim.step(act); }
  CHECK((sim.take_sfx() & ew::audio::kSfxHit) != 0);
  CHECK(!scene.entities()[std::size_t(idx_l)].active);

  place(sim, 114.f, 156.f);
  (void)sim.take_sfx();
  bool opened_by_push = false;
  for (int i = 0; i < 40; ++i) {
    sim.step({1.f});
    if (sim.take_sfx() & ew::audio::kSfxSwitch) opened_by_push = true;
  }
  CHECK(opened_by_push && scene.entities()[std::size_t(idx_l)].active);
  CHECK(sim.player().pos.x > 136.f);

  // respawn() conserva one_way_door abiertas.
  { Input rst; rst.restart = true; sim.step(rst); }
  CHECK(scene.entities()[std::size_t(idx_r)].active);
  CHECK(scene.entities()[std::size_t(idx_l)].active);

  // save.ewsv round-trip conserva one_way_door abiertas (con anim == 1 y sin colisión).
  const auto bytes = encode_save(capture_save(sim));
  SaveData loaded;
  CHECK(decode_save(bytes.data(), bytes.size(), loaded) == SaveError::None);
  { Input full; full.restart_full = true; sim.step(full); }
  CHECK(!scene.entities()[std::size_t(idx_r)].active);
  CHECK(scene.solid_tile(10, 17));
  CHECK(apply_save(sim, loaded) == SaveError::None);
  CHECK(scene.entities()[std::size_t(idx_r)].active);
  CHECK(scene.entities()[std::size_t(idx_r)].anim == 1.f);
  CHECK(!scene.solid_tile(10, 17));

  // --- 2) elevator: estado inicial, desbloqueo por switch, viaje, pausa, llamada y respawn/reset ---
  { Input full; full.restart_full = true; sim.step(full); }
  CHECK(sim.mover_count() == 2);
  CHECK(sim.mover(0).is_elevator && sim.mover(1).is_elevator);
  CHECK(!scene.entities()[std::size_t(idx_el)].active);  // con link → arranca bloqueado
  CHECK(scene.entities()[std::size_t(idx_ef)].active);   // sin link → arranca desbloqueado
  CHECK(std::fabs(sim.mover(0).y - 72.f) < 1e-4f);

  // Parado sobre elev_locked bloqueado: no se mueve.
  place(sim, 204.f, 72.f - Sim::kPlayerRadius);
  for (int i = 0; i < 30; ++i) sim.step({});
  CHECK(std::fabs(sim.mover(0).y - 72.f) < 1e-4f);

  // Activar sw_elev desbloquea elev_locked.
  place(sim, 164.f, 68.f);
  sim.step({});
  (void)sim.take_sfx();
  { Input act; act.interact = true; sim.step(act); }
  CHECK((sim.take_sfx() & ew::audio::kSfxSwitch) != 0);
  CHECK(scene.entities()[std::size_t(idx_sw)].active);
  CHECK(scene.entities()[std::size_t(idx_el)].active);

  // Subirse arriba en wp[0] → baja hasta wp[1] (y = 152) cargando al player sin aplastarlo.
  place(sim, 204.f, 72.f - Sim::kPlayerRadius);
  sim.step({});
  CHECK(sim.ridden() == 0);
  bool arrived_bottom = false;
  int arrival_frame = -1;
  for (int i = 0; i < 120; ++i) {
    sim.step({});
    if (std::fabs(sim.mover(0).y - 152.f) < 0.05f) {
      arrived_bottom = true;
      arrival_frame = i;
      break;
    }
  }
  CHECK(arrived_bottom && arrival_frame > 40);
  CHECK(sim.deaths() == 0);
  CHECK(std::fabs(sim.player().pos.y - (152.f - Sim::kPlayerRadius)) < 1.2f);

  // Pausa abajo >= 0.65s antes de volver si el player sigue encima; bajarse lo deja estacionado abajo.
  for (int i = 0; i < 20; ++i) sim.step({});
  CHECK(std::fabs(sim.mover(0).y - 152.f) < 0.05f);
  place(sim, 232.f, 156.f);  // bajar del ascensor
  for (int i = 0; i < 40; ++i) sim.step({});
  CHECK(std::fabs(sim.mover(0).y - 152.f) < 0.05f);

  // Llamar desde arriba con interact vuelve el ascensor a wp[0].
  place(sim, 188.f, 68.f);
  (void)sim.take_sfx();
  { Input act; act.interact = true; sim.step(act); }
  CHECK((sim.take_sfx() & ew::audio::kSfxSwitch) != 0);
  bool returned_top = false;
  for (int i = 0; i < 120; ++i) {
    sim.step({});
    if (std::fabs(sim.mover(0).y - 72.f) < 0.05f) { returned_top = true; break; }
  }
  CHECK(returned_top);

  // Moverlo a wp[1] y hacer respawn(): vuelve a wp[0] pero sigue desbloqueado.
  place(sim, 204.f, 72.f - Sim::kPlayerRadius);
  for (int i = 0; i < 130; ++i) sim.step({});
  CHECK(sim.mover(0).y > 140.f);
  { Input rst; rst.restart = true; sim.step(rst); }
  CHECK(std::fabs(sim.mover(0).y - 72.f) < 1e-4f);
  CHECK(scene.entities()[std::size_t(idx_el)].active);

  // FrameData render check (sin GL): empuja marcadores y movers limpiamente.
  FrameData fv;
  build_world_frame(fv, scene, sim, 0.5f, false, false);
  CHECK(fv.mover_count == 2);
  CHECK(fv.marker_count > 0);

  // reset() total: elev_locked vuelve a bloquearse, elev_free sigue desbloqueado.
  { Input full; full.restart_full = true; sim.step(full); }
  CHECK(!scene.entities()[std::size_t(idx_el)].active);
  CHECK(scene.entities()[std::size_t(idx_ef)].active);
  CHECK(std::fabs(sim.mover(0).y - 72.f) < 1e-4f);

  std::puts("world_mechanisms_test: OK");
  return 0;
}
