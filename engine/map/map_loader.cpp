#include "map/map_loader.hpp"
#include "map/crc32.hpp"
#include <cstring>
#include <fstream>

namespace ew::map {

std::string_view to_string(MapError e) {
  switch (e) {
    case MapError::None: return "ok";
    case MapError::IoError: return "io error";
    case MapError::TooSmall: return "file too small";
    case MapError::BadMagic: return "bad magic (expected EWMP)";
    case MapError::UnsupportedVersion: return "unsupported version";
    case MapError::BadGeometry: return "unexpected tile/chunk geometry";
    case MapError::BadSectionTable: return "bad section table";
    case MapError::SectionOutOfBounds: return "section out of bounds";
    case MapError::Misaligned: return "section not 8-byte aligned";
    case MapError::CompressedUnsupported: return "compressed chunks not supported in v1";
    case MapError::CrcMismatch: return "crc mismatch";
  }
  return "unknown";
}

MapError MapLoader::load_file(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return MapError::IoError;
  const auto size = static_cast<std::size_t>(f.tellg());
  std::vector<std::byte> buf(size);
  f.seekg(0);
  if (!f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size)))
    return MapError::IoError;
  return load_bytes(std::move(buf));
}

MapError MapLoader::load_bytes(std::vector<std::byte> bytes) {
  bytes_ = std::move(bytes);
  MapError e = validate();
  if (e != MapError::None) bytes_.clear();
  return e;
}

MapError MapLoader::validate() {
  if (bytes_.size() < sizeof(EwMapHeader)) return MapError::TooSmall;
  const auto& h = header();
  if (std::memcmp(h.magic, kMagic, 4) != 0) return MapError::BadMagic;
  if (h.version_major != kVersionMajor) return MapError::UnsupportedVersion;
  if (h.tile_size != kTileSize || h.chunk_w != kChunkW || h.chunk_h != kChunkH)
    return MapError::BadGeometry;
  if (h.flags & kFlagCompressedChunks) return MapError::CompressedUnsupported;
  if (h.section_table_offset % kAlignment) return MapError::Misaligned;
  const uint64_t table_end = h.section_table_offset + uint64_t(h.section_count) * sizeof(SectionDesc);
  if (h.section_count == 0 || table_end > bytes_.size()) return MapError::BadSectionTable;

  const SectionDesc* crc_sec = nullptr;
  for (const auto& s : sections()) {
    if (s.offset % kAlignment) return MapError::Misaligned;
    if (s.offset + s.size > bytes_.size()) return MapError::SectionOutOfBounds;
    if (s.type == uint32_t(SectionType::CRC)) crc_sec = &s;
  }
  if (h.flags & kFlagCrc) {
    if (!crc_sec || crc_sec->size < 4) return MapError::CrcMismatch;
    uint32_t stored;
    std::memcpy(&stored, bytes_.data() + crc_sec->offset, 4);
    // CRC cubre todo el archivo previo a la sección CRC.
    if (crc32({bytes_.data(), static_cast<std::size_t>(crc_sec->offset)}) != stored)
      return MapError::CrcMismatch;
  }
  return MapError::None;
}

const EwMapHeader& MapLoader::header() const {
  return *reinterpret_cast<const EwMapHeader*>(bytes_.data());
}

std::span<const SectionDesc> MapLoader::sections() const {
  const auto& h = header();
  return {reinterpret_cast<const SectionDesc*>(bytes_.data() + h.section_table_offset), h.section_count};
}

std::span<const std::byte> MapLoader::section(SectionType type) const {
  for (const auto& s : sections())
    if (s.type == uint32_t(type)) return {bytes_.data() + s.offset, static_cast<std::size_t>(s.size)};
  return {};
}

// Secciones de registros: u32 count, u32 pad, luego count × T
template <class T> std::span<const T> MapLoader::records(SectionType t) const {
  auto s = section(t);
  if (s.size() < 8) return {};
  uint32_t n;
  std::memcpy(&n, s.data(), 4);
  if (8 + std::size_t(n) * sizeof(T) > s.size()) return {};
  return {reinterpret_cast<const T*>(s.data() + 8), n};
}

std::span<const EntityRecord> MapLoader::entities() const { return records<EntityRecord>(SectionType::ENT); }
std::span<const LinkRecord>   MapLoader::links()    const { return records<LinkRecord>(SectionType::LINK); }
std::span<const WaterRecord>  MapLoader::water()    const { return records<WaterRecord>(SectionType::WATER); }

std::string_view MapLoader::string_at(uint32_t offset) const {
  auto s = section(SectionType::STRTAB);
  if (offset >= s.size()) return {};
  const char* p = reinterpret_cast<const char*>(s.data() + offset);
  return {p, strnlen(p, s.size() - offset)};
}

uint32_t MapLoader::chunk_count() const {
  auto s = section(SectionType::TILE);
  if (s.size() < 8) return 0;
  uint32_t n;
  std::memcpy(&n, s.data(), 4);
  return n;
}

std::optional<std::span<const uint16_t>>
MapLoader::chunk_layer(uint16_t cx, uint16_t cy, Layer layer) const {
  auto s = section(SectionType::TILE);
  const uint32_t n = chunk_count();
  if (8 + std::size_t(n) * kChunkBytes > s.size()) return std::nullopt;
  for (uint32_t i = 0; i < n; ++i) {
    const std::byte* base = s.data() + 8 + std::size_t(i) * kChunkBytes;
    ChunkHeader ch;
    std::memcpy(&ch, base, sizeof ch);
    if (ch.cx == cx && ch.cy == cy) {
      auto* tiles = reinterpret_cast<const uint16_t*>(base + sizeof(ChunkHeader));
      return std::span<const uint16_t>(tiles + uint32_t(layer) * kTilesPerChunk, kTilesPerChunk);
    }
  }
  return std::nullopt;
}

} // namespace ew::map
