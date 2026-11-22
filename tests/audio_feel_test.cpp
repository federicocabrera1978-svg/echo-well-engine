// Test sin GL: música procedural por zona (music_zone_for) y pasos por superficie (Moss, Stone, Metal, Water).
#include "audio/sfx.hpp"
#include "game/music.hpp"
#include "game/sim.hpp"

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
using audio::MusicZone;
using game::Input;
using game::Scene;
using game::Sim;
using game::SurfaceKind;

namespace {

void place(Sim& s, float x, float y) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = p.prev = {x, y};
  p.force = {};
  p.grounded = false;
  s.scene().follow(x, y);
}

uint32_t walk_collect_sfx(Sim& s, float move_x, int steps) {
  (void)s.take_sfx();
  uint32_t acc = 0;
  Input in;
  in.move_x = move_x;
  for (int i = 0; i < steps; ++i) {
    s.step(in);
    acc |= s.take_sfx();
  }
  return acc;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "uso: audio_feel_test <world.ewmap> <mover.ewmap>\n");
    return 2;
  }

  Scene world_scene;
  std::string err;
  if (!world_scene.load(argv[1], err)) {
    std::fprintf(stderr, "load world error: %s\n", err.c_str());
    return 1;
  }
  Sim world_sim(world_scene);

  // --- 1) Zonas musicales en world.ewmap ------------------------------------
  // Spawn (0,0) Pozo -> Well
  for (int i = 0; i < 4; ++i) world_sim.step({});
  CHECK(game::music_zone_for(world_scene, world_sim) == MusicZone::Well);

  // Chunk (6,0) Invertido -> Catacombs (fuera del rango de boss_pozo)
  place(world_sim, 1964.f, 152.f);
  CHECK(game::music_zone_for(world_scene, world_sim) == MusicZone::Catacombs);

  // Chunk (3,1) Grieta -> WyrmLair (fuera del rango de bosses)
  place(world_sim, 1096.f, 336.f);
  CHECK(game::music_zone_for(world_scene, world_sim) == MusicZone::WyrmLair);

  // Arena de boss_pozo en (7,0): vivo -> Boss; tras derrotarlo -> WyrmLair (Arena)
  place(world_sim, 2360.f, 152.f);
  CHECK(game::music_zone_for(world_scene, world_sim) == MusicZone::Boss);
  for (int hit = 0; hit < world_sim.boss(0).max_hits() && !world_sim.boss(0).defeated; ++hit) {
    for (int i = 0; i < 300 && (world_sim.boss(0).hurt > 0.f || world_sim.boss(0).phase == 2); ++i) {
      place(world_sim, 2320.f, 152.f);
      world_sim.step({});
    }
    const auto& b = world_sim.boss(0);
    auto& pl = world_sim.world().points()[world_sim.player_index()];
    pl.pos = {b.x, b.y - b.height() - Sim::kPlayerRadius + 1.5f};
    pl.prev = {b.x, pl.pos.y - 2.f};
    world_sim.step({});
  }
  CHECK(world_sim.boss(0).defeated);
  place(world_sim, 2360.f, 152.f);
  CHECK(game::music_zone_for(world_scene, world_sim) == MusicZone::WyrmLair);

  // --- 2) Superficies y emisión de pasos en world.ewmap ---------------------
  // Moss (spawn en Pozo)
  world_sim.reset();
  for (int i = 0; i < 16; ++i) world_sim.step({});
  CHECK(world_sim.player().grounded);
  CHECK(world_sim.surface_under_feet() == SurfaceKind::Moss);
  const uint32_t moss_sfx = walk_collect_sfx(world_sim, 1.f, 24);
  CHECK((moss_sfx & audio::kSfxStepMoss) != 0);
  CHECK((moss_sfx & (audio::kSfxStepStone | audio::kSfxStepMetal | audio::kSfxStepWater)) == 0);
  // Quieto no emite pasos
  CHECK((walk_collect_sfx(world_sim, 0.f, 16) &
         (audio::kSfxStepStone | audio::kSfxStepMoss | audio::kSfxStepMetal | audio::kSfxStepWater)) == 0);

  // Stone (techo de piedra VIS=15 en chunk (6,0) Invertido, gravedad invertida)
  world_sim.set_gravity_flipped(true);
  place(world_sim, 1952.f, 28.f);
  for (int i = 0; i < 16; ++i) world_sim.step({});
  CHECK(world_sim.player().grounded);
  CHECK(world_sim.surface_under_feet() == SurfaceKind::Stone);
  const uint32_t stone_sfx = walk_collect_sfx(world_sim, 1.f, 24);
  CHECK((stone_sfx & audio::kSfxStepStone) != 0);
  CHECK((stone_sfx & (audio::kSfxStepMoss | audio::kSfxStepMetal | audio::kSfxStepWater)) == 0);
  world_sim.set_gravity_flipped(false);

  // Water (pileta inicial x=232..296, y=136..160 + vado poco profundo sobre y=160)
  place(world_sim, 244.f, 150.f);
  for (int i = 0; i < 4; ++i) world_sim.step({});
  CHECK(world_sim.player_in_water());
  CHECK(world_sim.surface_under_feet() == SurfaceKind::Water);
  world_sim.water().emplace_back(24.f, 158.f, 96.f, 4.f, 0.02f, 0.04f);
  place(world_sim, 36.f, 156.f);
  for (int i = 0; i < 16; ++i) world_sim.step({});
  CHECK(world_sim.player_in_water());
  CHECK(world_sim.player().grounded);
  CHECK(world_sim.surface_under_feet() == SurfaceKind::Water);
  const uint32_t water_sfx = walk_collect_sfx(world_sim, 1.f, 20);
  CHECK((water_sfx & audio::kSfxStepWater) != 0);
  CHECK((water_sfx & (audio::kSfxStepStone | audio::kSfxStepMoss | audio::kSfxStepMetal)) == 0);

  // Metal por tema Engranajes (chunk (5,2))
  place(world_sim, 1888.f, 456.f);
  for (int i = 0; i < 16; ++i) world_sim.step({});
  CHECK(world_sim.surface_under_feet() == SurfaceKind::Metal);

  // --- 3) Metal al caminar sobre un mover en mover.ewmap --------------------
  Scene mover_scene;
  if (!mover_scene.load(argv[2], err)) {
    std::fprintf(stderr, "load mover error: %s\n", err.c_str());
    return 1;
  }
  Sim mover_sim(mover_scene);
  place(mover_sim, 96.f + 12.f, 128.f - Sim::kPlayerRadius);
  for (int i = 0; i < 4; ++i) mover_sim.step({});
  CHECK(mover_sim.ridden() == 0);
  CHECK(mover_sim.player().grounded);
  CHECK(mover_sim.surface_under_feet() == SurfaceKind::Metal);
  const uint32_t metal_sfx = walk_collect_sfx(mover_sim, 0.5f, 12);
  CHECK((metal_sfx & audio::kSfxStepMetal) != 0);

  std::puts("audio_feel ok");
  return 0;
}
