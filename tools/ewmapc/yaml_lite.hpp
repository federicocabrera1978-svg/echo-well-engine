// Parser YAML mínimo para fuentes de mapa v1 (subconjunto deliberadamente chico):
//   key: scalar            key: [a, b]          key: []
//   key:                   key:
//     sub: scalar            - {k: v, k2: v2}
// Comentarios con '#'. Sin anclas, sin multilínea. Reemplazable por yaml-cpp más adelante.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace ewmapc {
using Dict = std::map<std::string, std::string>;
struct YamlDoc {
  std::map<std::string, std::string> scalars;
  std::map<std::string, Dict> maps;
  std::map<std::string, std::vector<Dict>> lists;
};
// Devuelve false y llena `err` (con número de línea) si falla.
bool parse_yaml_lite(const std::string& text, YamlDoc& out, std::string& err);
}
