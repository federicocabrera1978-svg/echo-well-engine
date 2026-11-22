// Test sin GL: partida guardada (formato EWSV versionado, bounds + CRC, apply sobre slice).
#include "game/save.hpp"
#include "game/sim.hpp"
#include "map/crc32.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (línea %d)\n", #c, __LINE__); return 1; } } while (0)
using namespace ew;
using game::SaveData;
using game::SaveError;
using game::Sim;

static int idx(const game::Scene& s, const char* name) {
  const auto& e = s.entities();
  for (std::size_t i = 0; i < e.size(); ++i)
    if (e[i].name == name) return int(i);
  return -1;
}

// Re-sella el CRC después de tocar el payload (para probar validaciones de campos).
static void reseal(std::vector<uint8_t>& b) {
  const std::size_t n = b.size() - 4;
  const uint32_t c = map::crc32(std::span<const std::byte>(reinterpret_cast<const std::byte*>(b.data()), n));
  for (int i = 0; i < 4; ++i) b[n + std::size_t(i)] = uint8_t(c >> (8 * i));
}

static SaveError dec(const std::vector<uint8_t>& b, SaveData& out) { return game::decode_save(b.data(), b.size(), out); }

static void set_env(const char* k, const std::string& v) {
#ifdef _WIN32
  _putenv_s(k, v.c_str());
#else
  setenv(k, v.c_str(), 1);
#endif
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string err;
  game::Scene scene;
  if (!scene.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  Sim sim(scene);
  const int ck_mover = idx(scene, "ck_mover"), egg = idx(scene, "egg_well"), flood = idx(scene, "flood");
  const int sw_well = idx(scene, "switch_well"), door_hi = idx(scene, "door_well_hi");
  const int ck_grav = idx(scene, "ck_grav"), boss = idx(scene, "boss"), door_boss = idx(scene, "door_boss");
  CHECK(ck_mover >= 0 && egg >= 0 && flood >= 0 && sw_well >= 0 && door_hi >= 0 && ck_grav >= 0 && boss >= 0 && door_boss >= 0);

  // --- estado jugado: checkpoint de la sala 4, huevo, inundación, puertas del pozo ---
  {
    const auto& ck = scene.entities()[std::size_t(ck_mover)];
    auto& p = sim.world().points()[sim.player_index()];
    p.pos = p.prev = {ck.x, ck.y - 4.f};
    for (int i = 0; i < 5; ++i) sim.step({});
    CHECK(sim.checkpoint_index() == ck_mover);
    scene.set_active(std::size_t(egg), true);
    scene.set_switch(std::size_t(flood), true);
    scene.set_switch(std::size_t(sw_well), true);
    CHECK(sim.collected() == 1);
  }
  SaveData d = game::capture_save(sim);
  d.volume = 0.3f;
  d.muted = true;
  CHECK(d.map == "slice" && d.checkpoint == ck_mover && !d.flipped);
  CHECK(d.active.size() == scene.entities().size());
  const auto bytes = game::encode_save(d);
  CHECK(bytes.size() < game::kSaveMaxBytes);
  CHECK(std::memcmp(bytes.data(), "EWSV", 4) == 0);
  SaveData r;
  CHECK(dec(bytes, r) == SaveError::None);
  CHECK(r.map == d.map && r.checkpoint == d.checkpoint && r.flipped == d.flipped && r.muted && r.active == d.active);
  CHECK(std::fabs(r.volume - 0.3f) < 1e-6f);
  if (argc > 2) {  // save_test MAPA OUT: escribe un save válido (semilla del corpus de fuzz)
    std::ofstream f(argv[2], std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  }

  // --- apply sobre una sim nueva: aparece en el checkpoint con todo restaurado ---
  {
    game::Scene s2;
    CHECK(s2.load(argv[1], err));
    Sim b(s2);
    CHECK(b.collected() == 0 && b.checkpoint_index() == -1);
    CHECK(game::apply_save(b, r) == SaveError::None);
    CHECK(b.checkpoint_index() == ck_mover && b.collected() == 1);
    const auto& ck = s2.entities()[std::size_t(ck_mover)];
    CHECK(std::fabs(b.player().pos.x - ck.x) < 0.01f && std::fabs(b.player().pos.y - ck.y) < 0.01f);
    CHECK(s2.entities()[std::size_t(door_hi)].active && s2.entities()[std::size_t(door_hi)].anim == 1.f);
    bool flooded = false;
    for (const auto& w : b.water())
      if (w.x() >= 400.f) flooded = flooded || std::fabs(w.rest_y() - Sim::kFloodRestY) < 0.01f;
    CHECK(flooded);  // la pileta inundada queda arriba sin animar
    for (int i = 0; i < 120; ++i) b.step({});
    CHECK(b.deaths() == 0 && b.collected() == 1);  // estable en el checkpoint
    // R largo: vuelve a un spawn limpio (el save no "pega" estado).
    game::Input full; full.restart_full = true;
    b.step(full);
    CHECK(b.collected() == 0 && b.checkpoint_index() == -1);
  }

  // --- boss vencido + checkpoint con gravedad invertida ---
  {
    game::Scene s3;
    CHECK(s3.load(argv[1], err));
    Sim c(s3);
    SaveData g = r;
    g.checkpoint = ck_grav;
    g.flipped = true;
    g.active[std::size_t(boss)] = 1;
    g.active[std::size_t(door_boss)] = 1;
    SaveData g2;
    CHECK(dec(game::encode_save(g), g2) == SaveError::None && g2.flipped);
    CHECK(game::apply_save(c, g2) == SaveError::None);
    CHECK(c.bosses_defeated() && s3.entities()[std::size_t(door_boss)].active);
    CHECK(c.down() < 0.f && c.checkpoint_index() == ck_grav);
  }

  // --- validación contra el mapa: no toca la sim si no valida ---
  {
    game::Scene s4;
    CHECK(s4.load(argv[1], err));
    Sim m(s4);
    SaveData bad = r;
    bad.map = "otro";
    CHECK(game::apply_save(m, bad) == SaveError::Mismatch);
    bad = r;
    bad.active.pop_back();
    CHECK(game::apply_save(m, bad) == SaveError::Mismatch);
    bad = r;
    bad.checkpoint = egg;  // no es un checkpoint
    CHECK(game::apply_save(m, bad) == SaveError::Mismatch);
    bad = r;
    bad.abilities = 1u << 30;  // bit reservado / desconocido
    CHECK(game::apply_save(m, bad) == SaveError::BadField);
    CHECK(m.checkpoint_index() == -1 && m.collected() == 0);
  }

  // --- decoder: magic, versión, tamaños, CRC, campos ---
  {
    SaveData o;
    CHECK(game::decode_save(nullptr, 0, o) == SaveError::TooSmall);
    CHECK(game::decode_save(bytes.data(), 15, o) == SaveError::TooSmall);
    std::vector<uint8_t> big(game::kSaveMaxBytes + 1, 0);
    CHECK(dec(big, o) == SaveError::TooBig);
    auto b = bytes; b[0] = 'X';
    CHECK(dec(b, o) == SaveError::BadMagic);
    b = bytes; b[4] = 99; reseal(b);  // versión futura
    CHECK(dec(b, o) == SaveError::BadVersion);
    b = bytes; b[6] = 1; reseal(b);
    CHECK(dec(b, o) == SaveError::BadField);
    // Cualquier byte cambiado (o truncado) se rechaza; nunca se acepta basura.
    for (std::size_t i = 0; i < bytes.size(); ++i) {
      b = bytes; b[i] ^= 0x5a;
      CHECK(dec(b, o) != SaveError::None);
      b = bytes; b.resize(i);
      CHECK(dec(b, o) != SaveError::None);
    }
    b = bytes; b.push_back(0);
    CHECK(dec(b, o) != SaveError::None);
    // Campos fuera de rango con CRC válido.
    const std::size_t pl = 12, ml = bytes[pl];
    const std::size_t off_ck = pl + 1 + ml, off_flags = off_ck + 2, off_vol = off_flags + 1;
    b = bytes; b[off_flags] = 0x80; reseal(b);
    CHECK(dec(b, o) == SaveError::BadField);
    const float nan = std::nanf("");
    b = bytes; std::memcpy(&b[off_vol], &nan, 4); reseal(b);
    CHECK(dec(b, o) == SaveError::BadField);
    const float two = 2.f;
    b = bytes; std::memcpy(&b[off_vol], &two, 4); reseal(b);
    CHECK(dec(b, o) == SaveError::BadField);
    b = bytes; b[off_ck] = 0xff; b[off_ck + 1] = 0x7f; reseal(b);  // checkpoint ≥ entity_count
    CHECK(dec(b, o) == SaveError::BadField);
    b = bytes; b[pl] = 0; reseal(b);  // nombre vacío
    CHECK(dec(b, o) != SaveError::None);
    b = bytes; b[pl + 1] = 0x01; reseal(b);  // nombre no imprimible
    CHECK(dec(b, o) == SaveError::BadField);
    const std::size_t off_bits = off_vol + 4 + 2, nb = (r.active.size() + 7) / 8;
    if (r.active.size() % 8) {  // bits de relleno ≠ 0
      b = bytes; b[off_bits + nb - 1] |= 0x80; reseal(b);
      CHECK(dec(b, o) == SaveError::BadField);
    }
    // --- controles (v2) + habilidades (v3) + héroe (v4) ---
    const std::size_t off_bind = off_bits + nb;
    const std::size_t off_ab = off_bind + 1 + game::kBindCount * 6;
    const std::size_t off_hero = off_ab + 4;
    CHECK(bytes[off_bind] == game::kBindCount && bytes.size() == off_hero + 1 + 4);
    b = bytes; b[off_bind] = 3; reseal(b);
    CHECK(dec(b, o) == SaveError::BadField);
    b = bytes; b[off_bind + 1 + 6] = b[off_bind + 1]; b[off_bind + 1 + 7] = b[off_bind + 2]; reseal(b);  // tecla repetida
    CHECK(dec(b, o) == SaveError::BadField);
    b = bytes; b[off_bind + 1] = 0x00; b[off_bind + 2] = 0x01; reseal(b);  // 256 = Esc: reservada
    CHECK(dec(b, o) == SaveError::BadField);
    b = bytes; b[off_bind + 1 + 4] = 7; reseal(b);  // botón en IZQUIERDA (no remapeable en pad)
    CHECK(dec(b, o) == SaveError::BadField);
    b = bytes; b[off_ab + 3] = 0x80; reseal(b);  // bit de habilidad desconocido
    CHECK(dec(b, o) == SaveError::BadField);
    b = bytes; b[off_hero] = uint8_t(game::kHeroCount); reseal(b);  // héroe fuera de rango
    CHECK(dec(b, o) == SaveError::BadField);
    // Remapeo + habilidades + héroe → round-trip y apply_save (reset preserva héroe).
    SaveData cb = r;
    CHECK(cb.bind.bind_key(game::kBindJump, 'K') && cb.bind.bind_pad(game::kBindGrab, game::pad::kA));
    CHECK(cb.bind.valid());
    cb.abilities = game::kAbilityDoubleJump | game::kAbilityDash | game::kAbilityWallClimb;
    cb.hero = game::HeroKind::Beaver;
    CHECK(dec(game::encode_save(cb), o) == SaveError::None && o.bind == cb.bind &&
          !(o.bind == game::Bindings::defaults()) &&
          o.abilities == (game::kAbilityDoubleJump | game::kAbilityDash | game::kAbilityWallClimb) &&
          o.hero == game::HeroKind::Beaver);
    {
      game::Scene s4;
      CHECK(s4.load(argv[1], err));
      Sim ab_sim(s4);
      CHECK(game::apply_save(ab_sim, o) == SaveError::None);
      CHECK(ab_sim.has_ability(game::kAbilityDoubleJump) && ab_sim.air_jumps() == 1);
      CHECK(ab_sim.has_ability(game::kAbilityDash) && ab_sim.dash_ready());
      CHECK(ab_sim.has_ability(game::kAbilityWallClimb));
      CHECK(ab_sim.hero() == game::HeroKind::Beaver);
      game::Input full; full.restart_full = true;
      ab_sim.step(full);
      CHECK(!ab_sim.has_ability(game::kAbilityDoubleJump) && ab_sim.air_jumps() == 0);
      CHECK(!ab_sim.has_ability(game::kAbilityDash) && !ab_sim.dash_ready());
      CHECK(!ab_sim.has_ability(game::kAbilityWallClimb));
      CHECK(ab_sim.hero() == game::HeroKind::Beaver);
    }
    // v3 (sin héroe) se sigue leyendo: preserva controles y habilidades, hero = Lantern.
    b = game::encode_save(cb);
    b.erase(b.begin() + std::ptrdiff_t(off_hero), b.begin() + std::ptrdiff_t(off_hero + 1));
    b[4] = 3;
    const uint32_t psz3 = uint32_t(b.size() - 16);
    for (int i = 0; i < 4; ++i) b[8 + std::size_t(i)] = uint8_t(psz3 >> (8 * i));
    reseal(b);
    CHECK(dec(b, o) == SaveError::None && o.bind == cb.bind &&
          o.abilities == (game::kAbilityDoubleJump | game::kAbilityDash | game::kAbilityWallClimb) &&
          o.hero == game::HeroKind::Lantern && o.active == r.active);
    // v2 (sin habilidades ni héroe) se sigue leyendo: preserva controles, abilities = 0, hero = Lantern.
    b = game::encode_save(cb);
    b.erase(b.begin() + std::ptrdiff_t(off_ab), b.begin() + std::ptrdiff_t(off_hero + 1));
    b[4] = 2;
    const uint32_t psz2 = uint32_t(b.size() - 16);
    for (int i = 0; i < 4; ++i) b[8 + std::size_t(i)] = uint8_t(psz2 >> (8 * i));
    reseal(b);
    CHECK(dec(b, o) == SaveError::None && o.bind == cb.bind && o.abilities == 0 &&
          o.hero == game::HeroKind::Lantern && o.active == r.active);
    // v1 (sin controles, habilidades ni héroe) se sigue leyendo: controles por defecto, abilities = 0, hero = Lantern.
    b = game::encode_save(cb);
    b.erase(b.begin() + std::ptrdiff_t(off_bind), b.begin() + std::ptrdiff_t(off_hero + 1));
    b[4] = 1;
    const uint32_t psz1 = uint32_t(b.size() - 16);
    for (int i = 0; i < 4; ++i) b[8 + std::size_t(i)] = uint8_t(psz1 >> (8 * i));
    reseal(b);
    CHECK(dec(b, o) == SaveError::None && o.bind == game::Bindings::defaults() && o.abilities == 0 &&
          o.hero == game::HeroKind::Lantern && o.active == r.active);
    b[4] = 5; reseal(b);
    CHECK(dec(b, o) == SaveError::BadVersion);
    CHECK(std::string(game::save_error_str(SaveError::BadCrc)).size() > 0);
  }

  // --- archivo + directorio de datos del usuario ---
  {
    const auto tmp = std::filesystem::temp_directory_path() / "ew_save_test";
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    set_env("EW_SAVE_DIR", (tmp / "datos").string());
    const std::string dir = game::save_dir();
    CHECK(dir == (tmp / "datos").string() && std::filesystem::is_directory(dir));
    const std::string path = (std::filesystem::path(dir) / game::kSaveFileName).string();
    SaveData o;
    CHECK(game::read_save_file(path, o) == SaveError::Io);  // todavía no hay save
    CHECK(game::write_save_file(path, d) == SaveError::None);
    CHECK(!std::filesystem::exists(path + ".tmp"));
    CHECK(game::read_save_file(path, o) == SaveError::None && o.active == d.active && o.checkpoint == d.checkpoint);
    SaveData d2 = d; d2.volume = 0.9f;  // sobrescribe
    CHECK(game::write_save_file(path, d2) == SaveError::None);
    CHECK(game::read_save_file(path, o) == SaveError::None && std::fabs(o.volume - 0.9f) < 1e-6f);
    { std::ofstream f(path, std::ios::binary | std::ios::trunc); f << "basura"; }
    CHECK(game::read_save_file(path, o) != SaveError::None);
    std::filesystem::remove_all(tmp, ec);
  }
  std::puts("save ok");
  return 0;
}
