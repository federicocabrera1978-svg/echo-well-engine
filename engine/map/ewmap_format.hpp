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
  TILE = 1, ENT = 2, LINK = 3, WATER = 4, STRTAB = 5, CRC = 6, ROPE = 7, MOVER = 8, BACKDROP = 9,
  DIALOG = 10, GUIDE = 11,
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
struct RopeRecord {             // 16 bytes: cuerda Verlet colgando de un ancla fija
  int32_t  x, y;                // ancla (px)
  uint32_t segments;            // cantidad de segmentos (>= 1)
  float    seg_len;             // largo de cada segmento (px)
};

inline constexpr uint32_t kMaxMoverWaypoints = 8;
inline constexpr uint16_t kMaxMoverTiles = 64;  // w/h en tiles

struct MoverRecord {            // 88 bytes: plataforma que recorre waypoints (ping-pong)
  uint32_t entity;              // índice en ENT (type "mover")
  uint16_t w_tiles, h_tiles;    // tamaño en tiles (1..kMaxMoverTiles)
  float    speed;               // px/s a lo largo del segmento
  float    pause;               // s de pausa al llegar a cada waypoint
  uint16_t n_wp;                // 2..kMaxMoverWaypoints
  uint16_t tile;                // id fuente YAML (familia = (tile%15)+1)
  uint32_t pad;                 // 0
  int32_t  wp_x[kMaxMoverWaypoints];
  int32_t  wp_y[kMaxMoverWaypoints];
};
struct BackdropRecord {         // 8 bytes: fondo (backdrop) de un chunk / pantalla
  uint16_t cx, cy;              // chunk
  uint16_t theme;               // ew::map::BackdropTheme (ver map/backdrop.hpp); desconocido = cueva
  uint16_t seed;                // variación del dibujo procedural (mismo tema, otra disposición)
};

enum class DialogTrigger : uint16_t {
  Interact  = 0,  // E / X / ↓ cerca de la entidad (npc / lore)
  Pickup    = 1,  // automático al recoger la entidad (collectible / ability_*)
  Proximity = 2,  // automático al acercarse por primera vez
};
inline constexpr uint16_t kDialogTriggerCount = 3;
inline constexpr uint16_t kDialogFlagOnce = 1u << 0;

struct DialogRecord {           // 16 bytes: diálogo asociado a una entidad
  uint32_t entity;              // índice en ENT
  uint32_t speaker_str;         // offset en STRTAB (hablante / título)
  uint32_t text_str;            // offset en STRTAB (páginas separadas por '|', líneas por '/')
  uint16_t trigger;             // DialogTrigger
  uint16_t flags;               // kDialogFlagOnce, etc.
};

enum class GuideKind : uint16_t { Objective = 0, Shaft = 1 };
enum GuideFlags : uint16_t {
  kGuideAbility       = 1u << 0, // estilo cian de reliquia/habilidad (sino ámbar/switch)
  kGuideCrossWhenOpen = 1u << 1, // activo cuando target->active es true y gate_entity (o !below_y) aún falta
  kGuideForwardX      = 1u << 2, // se considera superado si player.x > x
  kGuideBelowY        = 1u << 3, // para Objective cross: solo mientras player.y >= y_bot
};
struct GuideRecord { // 24 bytes: objetivos y rutas verticales opcionales (sección GUIDE = 11)
  uint32_t entity;      // Objective: índice en ENT; Shaft: up_x en px (reinterpretado) o ~0u (= x)
  uint32_t gate_entity; // Objective: puerta/muro asociado (~0u si no aplica); Shaft: y_split (default 184)
  int32_t  x;           // Shaft: centro X para bajar (px); Objective: max_x si kGuideForwardX
  int32_t  y_top;       // Shaft: Y boca superior (px); Objective: 0
  int32_t  y_bot;       // Shaft: Y boca inferior (px); Objective: min_y si kGuideBelowY
  uint16_t kind;        // GuideKind
  uint16_t flags;       // GuideFlags
};
#pragma pack(pop)

static_assert(sizeof(EwMapHeader) == 40);
static_assert(sizeof(SectionDesc) == 24);
static_assert(sizeof(ChunkHeader) == 8);
static_assert(sizeof(EntityRecord) == 16);
static_assert(sizeof(LinkRecord) == 8);
static_assert(sizeof(WaterRecord) == 24);
static_assert(sizeof(RopeRecord) == 16);
static_assert(sizeof(MoverRecord) == 88);
static_assert(sizeof(BackdropRecord) == 8);
static_assert(sizeof(DialogRecord) == 16);
static_assert(sizeof(GuideRecord) == 24);

inline constexpr std::size_t kChunkBytes =
    sizeof(ChunkHeader) + kLayerCount * kTilesPerChunk * sizeof(uint16_t); // 7368
static_assert(kChunkBytes % kAlignment == 0);

constexpr uint64_t align_up(uint64_t v, uint64_t a = kAlignment) {
  return (v + a - 1) & ~(a - 1);
}

} // namespace ew::map
