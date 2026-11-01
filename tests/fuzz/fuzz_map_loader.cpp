// Fuzz target: MapLoader sobre bytes arbitrarios (.ewmap no confiable).
// libFuzzer (clang -fsanitize=fuzzer) o driver standalone (fuzz_driver.cpp).
#include "map/map_loader.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using namespace ew::map;
  std::vector<std::byte> buf(size);
  if (size) std::memcpy(buf.data(), data, size);
  MapLoader m;
  if (m.load_bytes(std::move(buf)) != MapError::None) return 0;
  volatile uint64_t sink = 0;
  for (const auto& s : m.sections()) sink += s.size;
  for (const auto& e : m.entities()) sink += m.string_at(e.name_str).size() + m.string_at(e.type_str).size();
  for (const auto& l : m.links()) sink += l.from_entity ^ l.to_entity;
  for (const auto& w : m.water()) sink += uint32_t(w.w);
  for (const auto& r : m.ropes()) sink += r.segments;
  sink += m.chunk_count();
  const auto& h = m.header();
  for (uint16_t cy = 0; cy < h.map_h_chunks && cy < 64; ++cy)
    for (uint16_t cx = 0; cx < h.map_w_chunks && cx < 64; ++cx)
      for (uint32_t l = 0; l < kLayerCount; ++l)
        if (auto t = m.chunk_layer(cx, cy, Layer(l))) sink += (*t)[0] + (*t)[t->size() - 1];
  (void)sink;
  return 0;
}
