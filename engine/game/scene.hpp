// Scene: estado de juego sin GL. Envuelve MapLoader + ChunkCache y expone
// colisión por tiles (capa COL), entidades, cámara y chunk activo.
#pragma once
#include "map/chunk_cache.hpp"
#include "map/map_loader.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ew::game {

inline constexpr int kViewW = 320, kViewH = 180;  // resolución interna

struct Entity {
  std::string name, type;
  float x = 0, y = 0;  // px mundo (y hacia abajo, como el YAML)
  bool active = false;  // switch: encendido · door: abierta
};

// Puerta: 1×3 tiles (8×24 px) con esquina superior izquierda en (x, y).
inline constexpr int kDoorWTiles = 1, kDoorHTiles = 3;
inline constexpr float kSwitchReach = 14.f;  // px: distancia máx. player↔switch para activarlo

class Scene {
 public:
  bool load(const std::filesystem::path& path, std::string& err);

  int width_px() const { return w_px_; }
  int height_px() const { return h_px_; }
  const std::vector<Entity>& entities() const { return entities_; }
  // Switch → puerta (sección LINK). Toggle del switch más cercano dentro de kSwitchReach
  // de (x, y); abre/cierra todas sus puertas enlazadas. Devuelve el índice o -1.
  int interact(float x, float y);
  void set_switch(std::size_t idx, bool on);
  // Puerta cerrada en el tile (tx, ty)?
  bool door_blocks(int tx, int ty) const;
  const Entity* find_type(std::string_view type) const;
  const ew::map::MapLoader& map() const { return map_; }

  // Tile de una capa en coordenadas de tile globales (0 fuera del mapa).
  uint16_t tile(int tx, int ty, ew::map::Layer layer);
  // Sólido = COL != 0, o fuera del mapa (bordes cerrados).
  bool solid_tile(int tx, int ty);
  bool solid_px(float x, float y);

  // Cámara por pantallas (estilo Animal Well): cada chunk (320×184) es una pantalla.
  // El objetivo es el origen del chunk del player (+ los 4 px de holgura vertical
  // siguiendo al player). Con dt > 0 desliza hacia el objetivo (exponencial, ~0.3 s);
  // con dt <= 0 hace snap. Actualiza el chunk activo y su vecindario 3×3.
  static constexpr float kCamSlide = 12.f;  // 1/s
  void follow(float x, float y, float dt = 0.f);
  bool cam_transitioning() const { return cam_x_ != tgt_x_ || cam_y_ != tgt_y_; }
  float cam_x() const { return cam_x_; }
  float cam_y() const { return cam_y_; }
  int chunk_x() const { return cx_; }
  int chunk_y() const { return cy_; }
  // Incrementa cada vez que cambia el chunk activo (para re-subir texturas).
  uint32_t chunk_version() const { return chunk_ver_; }
  // Vecindario 3×3 de chunks centrado en el activo, empaquetado (R = COL, G = VIS),
  // kNbW×kNbH tiles, fila 0 = arriba. Chunks fuera del mapa = 0. Origen en nb_origin_*.
  static constexpr int kNbW = ew::map::kChunkW * 3, kNbH = ew::map::kChunkH * 3;
  const std::array<uint8_t, std::size_t(kNbW) * kNbH * 2>& active_tiles() const { return packed_; }
  float nb_origin_x() const { return float((cx_ - 1) * ew::map::kChunkW * ew::map::kTileSize); }
  float nb_origin_y() const { return float((cy_ - 1) * ew::map::kChunkH * ew::map::kTileSize); }

 private:
  void rebuild_active();
  ew::map::MapLoader map_;
  std::unique_ptr<ew::map::ChunkCache> cache_;
  std::vector<Entity> entities_;
  std::vector<std::pair<uint32_t, uint32_t>> links_;
  int w_px_ = 0, h_px_ = 0;
  float cam_x_ = 0, cam_y_ = 0, tgt_x_ = 0, tgt_y_ = 0;
  int cx_ = -1, cy_ = -1;
  uint32_t chunk_ver_ = 0;
  std::array<uint8_t, std::size_t(kNbW) * kNbH * 2> packed_{};
};

}  // namespace ew::game
