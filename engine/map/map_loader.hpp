// MapLoader: valida un .ewmap en memoria y expone vistas (std::span) sin copiar.
#pragma once
#include "map/ewmap_format.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ew::map {

enum class MapError {
  None = 0,
  IoError,
  TooSmall,
  BadMagic,
  UnsupportedVersion,
  BadGeometry,          // tile/chunk distintos de 8 / 40×23
  BadSectionTable,
  SectionOutOfBounds,
  Misaligned,
  CompressedUnsupported,
  CrcMismatch,
};
std::string_view to_string(MapError e);

class MapLoader {
public:
  // Carga y valida. Devuelve MapError::None si todo OK.
  MapError load_file(const std::filesystem::path& path);
  MapError load_bytes(std::vector<std::byte> bytes);

  const EwMapHeader&          header()   const;
  std::span<const SectionDesc> sections() const;
  // Payload de la primera sección de ese tipo (vacío si no existe).
  std::span<const std::byte>  section(SectionType type) const;

  // Helpers tipados
  std::span<const EntityRecord> entities() const;
  std::span<const LinkRecord>   links()    const;
  std::span<const WaterRecord>  water()    const;
  std::string_view              string_at(uint32_t offset) const;

  uint32_t chunk_count() const;
  // Tiles de una capa de un chunk (920 u16) o nullopt si no existe.
  std::optional<std::span<const uint16_t>> chunk_layer(uint16_t cx, uint16_t cy, Layer layer) const;

  bool loaded() const { return !bytes_.empty(); }

private:
  MapError validate();
  template <class T> std::span<const T> records(SectionType t) const;

  std::vector<std::byte> bytes_;
};

} // namespace ew::map
