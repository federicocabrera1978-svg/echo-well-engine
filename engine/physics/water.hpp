// Agua con resortes de Hooke: la superficie es una fila de columnas; cada una es
// un resorte (a = -k·x - d·v) acoplado a sus vecinas (propagación de ondas).
#pragma once
#include "physics/verlet.hpp"

#include <vector>

namespace ew::physics {

class WaterBody {
 public:
  // rect en px mundo (y hacia abajo: y = superficie en reposo, y + h = fondo).
  // stiffness/damping por paso, como en WaterRecord del .ewmap.
  WaterBody(float x, float y, float w, float h, float stiffness, float damping, int max_cols = 64);

  float spread = 0.25f;        // acople entre columnas vecinas
  int spread_passes = 4;
  float buoyancy = 1400.f;     // px/s² por unidad de sumergido (0..1) (gravedad = 600)
  float drag = 0.15f;          // pérdida de velocidad por paso dentro del agua
  float splash = 0.35f;        // cuánto de la velocidad vertical del cuerpo pasa a la superficie

  void step();
  // Empuje + arrastre a los puntos sumergidos y perturbación de la superficie.
  void interact(World& world);
  void disturb(float x, float dv);  // suma velocidad a la columna bajo x

  int cols() const { return int(h_.size()); }
  float x() const { return x_; }
  float rest_y() const { return y_; }
  float width() const { return w_; }
  float bottom() const { return y_ + h_px_; }
  float surface_at(float x) const;  // y de la superficie en x (px mundo)
  float surface_col(int i) const { return y_ + h_[std::size_t(i)]; }
  float energy() const;              // Σ v² + k·x² (para tests)
  bool contains(float x, float y) const { return x >= x_ && x < x_ + w_ && y >= surface_at(x) && y < bottom(); }

 private:
  int col_of(float x) const;
  float x_, y_, w_, h_px_, k_, d_;
  std::vector<float> h_, v_, ldelta_, rdelta_;
};

}  // namespace ew::physics
