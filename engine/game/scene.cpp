#include "game/scene.hpp"

#include <algorithm>
#include <cmath>

namespace ew::game {
using namespace ew::map;

bool Scene::load(const std::filesystem::path& path, std::string& err) {
  if (auto e = map_.load_file(path); e != MapError::None) {
    err = "no se pudo cargar " + path.string() + ": " + std::string(to_string(e));
    return false;
  }
  const auto& h = map_.header();
  w_px_ = h.map_w_chunks * kChunkW * kTileSize;
  h_px_ = h.map_h_chunks * kChunkH * kTileSize;
  cache_ = std::make_unique<ChunkCache>(map_, 9);
  entities_.clear();
  for (const auto& r : map_.entities())
    entities_.push_back({std::string(map_.string_at(r.name_str)), std::string(map_.string_at(r.type_str)),
                         float(r.x), float(r.y)});
  cx_ = cy_ = -1;
  if (const Entity* s = find_type("player_spawn")) follow(s->x, s->y); else follow(0, 0);
  return true;
}

const Entity* Scene::find_type(std::string_view type) const {
  for (const auto& e : entities_) if (e.type == type) return &e;
  return nullptr;
}

uint16_t Scene::tile(int tx, int ty, Layer layer) {
  if (tx < 0 || ty < 0 || !cache_) return 0;
  const int cx = tx / kChunkW, cy = ty / kChunkH;
  if (cx >= map_.header().map_w_chunks || cy >= map_.header().map_h_chunks) return 0;
  const ChunkData* c = cache_->acquire(uint16_t(cx), uint16_t(cy));
  if (!c) return 0;
  return c->layers[uint32_t(layer)][std::size_t(ty % kChunkH) * kChunkW + std::size_t(tx % kChunkW)];
}

bool Scene::solid_tile(int tx, int ty) {
  if (tx < 0 || ty < 0 || tx * kTileSize >= w_px_ || ty * kTileSize >= h_px_) return true;
  return tile(tx, ty, Layer::COL) != 0;
}

bool Scene::solid_px(float x, float y) {
  return solid_tile(int(std::floor(x / kTileSize)), int(std::floor(y / kTileSize)));
}

void Scene::follow(float x, float y) {
  cam_x_ = std::clamp(std::round(x - kViewW / 2.f), 0.f, float(std::max(0, w_px_ - kViewW)));
  cam_y_ = std::clamp(std::round(y - kViewH / 2.f), 0.f, float(std::max(0, h_px_ - kViewH)));
  const int cx = std::clamp(int(x) / (kChunkW * kTileSize), 0, std::max(0, map_.header().map_w_chunks - 1));
  const int cy = std::clamp(int(y) / (kChunkH * kTileSize), 0, std::max(0, map_.header().map_h_chunks - 1));
  if (cx != cx_ || cy != cy_) { cx_ = cx; cy_ = cy; rebuild_active(); }
}

void Scene::rebuild_active() {
  packed_.fill(0);
  if (const ChunkData* c = cache_->acquire(uint16_t(cx_), uint16_t(cy_))) {
    for (std::size_t i = 0; i < kTilesPerChunk; ++i) {
      packed_[i * 2 + 0] = uint8_t(std::min<uint16_t>(c->layers[uint32_t(Layer::COL)][i], 255));
      packed_[i * 2 + 1] = uint8_t(std::min<uint16_t>(c->layers[uint32_t(Layer::VIS)][i], 255));
    }
  }
  ++chunk_ver_;
}

}  // namespace ew::game
