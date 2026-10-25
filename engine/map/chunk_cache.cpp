#include "map/chunk_cache.hpp"
namespace ew::map {
const ChunkData* ChunkCache::acquire(uint16_t cx, uint16_t cy) {
  const uint32_t k = key(cx, cy);
  if (auto it = index_.find(k); it != index_.end()) {
    lru_.splice(lru_.begin(), lru_, it->second);
    return &*it->second;
  }
  ChunkData d{cx, cy, {}};
  for (uint32_t l = 0; l < kLayerCount; ++l) {
    auto layer = loader_.chunk_layer(cx, cy, Layer(l));
    if (!layer) return nullptr;
    d.layers[l] = *layer;
  }
  if (capacity_ && lru_.size() >= capacity_) {
    index_.erase(key(lru_.back().cx, lru_.back().cy));
    lru_.pop_back();
  }
  lru_.push_front(d);
  index_[k] = lru_.begin();
  return &lru_.front();
}
} // namespace ew::map
