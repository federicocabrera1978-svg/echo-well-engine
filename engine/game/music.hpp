#pragma once
// Selección de zona musical procedural según el estado del juego (sin GL/GLFW).
#include "audio/sfx.hpp"
#include "game/scene.hpp"
#include "game/sim.hpp"
#include "map/backdrop.hpp"

#include <cmath>

namespace ew::game {

// Determina la zona musical correspondiente al chunk actual y proximidad a bosses activos.
inline ew::audio::MusicZone music_zone_for(const Scene& scene, const Sim& sim) {
  const auto& pos = sim.player().pos;
  for (int i = 0; i < sim.boss_count(); ++i) {
    const auto& b = sim.boss(i);
    if (!b.defeated && std::fabs(b.x - pos.x) <= 320.f && std::fabs(b.y - pos.y) <= 184.f)
      return ew::audio::MusicZone::Boss;
  }
  using BT = ew::map::BackdropTheme;
  const auto theme = static_cast<BT>(scene.backdrop(scene.chunk_x(), scene.chunk_y()).theme);
  switch (theme) {
    case BT::Cripta:
    case BT::Abismo:
    case BT::Santuario:
    case BT::Engranajes:
    case BT::Invertido:
      return ew::audio::MusicZone::Catacombs;
    case BT::Guarida:
    case BT::Grieta:
    case BT::Arena:
      return ew::audio::MusicZone::WyrmLair;
    case BT::Pozo:
    case BT::Raices:
    case BT::Cisterna:
    case BT::Nido:
    case BT::Cueva:
    case BT::Vacio:
    default:
      return ew::audio::MusicZone::Well;
  }
}

}  // namespace ew::game
