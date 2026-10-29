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
  links_.clear();
  for (const auto& l : map_.links())
    if (l.from_entity < entities_.size() && l.to_entity < entities_.size()) links_.emplace_back(l.from_entity, l.to_entity);
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
  return tile(tx, ty, Layer::COL) != 0 || door_blocks(tx, ty);
}

bool Scene::door_blocks(int tx, int ty) const {
  for (const auto& e : entities_) {
    if (e.type != "door" || e.active) continue;
    const int dx = int(std::floor(e.x / kTileSize)), dy = int(std::floor(e.y / kTileSize));
    if (tx >= dx && tx < dx + kDoorWTiles && ty >= dy && ty < dy + kDoorHTiles) return true;
  }
  return false;
}

void Scene::set_switch(std::size_t idx, bool on) {
  if (idx >= entities_.size() || entities_[idx].type != "switch") return;
  entities_[idx].active = on;
  for (const auto& [from, to] : links_)
    if (from == idx && entities_[to].type == "door") entities_[to].active = on;
}

int Scene::interact(float x, float y) {
  int best = -1;
  float best_d2 = kSwitchReach * kSwitchReach;
  for (std::size_t i = 0; i < entities_.size(); ++i) {
    if (entities_[i].type != "switch") continue;
    const float dx = entities_[i].x - x, dy = entities_[i].y - y, d2 = dx * dx + dy * dy;
    if (d2 <= best_d2) { best_d2 = d2; best = int(i); }
  }
  if (best >= 0) set_switch(std::size_t(best), !entities_[std::size_t(best)].active);
  return best;
}

bool Scene::solid_px(float x, float y) {
  return solid_tile(int(std::floor(x / kTileSize)), int(std::floor(y / kTileSize)));
}

float Scene::dead_zone_step(float off, float p, float half, float view, float extent) {
  const float c = off + view / 2.f;
  if (p < c - half) off = p - (view / 2.f - half);
  else if (p > c + half) off = p - (view / 2.f + half);
  return std::clamp(std::round(off), 0.f, std::max(0.f, extent - view));
}

void Scene::follow(float x, float y, float dt) {
  constexpr int cw = kChunkW * kTileSize, ch = kChunkH * kTileSize;
  const int cx = std::clamp(int(std::floor(x / cw)), 0, std::max(0, map_.header().map_w_chunks - 1));
  const int cy = std::clamp(int(std::floor(y / ch)), 0, std::max(0, map_.header().map_h_chunks - 1));
  const bool changed = cx != cx_ || cy != cy_;
  if (changed) { cx_ = cx; cy_ = cy; rebuild_active(); }
  const float lx = x - cx * cw, ly = y - cy * ch;
  if (changed || dt <= 0.f) {  // pantalla nueva o snap: centrar (clampado)
    off_x_ = dead_zone_step(0.f, lx, 0.f, kViewW, cw);
    off_y_ = dead_zone_step(0.f, ly, 0.f, kViewH, ch);
  } else {                     // misma pantalla: solo mover al salir de la dead-zone
    off_x_ = dead_zone_step(off_x_, lx, kDeadHalfW, kViewW, cw);
    off_y_ = dead_zone_step(off_y_, ly, kDeadHalfH, kViewH, ch);
  }
  tgt_x_ = float(cx * cw) + off_x_;
  tgt_y_ = float(cy * ch) + off_y_;
  if (dt <= 0.f) { cam_x_ = tgt_x_; cam_y_ = tgt_y_; return; }
  const float a = 1.f - std::exp(-kCamSlide * dt);
  auto slide = [a](float c, float t) {
    const float n = c + (t - c) * a;
    return std::fabs(t - n) < 0.5f ? t : n;
  };
  cam_x_ = slide(cam_x_, tgt_x_);
  cam_y_ = slide(cam_y_, tgt_y_);
}

void Scene::rebuild_active() {
  packed_.fill(0);
  const auto& h = map_.header();
  for (int oy = 0; oy < 3; ++oy)
    for (int ox = 0; ox < 3; ++ox) {
      const int ncx = cx_ - 1 + ox, ncy = cy_ - 1 + oy;
      if (ncx < 0 || ncy < 0 || ncx >= h.map_w_chunks || ncy >= h.map_h_chunks) continue;
      const ChunkData* c = cache_->acquire(uint16_t(ncx), uint16_t(ncy));
      if (!c) continue;
      for (int ty = 0; ty < kChunkH; ++ty)
        for (int tx = 0; tx < kChunkW; ++tx) {
          const std::size_t i = std::size_t(ty) * kChunkW + std::size_t(tx);
          const std::size_t o = (std::size_t(oy * kChunkH + ty) * kNbW + std::size_t(ox * kChunkW + tx)) * 2;
          packed_[o + 0] = uint8_t(std::min<uint16_t>(c->layers[uint32_t(Layer::COL)][i], 255));
          packed_[o + 1] = uint8_t(std::min<uint16_t>(c->layers[uint32_t(Layer::VIS)][i], 255));
        }
    }
  ++chunk_ver_;
}

}  // namespace ew::game
