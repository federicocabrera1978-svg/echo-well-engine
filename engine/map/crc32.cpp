#include "map/crc32.hpp"
#include <array>
namespace ew::map {
namespace {
constexpr std::array<uint32_t, 256> make_table() {
  std::array<uint32_t, 256> t{};
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    t[i] = c;
  }
  return t;
}
constexpr auto kTable = make_table();
} // namespace
uint32_t crc32(std::span<const std::byte> data) {
  uint32_t c = 0xFFFFFFFFu;
  for (std::byte b : data) c = kTable[(c ^ static_cast<uint8_t>(b)) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}
} // namespace ew::map
