// ewmapc — compila fuentes YAML de mapa a binario .ewmap
//   ewmapc [--crc] <input.yaml> -o <output.ewmap>
#include "map/map_loader.hpp"
#include "writer.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
  std::string in, out;
  ewmapc::WriteOptions opt;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--crc") opt.crc = true;
    else if (a == "-o" && i + 1 < argc) out = argv[++i];
    else if (a == "-h" || a == "--help") in.clear(), out.clear(), i = argc;
    else in = a;
  }
  if (in.empty() || out.empty()) {
    std::fprintf(stderr, "usage: ewmapc [--crc] <input.yaml> -o <output.ewmap>\n");
    return 2;
  }
  std::ifstream fin(in);
  if (!fin) { std::fprintf(stderr, "ewmapc: cannot read %s\n", in.c_str()); return 1; }
  std::stringstream ss;
  ss << fin.rdbuf();

  ewmapc::YamlDoc doc;
  std::string err;
  if (!ewmapc::parse_yaml_lite(ss.str(), doc, err)) {
    std::fprintf(stderr, "%s:%s\n", in.c_str(), err.c_str());
    return 1;
  }
  std::vector<std::byte> bytes;
  if (!ewmapc::build_ewmap(doc, opt, bytes, err)) {
    std::fprintf(stderr, "%s: %s\n", in.c_str(), err.c_str());
    return 1;
  }
  // Autoverificación: lo que emitimos tiene que cargar.
  ew::map::MapLoader check;
  if (auto e = check.load_bytes(bytes); e != ew::map::MapError::None) {
    std::fprintf(stderr, "ewmapc: internal error, output fails validation: %.*s\n",
                 int(to_string(e).size()), to_string(e).data());
    return 1;
  }
  std::ofstream fout(out, std::ios::binary);
  fout.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  if (!fout) { std::fprintf(stderr, "ewmapc: cannot write %s\n", out.c_str()); return 1; }
  std::printf("ewmapc: %s -> %s (%zu bytes, %u chunks, %zu entities%s)\n", in.c_str(), out.c_str(),
              bytes.size(), check.chunk_count(), check.entities().size(), opt.crc ? ", crc" : "");
  return 0;
}
