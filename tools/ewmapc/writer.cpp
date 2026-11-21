#include "writer.hpp"
#include "map/crc32.hpp"
#include "map/ewmap_format.hpp"
#include "map/autotile.hpp"
#include "map/backdrop.hpp"
#include <set>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace ew::map;

namespace ewmapc {
namespace {
class Buf {
public:
  template <class T> void put(const T& v) {
    auto p = reinterpret_cast<const std::byte*>(&v);
    b.insert(b.end(), p, p + sizeof(T));
  }
  void put_bytes(const void* p, std::size_t n) {
    auto q = static_cast<const std::byte*>(p);
    b.insert(b.end(), q, q + n);
  }
  void pad() { b.resize(align_up(b.size())); }
  std::vector<std::byte> b;
};

struct StrTab {
  std::vector<char> data{'\0'}; // offset 0 = ""
  std::map<std::string, uint32_t> idx;
  uint32_t add(const std::string& s) {
    if (s.empty()) return 0;
    if (auto it = idx.find(s); it != idx.end()) return it->second;
    uint32_t off = uint32_t(data.size());
    data.insert(data.end(), s.begin(), s.end());
    data.push_back('\0');
    return idx[s] = off;
  }
};

int to_int(const Dict& d, const char* k, int def = 0) {
  auto it = d.find(k);
  return it == d.end() ? def : std::stoi(it->second);
}
float to_float(const Dict& d, const char* k, float def) {
  auto it = d.find(k);
  return it == d.end() ? def : std::stof(it->second);
}
std::string to_str(const Dict& d, const char* k) {
  auto it = d.find(k);
  return it == d.end() ? std::string{} : it->second;
}
bool layer_from(const std::string& s, uint32_t& out) {
  static const std::map<std::string, uint32_t> m{{"COL", 0}, {"VIS", 1}, {"FG", 2}, {"META", 3}};
  auto it = m.find(s);
  if (it == m.end()) return false;
  out = it->second;
  return true;
}
bool parse_pair(const std::string& s, int& a, int& b) {
  return std::sscanf(s.c_str(), " [ %d , %d ]", &a, &b) == 2;
}
template <class T> std::vector<std::byte> record_section(const std::vector<T>& v) {
  Buf s;
  s.put(uint32_t(v.size()));
  s.put(uint32_t(0));
  for (auto& r : v) s.put(r);
  return s.b;
}
} // namespace

bool build_ewmap(const YamlDoc& doc, const WriteOptions& opt, std::vector<std::byte>& out, std::string& err) try {
  int w = 1, h = 1;
  if (auto it = doc.scalars.find("size_chunks"); it != doc.scalars.end() && !parse_pair(it->second, w, h)) {
    err = "size_chunks must be [w, h]";
    return false;
  }
  if (w < 1 || h < 1 || w > 0xFFFF || h > 0xFFFF) { err = "size_chunks out of range"; return false; }

  // --- tiles: grilla global por capa ----------------------------------------
  const int tw = w * kChunkW, th = h * kChunkH;
  std::vector<std::vector<uint16_t>> grid(kLayerCount, std::vector<uint16_t>(std::size_t(tw) * th, 0));
  if (auto it = doc.maps.find("fill"); it != doc.maps.end())
    for (auto& [ln, val] : it->second) {
      uint32_t l;
      if (!layer_from(ln, l)) { err = "unknown layer in fill: " + ln; return false; }
      std::fill(grid[l].begin(), grid[l].end(), uint16_t(std::stoi(val)));
    }
  if (auto it = doc.lists.find("rects"); it != doc.lists.end())
    for (auto& r : it->second) {
      uint32_t l;
      if (!layer_from(to_str(r, "layer"), l)) { err = "rect with unknown layer"; return false; }
      int x0 = to_int(r, "x"), y0 = to_int(r, "y"), rw = to_int(r, "w", 1), rh = to_int(r, "h", 1);
      auto tile = uint16_t(to_int(r, "tile", 1));
      for (int y = std::max(0, y0); y < std::min(th, y0 + rh); ++y)
        for (int x = std::max(0, x0); x < std::min(tw, x0 + rw); ++x)
          grid[l][std::size_t(y) * tw + x] = tile;
    }

  // Autotiling 4 vecinos en VIS y FG (el YAML puede desactivarlo con autotile: false).
  bool do_at = true;
  if (auto it = doc.scalars.find("autotile"); it != doc.scalars.end()) {
    const auto& v = it->second;
    do_at = !(v == "false" || v == "0" || v == "no" || v == "off");
  }
  if (do_at) {
    apply_autotile4(grid[uint32_t(Layer::VIS)].data(), tw, th);
    apply_autotile4(grid[uint32_t(Layer::FG)].data(), tw, th);
  }

  Buf tile;
  tile.put(uint32_t(w * h));
  tile.put(uint32_t(0));
  for (int cy = 0; cy < h; ++cy)
    for (int cx = 0; cx < w; ++cx) {
      tile.put(ChunkHeader{uint16_t(cx), uint16_t(cy), kLayerCount});
      for (uint32_t l = 0; l < kLayerCount; ++l)
        for (int y = 0; y < kChunkH; ++y)
          tile.put_bytes(&grid[l][std::size_t(cy * kChunkH + y) * tw + cx * kChunkW], kChunkW * sizeof(uint16_t));
    }

  // --- entidades / links / agua -------------------------------------------
  StrTab st;
  st.add(doc.scalars.count("name") ? doc.scalars.at("name") : "untitled"); // offset 1 = nombre del mapa
  std::vector<EntityRecord> ents;
  std::vector<MoverRecord> movers;
  std::map<std::string, uint32_t> ent_by_name;
  if (auto it = doc.lists.find("entities"); it != doc.lists.end())
    for (auto& e : it->second) {
      auto name = to_str(e, "name");
      const auto type = to_str(e, "type");
      const int ex = to_int(e, "x"), ey = to_int(e, "y");
      if (!name.empty()) ent_by_name[name] = uint32_t(ents.size());
      ents.push_back({st.add(name), st.add(type), ex, ey});
      if (type != "mover") continue;
      MoverRecord m{};
      m.entity = uint32_t(ents.size() - 1);
      const int mw = to_int(e, "w"), mh = to_int(e, "h");
      if (mw < 1 || mh < 1 || mw > int(kMaxMoverTiles) || mh > int(kMaxMoverTiles)) {
        err = "mover needs w,h in [1,64] tiles"; return false;
      }
      m.w_tiles = uint16_t(mw);
      m.h_tiles = uint16_t(mh);
      m.speed = to_float(e, "speed", 40.f);
      m.pause = to_float(e, "pause", 0.5f);
      if (!(m.speed > 0.f) || m.speed > 1000.f) { err = "mover speed must be in (0, 1000]"; return false; }
      if (m.pause < 0.f || m.pause > 60.f) { err = "mover pause must be in [0, 60]"; return false; }
      const int tile = to_int(e, "tile", 6);
      if (tile < 0 || tile > 65535) { err = "mover tile out of range"; return false; }
      m.tile = uint16_t(tile);
      std::vector<std::pair<int, int>> wp{{ex, ey}};
      const auto path = to_str(e, "path");
      if (!path.empty()) {
        std::stringstream ss(path);
        std::vector<int> nums;
        for (int v; ss >> v;) nums.push_back(v);
        if (nums.size() < 2 || nums.size() % 2) { err = "mover path needs an even list of x y ..."; return false; }
        for (std::size_t i = 0; i < nums.size(); i += 2) wp.push_back({nums[i], nums[i + 1]});
      } else if (e.count("to_x") || e.count("to_y")) {
        wp.push_back({to_int(e, "to_x", ex), to_int(e, "to_y", ey)});
      }
      if (wp.size() < 2 || wp.size() > kMaxMoverWaypoints) {
        err = "mover needs 2..8 waypoints (to_x/to_y or path)"; return false;
      }
      m.n_wp = uint16_t(wp.size());
      for (std::size_t i = 0; i < wp.size(); ++i) {
        m.wp_x[i] = wp[i].first;
        m.wp_y[i] = wp[i].second;
      }
      movers.push_back(m);
    }
  std::vector<LinkRecord> links;
  if (auto it = doc.lists.find("links"); it != doc.lists.end())
    for (auto& l : it->second) {
      auto f = ent_by_name.find(to_str(l, "from")), t = ent_by_name.find(to_str(l, "to"));
      if (f == ent_by_name.end() || t == ent_by_name.end()) { err = "link references unknown entity"; return false; }
      links.push_back({f->second, t->second});
    }
  std::vector<WaterRecord> water;
  if (auto it = doc.lists.find("water"); it != doc.lists.end())
    for (auto& v : it->second)
      water.push_back({to_int(v, "x"), to_int(v, "y"), to_int(v, "w"), to_int(v, "h"),
                       to_float(v, "stiffness", 0.02f), to_float(v, "damping", 0.04f)});

  std::vector<RopeRecord> ropes;
  if (auto it = doc.lists.find("ropes"); it != doc.lists.end())
    for (auto& v : it->second) {
      const int n = to_int(v, "segments");
      const float len = to_float(v, "seg_len", 6.f);
      if (n < 1 || n > 256 || !(len > 0)) { err = "rope needs segments in [1,256] and seg_len > 0"; return false; }
      ropes.push_back({to_int(v, "x"), to_int(v, "y"), uint32_t(n), len});
    }

  // Fondos por chunk (opcional): solo se escribe la sección si el YAML trae `backdrops`,
  // así los mapas sin fondos quedan byte a byte iguales que antes.
  std::vector<BackdropRecord> backdrops;
  const bool has_backdrops = doc.lists.count("backdrops") > 0;
  if (has_backdrops) {
    std::set<std::pair<int, int>> seen;
    for (auto& v : doc.lists.at("backdrops")) {
      const int cx = to_int(v, "cx", -1), cy = to_int(v, "cy", 0);
      if (cx < 0 || cy < 0 || cx >= w || cy >= h) { err = "backdrop cx/cy outside size_chunks"; return false; }
      if (!seen.insert({cx, cy}).second) { err = "duplicate backdrop for chunk"; return false; }
      const auto name = to_str(v, "theme");
      const int theme = backdrop_from_name(name);
      if (theme < 0) {
        err = "unknown backdrop theme '" + name + "' (valid:";
        for (auto n : kBackdropNames) err += " " + std::string(n);
        err += ")";
        return false;
      }
      const int seed = to_int(v, "seed", cx * 7 + cy * 13);
      if (seed < 0 || seed > 65535) { err = "backdrop seed out of range [0, 65535]"; return false; }
      backdrops.push_back({uint16_t(cx), uint16_t(cy), uint16_t(theme), uint16_t(seed)});
    }
  }

  std::vector<std::pair<SectionType, std::vector<std::byte>>> secs;
  secs.emplace_back(SectionType::TILE, tile.b);
  secs.emplace_back(SectionType::ENT, record_section(ents));
  secs.emplace_back(SectionType::LINK, record_section(links));
  secs.emplace_back(SectionType::WATER, record_section(water));
  secs.emplace_back(SectionType::ROPE, record_section(ropes));
  secs.emplace_back(SectionType::MOVER, record_section(movers));
  if (has_backdrops) secs.emplace_back(SectionType::BACKDROP, record_section(backdrops));
  {
    std::vector<std::byte> s(st.data.size());
    std::memcpy(s.data(), st.data.data(), s.size());
    secs.emplace_back(SectionType::STRTAB, std::move(s));
  }
  const uint32_t count = uint32_t(secs.size()) + (opt.crc ? 1 : 0);

  // --- layout: header | tabla de secciones | payloads | [CRC] -------------
  Buf f;
  EwMapHeader hdr{};
  std::memcpy(hdr.magic, kMagic, 4);
  hdr.version_major = kVersionMajor;
  hdr.version_minor = kVersionMinor;
  hdr.flags = opt.crc ? uint32_t(kFlagCrc) : 0u;
  hdr.section_count = count;
  hdr.tile_size = kTileSize;
  hdr.chunk_w = kChunkW;
  hdr.chunk_h = kChunkH;
  hdr.map_w_chunks = uint16_t(w);
  hdr.map_h_chunks = uint16_t(h);
  hdr.section_table_offset = sizeof(EwMapHeader);
  f.put(hdr);
  const std::size_t table_at = f.b.size();
  f.b.resize(table_at + count * sizeof(SectionDesc));
  f.pad();

  std::vector<SectionDesc> table;
  for (auto& [type, data] : secs) {
    table.push_back({uint32_t(type), 0, f.b.size(), data.size()});
    f.put_bytes(data.data(), data.size());
    f.pad();
  }
  if (opt.crc) table.push_back({uint32_t(SectionType::CRC), 0, f.b.size(), 4});
  std::memcpy(f.b.data() + table_at, table.data(), table.size() * sizeof(SectionDesc));
  if (opt.crc) {
    f.put(crc32(f.b)); // cubre [0, offset de CRC)
    f.pad();
  }
  out = std::move(f.b);
  return true;
} catch (const std::exception& e) {
  err = std::string("invalid number: ") + e.what();
  return false;
}
} // namespace ewmapc
