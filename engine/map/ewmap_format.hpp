// Formato binario .ewmap v1 — ver docs/ewmap.md
// Little-endian, todas las secciones alineadas a 8 bytes.
#pragma once
#include <cstddef>
#include <cstdint>

namespace ew::map {

inline constexpr char     kMagic[4]       = {'E', 'W', 'M', 'P'};
inline constexpr uint16_t kVersionMajor   = 1;
inline constexpr uint16_t kVersionMinor   = 0;
inline constexpr uint32_t kAlignment      = 8;
inline constexpr uint16_t kTileSize       = 8;   // px
inline constexpr uint16_t kChunkW         = 40;  // tiles
inline constexpr uint16_t kChunkH         = 23;  // tiles
inline constexpr uint32_t kTilesPerChunk  = kChunkW * kChunkH; // 920

enum class Layer : uint32_t { COL = 0, VIS = 1, FG = 2, META = 3 };
inline constexpr uint32_t kLayerCount = 4;

enum class SectionType : uint32_t {
  TILE = 1, ENT = 2, LINK = 3, WATER = 4, STRTAB = 5, CRC = 6,
};

enum HeaderFlags : uint32_t {
  kFlagCrc              = 1u << 0, // hay sección CRC (en CI: siempre)
  kFlagCompressedChunks = 1u << 1, // RESERVADO: v1 siempre raw
};

#pragma pack(push, 1)
struct EwMapHeader {            // 40 bytes
  char     magic[4];            // "EWMP"
  uint16_t version_major;
  uint16_t version_minor;
  uint32_t flags;               // HeaderFlags
  uint32_t section_count;
  uint16_t tile_size;           // 8
  uint16_t chunk_w;             // 40
  uint16_t chunk_h;             // 23
  uint16_t map_w_chunks;
  uint16_t map_h_chunks;
  uint16_t reserved0;
  uint32_t reserved1;
  uint64_t section_table_offset;
};

struct SectionDesc {            // 24 bytes
  uint32_t type;                // SectionType
  uint32_t flags;               // por sección (0 en v1)
  uint64_t offset;              // desde el inicio del archivo, múltiplo de 8
  uint64_t size;                // bytes útiles (sin padding)
};

struct ChunkHeader {            // 8 bytes, seguido de 4 capas × 920 u16
  uint16_t cx, cy;
  uint32_t layer_count;         // 4
};

struct EntityRecord {           // 16 bytes
  uint32_t name_str;            // offset en STRTAB
  uint32_t type_str;            // offset en STRTAB
  int32_t  x, y;                // px
};

struct LinkRecord {             // 8 bytes: entidad -> entidad (switch -> puerta)
  uint32_t from_entity, to_entity;
};

struct WaterRecord {            // 24 bytes: volumen de agua (resortes Hooke)
  int32_t x, y, w, h;           // px
  float   stiffness, damping;
};
#pragma pack(pop)

static_assert(sizeof(EwMapHeader) == 40);
static_assert(sizeof(SectionDesc) == 24);
static_assert(sizeof(ChunkHeader) == 8);
static_assert(sizeof(EntityRecord) == 16);
static_assert(sizeof(LinkRecord) == 8);
static_assert(sizeof(WaterRecord) == 24);

inline constexpr std::size_t kChunkBytes =
    sizeof(ChunkHeader) + kLayerCount * kTilesPerChunk * sizeof(uint16_t); // 7368
static_assert(kChunkBytes % kAlignment == 0);

constexpr uint64_t align_up(uint64_t v, uint64_t a = kAlignment) {
  return (v + a - 1) & ~(a - 1);
}

} // namespace ew::map
