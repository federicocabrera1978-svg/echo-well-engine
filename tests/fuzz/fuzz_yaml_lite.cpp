// Fuzz target: parser yaml_lite de ewmapc sobre texto arbitrario.
#include "yaml_lite.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string text(reinterpret_cast<const char*>(data), size);
  ewmapc::YamlDoc doc;
  std::string err;
  (void)ewmapc::parse_yaml_lite(text, doc, err);
  return 0;
}
