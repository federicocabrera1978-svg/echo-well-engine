#include "writer.hpp"
#include "map/crc32.hpp"
#include "map/ewmap_format.hpp"
#include <cstring>
#include <map>
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
  std::map<std::string, uint32_t> ent_by_name;
  if (auto it = doc.lists.find("entities"); it != doc.lists.end())
    for (auto& e : it->second) {
      auto name = to_str(e, "name");
      if (!name.empty()) ent_by_name[name] = uint32_t(ents.size());
      ents.push_back({st.add(name), st.add(to_str(e, "type")), to_int(e, "x"), to_int(e, "y")});
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

  std::vector<std::pair<SectionType, std::vector<std::byte>>> secs;
  secs.emplace_back(SectionType::TILE, tile.b);
  secs.emplace_back(SectionType::ENT, record_section(ents));
  secs.emplace_back(SectionType::LINK, record_section(links));
  secs.emplace_back(SectionType::WATER, record_section(water));
  secs.emplace_back(SectionType::ROPE, record_section(ropes));
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
