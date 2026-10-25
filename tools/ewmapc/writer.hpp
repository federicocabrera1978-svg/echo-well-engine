#pragma once
#include "yaml_lite.hpp"
#include <cstddef>
#include <string>
#include <vector>
namespace ewmapc {
struct WriteOptions { bool crc = false; };
// Convierte el documento fuente a bytes .ewmap. false + err si la fuente es inválida.
bool build_ewmap(const YamlDoc& doc, const WriteOptions& opt, std::vector<std::byte>& out, std::string& err);
}
