// Streaming de chunks con LRU. Scaffold: hoy los chunks viven en el buffer del
// MapLoader (raw, v1); esta caché define la API para cuando haya descompresión
// / carga parcial desde disco.
#pragma once
#include "map/map_loader.hpp"
#include <array>
#include <cstddef>
#include <list>
#include <unordered_map>

namespace ew::map {

struct ChunkData {
  uint16_t cx = 0, cy = 0;
  std::array<std::span<const uint16_t>, kLayerCount> layers{};
};

class ChunkCache {
public:
  ChunkCache(const MapLoader& loader, std::size_t capacity) : loader_(loader), capacity_(capacity) {}
  // Devuelve el chunk (y lo marca como más reciente) o nullptr si no existe.
  const ChunkData* acquire(uint16_t cx, uint16_t cy);
  std::size_t size() const { return lru_.size(); }
  std::size_t capacity() const { return capacity_; }

private:
  static uint32_t key(uint16_t cx, uint16_t cy) { return (uint32_t(cx) << 16) | cy; }
  const MapLoader& loader_;
  std::size_t capacity_;
  std::list<ChunkData> lru_; // frente = más reciente
  std::unordered_map<uint32_t, std::list<ChunkData>::iterator> index_;
};

} // namespace ew::map
