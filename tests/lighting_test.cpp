// Test sin GL: sistema de iluminación multipunto por bioma y entidades (Etapa 15 Phase A).
//   lighting_test slice.ewmap level2.ewmap level3.ewmap world.ewmap
#include "game/lighting.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#define CHECK(c)                                                                       \
  do {                                                                                 \
    if (!(c)) {                                                                        \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                \
      return 1;                                                                        \
    }                                                                                  \
  } while (0)

using namespace ew::game;
using BT = ew::map::BackdropTheme;

namespace {

bool has_light_kind(const FrameData& v, PointLightKind kind) {
  for (int i = 0; i < v.point_light_count; ++i) {
    const int k = int(std::lround(v.point_light_pos[std::size_t(i) * 4 + 3]));
    if (k == int(kind)) return true;
  }
  return false;
}

bool check_lights_in_air_and_culled(const FrameData& v, Scene& scene) {
  const float cam_x = scene.cam_x();
  const float cam_y = scene.cam_y();
  for (int i = 0; i < v.point_light_count; ++i) {
    const std::size_t b = std::size_t(i) * 4;
    const float lx = v.point_light_pos[b + 0];
    const float ly = v.point_light_pos[b + 1];
    const float lr = v.point_light_pos[b + 2];
    if (lr <= 0.f) return false;
    if (lx < cam_x - 48.f || lx > cam_x + float(kInternalW) + 48.f) return false;
    if (ly < cam_y - 48.f || ly > cam_y + float(kInternalH) + 48.f) return false;
    if (scene.solid_px(lx, ly)) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) {
    std::fprintf(stderr, "uso: lighting_test slice.ewmap level2.ewmap level3.ewmap world.ewmap\n");
    return 2;
  }

  // --- 1) Oscuridad por bioma -------------------------------------------------
  CHECK(biome_darkness(BT::Vacio) == 0.f);
  CHECK(biome_darkness(BT::Abismo) >= 0.80f);
  CHECK(biome_darkness(BT::Guarida) >= 0.80f);
  CHECK(biome_darkness(BT::Cripta) >= 0.80f);
  CHECK(biome_darkness(BT::Pozo) >= 0.65f);
  CHECK(biome_darkness(BT::Cueva) >= 0.65f);
  CHECK(biome_darkness(BT::Raices) >= 0.65f);
  CHECK(biome_darkness(BT::Grieta) >= 0.65f);
  CHECK(biome_darkness(BT::Invertido) >= 0.65f);
  CHECK(biome_darkness(BT::Arena) >= 0.65f);
  CHECK(biome_darkness(BT::Cisterna) > 0.f && biome_darkness(BT::Cisterna) < 0.65f);
  CHECK(biome_darkness(BT::Santuario) > 0.f && biome_darkness(BT::Santuario) < 0.65f);

  // --- 2) world.ewmap: Zonas 1, 2 y 3 + farol del héroe -----------------------
  {
    Scene scene;
    std::string err;
    CHECK(scene.load(argv[4], err));
    Sim sim(scene);

    // Zona 1 (0,0): Pozo — farol adelantado según facing y gravedad.
    scene.follow(160.f, 92.f);
    Input in_right;
    in_right.move_x = 1.f;
    for (int i = 0; i < 4; ++i) sim.step(in_right);
    CHECK(sim.player_facing() == 1);

    FrameData v_right;
    collect_room_lights(v_right, scene, sim, 0.5f);
    CHECK(std::fabs(v_right.light_x - (sim.player().pos.x + 4.f)) < 1e-3f);
    CHECK(std::fabs(v_right.light_y - (sim.player().pos.y - 8.f * sim.down())) < 1e-3f);
    CHECK(std::fabs(v_right.ambient_darkness - biome_darkness(BT::Pozo)) < 1e-4f);
    CHECK(v_right.lantern_radius == 125.f);
    CHECK(v_right.point_light_count > 0 && v_right.point_light_count <= kMaxPointLights);
    CHECK(has_light_kind(v_right, PointLightKind::Torch));
    CHECK(check_lights_in_air_and_culled(v_right, scene));

    Input in_left;
    in_left.move_x = -1.f;
    for (int i = 0; i < 4; ++i) sim.step(in_left);
    CHECK(sim.player_facing() == -1);

    FrameData v_left;
    collect_room_lights(v_left, scene, sim, 0.6f);
    CHECK(std::fabs(v_left.light_x - (sim.player().pos.x - 4.f)) < 1e-3f);
    CHECK(v_left.light_x < sim.player().pos.x);

    // Gravedad invertida: down() == -1 → farol se desplaza +8 en Y.
    sim.set_gravity_flipped(true);
    CHECK(sim.down() < 0.f);
    FrameData v_flip;
    collect_room_lights(v_flip, scene, sim, 0.7f);
    CHECK(std::fabs(v_flip.light_y - (sim.player().pos.y + 8.f)) < 1e-3f);
    sim.set_gravity_flipped(false);

    // Zona 2 (6,1) Abismo y (7,1) Cripta: oscuridad profunda + antorchas y cristales.
    for (int cx : {6, 7}) {
      scene.follow(float(cx) * 320.f + 160.f, 184.f + 92.f);
      CHECK(scene.chunk_x() == cx && scene.chunk_y() == 1);
      FrameData vz2;
      collect_room_lights(vz2, scene, sim, 1.0f);
      CHECK(vz2.ambient_darkness >= 0.80f);
      CHECK(vz2.point_light_count > 0 && vz2.point_light_count <= kMaxPointLights);
      CHECK(has_light_kind(vz2, PointLightKind::Torch));
      CHECK(has_light_kind(vz2, PointLightKind::Crystal));
      CHECK(check_lights_in_air_and_culled(vz2, scene));
    }

    // Zona 3 (1,2) Santuario y (4,1) Guarida: antorchas/cristales y lava/brasas.
    scene.follow(1.f * 320.f + 160.f, 2.f * 184.f + 92.f);
    CHECK(scene.chunk_x() == 1 && scene.chunk_y() == 2);
    FrameData v_sanc;
    collect_room_lights(v_sanc, scene, sim, 1.2f);
    CHECK(std::fabs(v_sanc.ambient_darkness - biome_darkness(BT::Santuario)) < 1e-4f);
    CHECK(v_sanc.point_light_count > 0 && v_sanc.point_light_count <= kMaxPointLights);
    CHECK(has_light_kind(v_sanc, PointLightKind::Torch));
    CHECK(has_light_kind(v_sanc, PointLightKind::Crystal));
    CHECK(check_lights_in_air_and_culled(v_sanc, scene));

    scene.follow(4.f * 320.f + 160.f, 1.f * 184.f + 92.f);
    CHECK(scene.chunk_x() == 4 && scene.chunk_y() == 1);
    FrameData v_guar;
    collect_room_lights(v_guar, scene, sim, 1.4f);
    CHECK(std::fabs(v_guar.ambient_darkness - biome_darkness(BT::Guarida)) < 1e-4f);
    CHECK(v_guar.point_light_count > 0 && v_guar.point_light_count <= kMaxPointLights);
    CHECK(has_light_kind(v_guar, PointLightKind::Lava));
    CHECK(check_lights_in_air_and_culled(v_guar, scene));

    // Checkpoint inactivo (cian) vs activo (verde santuario).
    std::size_t ck_idx = scene.entities().size();
    for (std::size_t i = 0; i < scene.entities().size(); ++i) {
      if (scene.entities()[i].type == "checkpoint") { ck_idx = i; break; }
    }
    CHECK(ck_idx < scene.entities().size());
    const auto& ck_e = scene.entities()[ck_idx];
    scene.follow(ck_e.x, ck_e.y);
    FrameData v_ck_off;
    collect_room_lights(v_ck_off, scene, sim, 0.f);
    CHECK(has_light_kind(v_ck_off, PointLightKind::Shrine));
    int off_idx = -1;
    for (int i = 0; i < v_ck_off.point_light_count; ++i) {
      const std::size_t b = std::size_t(i) * 4;
      if (std::fabs(v_ck_off.point_light_pos[b] - ck_e.x) < 1.f &&
          std::fabs(v_ck_off.point_light_pos[b + 1] - (ck_e.y - 4.f)) < 1.f) {
        off_idx = i;
        break;
      }
    }
    CHECK(off_idx >= 0);
    CHECK(v_ck_off.point_light_col[std::size_t(off_idx) * 4 + 2] >
          v_ck_off.point_light_col[std::size_t(off_idx) * 4 + 1]);  // más azul/cian que verde

    scene.set_active(ck_idx, true);
    FrameData v_ck_on;
    collect_room_lights(v_ck_on, scene, sim, 0.f);
    int on_idx = -1;
    for (int i = 0; i < v_ck_on.point_light_count; ++i) {
      const std::size_t b = std::size_t(i) * 4;
      if (std::fabs(v_ck_on.point_light_pos[b] - ck_e.x) < 1.f &&
          std::fabs(v_ck_on.point_light_pos[b + 1] - (ck_e.y - 4.f)) < 1.f) {
        on_idx = i;
        break;
      }
    }
    CHECK(on_idx >= 0);
    CHECK(v_ck_on.point_light_col[std::size_t(on_idx) * 4 + 1] > 0.90f);  // verde brillante
  }

  // --- 3) slice.ewmap & level2.ewmap: integración con build_world_frame ---------
  for (int map_arg : {1, 2}) {
    Scene scene;
    std::string err;
    CHECK(scene.load(argv[map_arg], err));
    Sim sim(scene);
    FrameData v;
    build_world_frame(v, scene, sim, 0.5f, false, false);
    CHECK(v.point_light_count > 0 && v.point_light_count <= kMaxPointLights);
    CHECK(v.lantern_radius == 125.f);
    CHECK(check_lights_in_air_and_culled(v, scene));
  }

  // --- 4) level3.ewmap: bolas de fuego del Wyrm generan luz Lava dinámica -----
  {
    Scene scene;
    std::string err;
    CHECK(scene.load(argv[3], err));
    Sim sim(scene);
    const Entity* wyrm = scene.find_type("boss_dragon");
    CHECK(wyrm != nullptr);

    Input idle{};
    bool saw_fireballs = false;
    for (int step = 0; step < 300; ++step) {
      sim.step(idle);
      if (!sim.fireballs().empty()) { saw_fireballs = true; break; }
    }
    CHECK(saw_fireballs);
    const auto fb = sim.fireballs().front();
    scene.follow(fb.x, fb.y);

    FrameData v;
    build_world_frame(v, scene, sim, 2.5f, false, false);
    CHECK(v.point_light_count > 0 && v.point_light_count <= kMaxPointLights);
    bool found_fb_light = false;
    for (int i = 0; i < v.point_light_count; ++i) {
      const std::size_t b = std::size_t(i) * 4;
      const int kind = int(std::lround(v.point_light_pos[b + 3]));
      if (kind == int(PointLightKind::Lava) &&
          std::fabs(v.point_light_pos[b + 0] - fb.x) < 1e-2f &&
          std::fabs(v.point_light_pos[b + 1] - fb.y) < 1e-2f &&
          std::fabs(v.point_light_pos[b + 2] - 44.f) < 1e-2f) {
        found_fb_light = true;
        break;
      }
    }
    CHECK(found_fb_light);
  }

  std::printf("lighting_test OK\n");
  return 0;
}
