// Fuzz target: decoder del save (bytes arbitrarios). Nunca debe leer fuera del buffer ni
// crashear; si acepta, re-codificar tiene que dar exactamente los mismos bytes (canónico).
#include "game/save.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ew::game::SaveData d;
  if (ew::game::decode_save(data, size, d) != ew::game::SaveError::None) return 0;
  const auto again = ew::game::encode_save(d);
  if (again.size() != size || std::memcmp(again.data(), data, size) != 0) std::abort();
  return 0;
}
