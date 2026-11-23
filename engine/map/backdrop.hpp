// Fondos (backdrops) por chunk/pantalla: temas procedurales que dibuja el geometry pass
// detrás de los tiles (assets/shaders/gbuffer_map.frag). Sin assets binarios.
// Fuente YAML: `backdrops: - {cx, cy, theme: <nombre>, seed}` → sección BACKDROP (opcional).
// Chunks sin registro (o mapas viejos sin la sección) usan `cueva`.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "map/ewmap_format.hpp"

namespace ew::map {

enum class BackdropTheme : uint16_t {
  Cueva = 0,       // por defecto: piedra gris azulada
  Pozo = 1,        // pared curva del pozo, haz de luz desde arriba, gotas
  Raices = 2,      // tierra y raíces colgantes, esporas
  Grieta = 3,      // roca rojiza con grietas que brillan, estalactitas
  Cisterna = 4,    // bloques, caños y marcas de nivel de agua
  Nido = 5,        // nichos con huevos, luciérnagas
  Engranajes = 6,  // engranajes que giran, cadenas, vigas
  Invertido = 7,   // arquitectura espejada techo/piso, polvo que cae hacia arriba
  Arena = 8,       // arcos de coliseo, sello de la bestia, antorchas
  Vacio = 9,       // fondo liso (como antes de los backdrops)
  Cripta = 10,     // arcos góticos con relieves alados, osarios y fuegos fatuos
  Guarida = 11,    // caverna dracónica con silueta de dragón, escamas y brasas tenues
  Santuario = 12,  // templo dracónico con vitral/runa de dragón y columnas altas
  Abismo = 13,     // contrafuertes hacia la penumbra, cadenas y ojos dracónicos
  Aqueduct = 14,   // archivo sumergido: bloques húmedos, caños, burbujas y cristales abisales
  Tower = 15,      // torre de los ecos / campanario: sillares altos, vitrales y campanas
};
inline constexpr uint16_t kBackdropCount = 16;

inline constexpr std::array<std::string_view, kBackdropCount> kBackdropNames{
    "cueva", "pozo", "raices", "grieta", "cisterna", "nido", "engranajes", "invertido", "arena", "vacio",
    "cripta", "guarida", "santuario", "abismo", "aqueduct", "tower"};

// Nombre YAML → id (-1 si no existe).
constexpr int backdrop_from_name(std::string_view n) {
  for (uint16_t i = 0; i < kBackdropCount; ++i)
    if (kBackdropNames[i] == n) return i;
  return -1;
}
// Id → nombre ("cueva" para ids fuera de rango: así los dibuja el runtime).
constexpr std::string_view backdrop_name(uint16_t id) {
  return id < kBackdropCount ? kBackdropNames[id] : kBackdropNames[0];
}
// Id saneado: fuera de rango → cueva (un .ewmap no confiable no puede pedir un tema inexistente).
constexpr uint16_t backdrop_sanitize(uint16_t id) { return id < kBackdropCount ? id : 0; }

struct BackdropCell {
  uint16_t theme = 0, seed = 0;
};
// Tema del chunk (cx, cy). Sin registro: cueva con seed derivada del chunk.
// Si hay registros repetidos para el mismo chunk gana el primero.
inline BackdropCell backdrop_at(std::span<const BackdropRecord> recs, int cx, int cy) {
  for (const auto& r : recs)
    if (r.cx == cx && r.cy == cy) return {backdrop_sanitize(r.theme), r.seed};
  return {0, uint16_t((cx * 7 + cy * 13) & 0xFFFF)};
}

}  // namespace ew::map
