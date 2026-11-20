#include "game/save.hpp"

#include "game/sim.hpp"
#include "map/crc32.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace ew::game {
namespace {

void put_u8(std::vector<uint8_t>& b, uint8_t v) { b.push_back(v); }
void put_u16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); }
void put_u32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); }

// Lector con bounds: cualquier lectura fuera de rango deja ok = false y devuelve 0.
struct Reader {
  const uint8_t* p;
  std::size_t n, i = 0;
  bool ok = true;
  bool need(std::size_t k) { if (!ok || k > n - i) { ok = false; return false; } return true; }
  uint8_t u8() { if (!need(1)) return 0; return p[i++]; }
  uint16_t u16() { if (!need(2)) return 0; const uint16_t v = uint16_t(p[i] | (p[i + 1] << 8)); i += 2; return v; }
  uint32_t u32() {
    if (!need(4)) return 0;
    const uint32_t v = uint32_t(p[i]) | (uint32_t(p[i + 1]) << 8) | (uint32_t(p[i + 2]) << 16) | (uint32_t(p[i + 3]) << 24);
    i += 4;
    return v;
  }
};

uint32_t crc_of(const uint8_t* p, std::size_t n) {
  return ew::map::crc32(std::span<const std::byte>(reinterpret_cast<const std::byte*>(p), n));
}

}  // namespace

const char* save_error_str(SaveError e) {
  switch (e) {
    case SaveError::None: return "ok";
    case SaveError::TooSmall: return "archivo demasiado chico";
    case SaveError::TooBig: return "archivo demasiado grande";
    case SaveError::BadMagic: return "no es un save (magic)";
    case SaveError::BadVersion: return "versión no soportada";
    case SaveError::BadSize: return "tamaño inconsistente";
    case SaveError::BadCrc: return "CRC inválido";
    case SaveError::BadField: return "campo fuera de rango";
    case SaveError::Mismatch: return "el save es de otro mapa";
    case SaveError::Io: return "error de E/S";
  }
  return "?";
}

std::vector<uint8_t> encode_save(const SaveData& d) {
  std::vector<uint8_t> pl;
  const std::size_t ml = std::min(d.map.size(), kSaveMaxMapName);
  put_u8(pl, uint8_t(ml));
  pl.insert(pl.end(), d.map.begin(), d.map.begin() + std::ptrdiff_t(ml));
  put_u16(pl, uint16_t(int16_t(d.checkpoint)));
  put_u8(pl, uint8_t((d.flipped ? 1 : 0) | (d.muted ? 2 : 0)));
  float vol = std::isfinite(d.volume) ? std::fmin(1.f, std::fmax(0.f, d.volume)) : 0.8f;
  uint32_t vb;
  std::memcpy(&vb, &vol, 4);
  put_u32(pl, vb);
  const std::size_t ne = std::min(d.active.size(), kSaveMaxEntities);
  put_u16(pl, uint16_t(ne));
  std::vector<uint8_t> bits((ne + 7) / 8, 0);
  for (std::size_t k = 0; k < ne; ++k)
    if (d.active[k]) bits[k / 8] = uint8_t(bits[k / 8] | (1u << (k % 8)));
  pl.insert(pl.end(), bits.begin(), bits.end());

  std::vector<uint8_t> out = {'E', 'W', 'S', 'V'};
  put_u16(out, kSaveVersion);
  put_u16(out, 0);
  put_u32(out, uint32_t(pl.size()));
  out.insert(out.end(), pl.begin(), pl.end());
  put_u32(out, crc_of(out.data(), out.size()));
  return out;
}

SaveError decode_save(const uint8_t* data, std::size_t size, SaveData& out) {
  constexpr std::size_t kHdr = 12, kCrc = 4;
  if (!data || size < kHdr + kCrc) return SaveError::TooSmall;
  if (size > kSaveMaxBytes) return SaveError::TooBig;
  if (std::memcmp(data, "EWSV", 4) != 0) return SaveError::BadMagic;
  Reader h{data, size};
  h.i = 4;
  const uint16_t ver = h.u16();
  const uint16_t rsv = h.u16();
  const uint32_t psz = h.u32();
  if (ver != kSaveVersion) return SaveError::BadVersion;
  if (rsv != 0) return SaveError::BadField;
  if (psz != size - kHdr - kCrc) return SaveError::BadSize;
  Reader c{data, size};
  c.i = size - kCrc;
  if (c.u32() != crc_of(data, size - kCrc)) return SaveError::BadCrc;

  Reader r{data + kHdr, psz};
  SaveData d;
  const uint8_t ml = r.u8();
  if (!r.ok || ml == 0 || ml > kSaveMaxMapName || !r.need(ml)) return SaveError::BadField;
  for (uint8_t k = 0; k < ml; ++k) {
    const char ch = char(r.p[r.i + k]);
    if (ch < 0x20 || ch > 0x7e) return SaveError::BadField;
  }
  d.map.assign(reinterpret_cast<const char*>(r.p + r.i), ml);
  r.i += ml;
  d.checkpoint = int16_t(r.u16());
  const uint8_t flags = r.u8();
  const uint32_t vb = r.u32();
  const uint16_t ne = r.u16();
  if (!r.ok) return SaveError::BadSize;
  if (flags & ~3u) return SaveError::BadField;
  d.flipped = flags & 1;
  d.muted = flags & 2;
  std::memcpy(&d.volume, &vb, 4);
  if (!std::isfinite(d.volume) || std::signbit(d.volume) || d.volume > 1.f) return SaveError::BadField;
  if (ne > kSaveMaxEntities) return SaveError::BadField;
  if (d.checkpoint < -1 || d.checkpoint >= int(ne)) return SaveError::BadField;
  const std::size_t nb = (std::size_t(ne) + 7) / 8;
  if (!r.need(nb)) return SaveError::BadSize;
  d.active.assign(ne, 0);
  for (std::size_t k = 0; k < ne; ++k) d.active[k] = (r.p[r.i + k / 8] >> (k % 8)) & 1;
  // Bits de relleno del último byte en 0 (canónico).
  if (ne % 8 && (r.p[r.i + nb - 1] >> (ne % 8))) return SaveError::BadField;
  r.i += nb;
  if (r.i != psz) return SaveError::BadSize;  // sin bytes de más
  out = std::move(d);
  return SaveError::None;
}

SaveData capture_save(const Sim& sim) {
  SaveData d;
  auto& sc = const_cast<Sim&>(sim).scene();
  d.map = std::string(sc.map().string_at(1));
  d.checkpoint = sim.checkpoint_index();
  d.flipped = sim.checkpoint_flipped();
  d.active.reserve(sc.entities().size());
  for (const auto& e : sc.entities()) d.active.push_back(e.active ? 1 : 0);
  return d;
}

SaveError apply_save(Sim& sim, const SaveData& d) {
  auto& sc = sim.scene();
  const auto& ents = sc.entities();
  if (d.map != sc.map().string_at(1) || d.active.size() != ents.size()) return SaveError::Mismatch;
  if (d.checkpoint >= 0 && ents[std::size_t(d.checkpoint)].type != "checkpoint") return SaveError::Mismatch;
  sim.load_state(d.checkpoint, d.flipped, d.active);
  return SaveError::None;
}

SaveError write_save_file(const std::string& path, const SaveData& d) {
  const auto bytes = encode_save(d);
  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return SaveError::Io;
    f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!f) return SaveError::Io;
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);  // reemplaza el anterior (POSIX y MSVC)
  if (ec) { std::filesystem::remove(tmp, ec); return SaveError::Io; }
  return SaveError::None;
}

SaveError read_save_file(const std::string& path, SaveData& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return SaveError::Io;
  std::vector<uint8_t> buf(kSaveMaxBytes + 1);
  f.read(reinterpret_cast<char*>(buf.data()), std::streamsize(buf.size()));
  const std::size_t n = std::size_t(f.gcount());
  if (n > kSaveMaxBytes) return SaveError::TooBig;
  return decode_save(buf.data(), n, out);
}

namespace {
bool dir_writable(const std::filesystem::path& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (!std::filesystem::is_directory(dir, ec)) return false;
  const auto probe = dir / ".ew_write_test";
  {
    std::ofstream f(probe, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.put('x');
    if (!f) return false;
  }
  std::filesystem::remove(probe, ec);
  return true;
}
std::string env(const char* k) {
  const char* v = std::getenv(k);  // NOLINT(concurrency-mt-unsafe): solo al arrancar
  return v ? std::string(v) : std::string();
}
}  // namespace

std::string save_dir(const std::string& fallback_dir) {
  std::filesystem::path p;
  if (const auto o = env("EW_SAVE_DIR"); !o.empty()) {
    p = o;
  } else {
#if defined(_WIN32)
    if (const auto a = env("APPDATA"); !a.empty()) p = std::filesystem::path(a) / "EchoWell";
#elif defined(__APPLE__)
    if (const auto h = env("HOME"); !h.empty()) p = std::filesystem::path(h) / "Library" / "Application Support" / "EchoWell";
#else
    if (const auto x = env("XDG_DATA_HOME"); !x.empty()) p = std::filesystem::path(x) / "echo-well";
    else if (const auto h = env("HOME"); !h.empty()) p = std::filesystem::path(h) / ".local" / "share" / "echo-well";
#endif
  }
  if (!p.empty() && dir_writable(p)) return p.string();
  if (!fallback_dir.empty() && dir_writable(fallback_dir)) return fallback_dir;
  return {};
}

}  // namespace ew::game
