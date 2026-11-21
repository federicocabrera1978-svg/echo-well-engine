// Test sin GL: atlas de sprites pixel-art 128×128 (8×8 celdas de 16×16 px), carga PNG con
// fallback procedural y máquina de estados de animación del protagonista (Sim::player_anim).
#include "game/sim.hpp"
#include "render/sprite_atlas.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

using namespace ew;
using ew::game::Input;
using ew::game::PlayerAnim;
using ew::game::Sim;
using namespace ew::render;

static std::vector<uint8_t> extract_cell(const std::vector<uint8_t>& px, int col, int row) {
  std::vector<uint8_t> out;
  out.reserve(std::size_t(kSpriteCellPx) * kSpriteCellPx * 4);
  const auto uv = sprite_uv(col, row);
  for (int y = 0; y < kSpriteCellPx; ++y) {
    const std::size_t row_off = (std::size_t(uv.y + y) * kSpriteAtlasPx + std::size_t(uv.x)) * 4;
    out.insert(out.end(), px.data() + row_off, px.data() + row_off + std::size_t(kSpriteCellPx) * 4);
  }
  return out;
}

static bool verify_atlas_cells(const std::vector<uint8_t>& px) {
  if (px.size() != std::size_t(kSpriteAtlasPx) * kSpriteAtlasPx * 4) return false;
  auto check_group = [&](int row, int col_begin, int col_end) -> bool {
    std::set<std::vector<uint8_t>> uniq;
    for (int col = col_begin; col < col_end; ++col) {
      const auto cell = extract_cell(px, col, row);
      auto alpha_at = [&](int x, int y) -> uint8_t {
        return cell[(std::size_t(y) * kSpriteCellPx + std::size_t(x)) * 4 + 3];
      };
      // Esquinas transparentes para dejar margen limpio de normales/contorno.
      if (alpha_at(0, 0) != 0 || alpha_at(kSpriteCellPx - 1, 0) != 0 ||
          alpha_at(0, kSpriteCellPx - 1) != 0 || alpha_at(kSpriteCellPx - 1, kSpriteCellPx - 1) != 0)
        return false;
      int opaque = 0;
      for (int y = 0; y < kSpriteCellPx; ++y)
        for (int x = 0; x < kSpriteCellPx; ++x)
          if (alpha_at(x, y) > 0) ++opaque;
      if (opaque < 20) return false;
      uniq.insert(cell);
    }
    return int(uniq.size()) == (col_end - col_begin);
  };

  for (int row = 0; row <= int(SpriteRow::Swim); ++row)
    if (!check_group(row, 0, 4)) return false;
  if (!check_group(int(SpriteRow::Items), 0, 4)) return false;  // huevo de dragón
  if (!check_group(int(SpriteRow::Items), 4, 8)) return false;  // reliquia alada
  if (!check_group(int(SpriteRow::Npc), 0, 4)) return false;    // NPC Vigía / Cronista
  if (!check_group(int(SpriteRow::Npc), 4, 8)) return false;    // Estela dracónica
  return true;
}

static void place(Sim& s, float x, float y) {
  auto& p = s.world().points()[s.player_index()];
  p.pos = p.prev = {x, y};
  p.force = {};
  p.grounded = false;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;

  // --- 1) Atlas procedural determinístico y layout 8×8 de 16×16 ---
  const auto a = build_sprite_atlas();
  const auto b = build_sprite_atlas();
  CHECK(a.size() == std::size_t(kSpriteAtlasPx) * kSpriteAtlasPx * 4);
  CHECK(std::memcmp(a.data(), b.data(), a.size()) == 0);
  CHECK(verify_atlas_cells(a));

  // --- 2) Carga desde archivo / memoria con fallback procedural ---
  {
    const auto f0 = load_sprite_atlas_file(nullptr);
    const auto f1 = load_sprite_atlas_file("");
    const auto f2 = load_sprite_atlas_file("/no/existe/sprites.png");
    CHECK(f0.source == AtlasSource::Procedural && f0.px == a);
    CHECK(f1.source == AtlasSource::Procedural && f1.px == a);
    CHECK(f2.source == AtlasSource::Procedural && f2.px == a);

    const unsigned char junk[] = {1, 2, 3, 4, 0x89, 'P', 'N', 'G', 0, 0};
    const auto m0 = load_sprite_atlas_mem(nullptr, 0);
    const auto m1 = load_sprite_atlas_mem(junk, sizeof(junk));
    CHECK(m0.source == AtlasSource::Procedural && m0.px == a);
    CHECK(m1.source == AtlasSource::Procedural && m1.px == a);
  }

  if (const char* dir = std::getenv("EW_TEST_FIXTURES"); dir && *dir) {
    const auto bad = load_sprite_atlas_file((std::string(dir) + "/atlas_16.png").c_str());
    CHECK(bad.source == AtlasSource::Procedural && bad.px == a);
  }

  if (const char* def = std::getenv("EW_DEFAULT_SPRITES"); def && *def) {
    const auto png = load_sprite_atlas_file(def);
    CHECK(png.source == AtlasSource::Png);
    CHECK(verify_atlas_cells(png.px));
    std::ifstream f(def, std::ios::binary);
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto mem = load_sprite_atlas_mem(buf.data(), buf.size());
    CHECK(mem.source == AtlasSource::Png && mem.px == png.px);
    std::printf("sprites PNG %s OK (%zu bytes PNG -> %zu bytes RGBA)\n", def, buf.size(), png.px.size());
  }

  // --- 3) Máquina de estados de animación del protagonista (sin GL) ---
  game::Scene scene;
  std::string err;
  CHECK(scene.load(argv[1], err));
  Sim sim(scene);

  // En reposo sobre el piso → Idle, facing +1, el frame avanza cada 16 pasos.
  for (int i = 0; i < 16; ++i) sim.step({});
  CHECK(sim.player().grounded);
  CHECK(sim.player_anim() == PlayerAnim::Idle);
  CHECK(sim.player_facing() == 1);
  const int f0 = sim.player_anim_frame();
  for (int i = 0; i < 16; ++i) sim.step({});
  const int f1 = sim.player_anim_frame();
  CHECK(sim.player_anim() == PlayerAnim::Idle);
  CHECK(f1 != f0);

  // Caminar a la izquierda y derecha → Walk, facing -1/+1, recorre frames 0..3.
  unsigned mask_walk = 0;
  for (int i = 0; i < 28; ++i) {
    sim.step({-1.f});
    if (sim.player_anim() == PlayerAnim::Walk) mask_walk |= 1u << unsigned(sim.player_anim_frame());
  }
  CHECK(sim.player_facing() == -1);
  CHECK(sim.player_anim() == PlayerAnim::Walk);
  CHECK(mask_walk == 0xFu);

  for (int i = 0; i < 12; ++i) sim.step({1.f});
  CHECK(sim.player_facing() == 1);
  CHECK(sim.player_anim() == PlayerAnim::Walk);

  // Esperar que frene de regreso a Idle.
  for (int i = 0; i < 20; ++i) sim.step({});
  CHECK(sim.player_anim() == PlayerAnim::Idle);

  // Salto normal: sube en Jump (frame 0..1) y cae en Jump (frame 2..3).
  { Input j; j.jump = true; sim.step(j); }
  CHECK(sim.player_anim() == PlayerAnim::Jump);
  CHECK(sim.player_anim_frame() == 0 || sim.player_anim_frame() == 1);
  bool saw_fall = false;
  for (int i = 0; i < 60 && !sim.player().grounded; ++i) {
    sim.step({});
    if (sim.player().velocity().y > 0.2f) {
      CHECK(sim.player_anim() == PlayerAnim::Jump);
      if (sim.player_anim_frame() >= 2) saw_fall = true;
    }
  }
  CHECK(saw_fall);
  CHECK(sim.player().grounded);

  // Agarrar reliquia de Doble Salto y ejecutar segundo salto en el aire → DoubleJump.
  for (int i = 0; i < 120 && !sim.has_ability(game::kAbilityDoubleJump); ++i) sim.step({1.f});
  CHECK(sim.has_ability(game::kAbilityDoubleJump));
  for (int i = 0; i < 10; ++i) sim.step({});
  { Input j; j.jump = true; sim.step(j); }
  for (int i = 0; i < 14; ++i) sim.step({});
  { Input j; j.jump = true; sim.step(j); }
  CHECK(sim.double_jump_flash() > 0);
  CHECK(sim.player_anim() == PlayerAnim::DoubleJump);
  unsigned mask_dj = 1u << unsigned(sim.player_anim_frame());
  for (int i = 0; i < 12; ++i) {
    sim.step({});
    if (sim.player_anim() == PlayerAnim::DoubleJump) mask_dj |= 1u << unsigned(sim.player_anim_frame());
  }
  CHECK((mask_dj & 0xFu) != 0x1u);  // avanza cuadro durante el flash aéreo

  // Colgado en la cuerda y trepando → Climb y avance de cuadros al trepar.
  for (int i = 0; i < 40; ++i) sim.step({});
  const auto tip = sim.world().points()[sim.rope_first() + uint32_t(sim.rope_points() - 1)].pos;
  place(sim, tip.x, tip.y);
  { Input g; g.grab = true; sim.step(g); }
  CHECK(sim.on_rope());
  CHECK(sim.player_anim() == PlayerAnim::Climb);
  const int c0 = sim.player_anim_frame();
  bool climb_changed = false;
  for (int i = 0; i < 16; ++i) {
    Input up; up.climb = -1.f;
    sim.step(up);
    CHECK(sim.player_anim() == PlayerAnim::Climb);
    if (sim.player_anim_frame() != c0) climb_changed = true;
  }
  CHECK(climb_changed);

  std::puts("sprite_test: OK");
  return 0;
}
