// Fuzz target: loader de atlas PNG (bytes arbitrarios). Nunca debe crashear:
// si el input no es un PNG 128×128 válido, cae al atlas procedural.
#include "render/tile_atlas.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto a = ew::render::load_tile_atlas_mem(data, size);
  const std::size_t want = std::size_t(ew::render::kAtlasPx) * ew::render::kAtlasPx * 4;
  if (a.px.size() != want) return 0;
  volatile uint64_t sink = a.px[0] + a.px[want - 1] + uint64_t(a.source);
  if (size && data) sink += data[0];
  (void)sink;
  return 0;
}
