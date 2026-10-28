// Carga un .ewmap y verifica lo básico de hello.yaml.
#include "map/chunk_cache.hpp"
#include "map/map_loader.hpp"
#include <cstdio>

using namespace ew::map;

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  MapLoader m;
  MapError e = m.load_file(argv[1]);
  if (e != MapError::None) { std::fprintf(stderr, "load: %.*s\n", int(to_string(e).size()), to_string(e).data()); return 1; }
  CHECK(m.header().flags & kFlagCrc);
  CHECK(m.chunk_count() == 2);  // 2×1
  auto col = m.chunk_layer(0, 0, Layer::COL);
  CHECK(col && col->size() == kTilesPerChunk);
  CHECK((*col)[22 * kChunkW + 0] == 1);   // piso
  CHECK((*col)[5 * kChunkW + 10] == 0); // aire
  CHECK(m.entities().size() == 3);
  CHECK(m.string_at(m.entities()[0].type_str) == "player_spawn");
  CHECK(m.links().size() == 1);
  CHECK(m.water().size() == 1);
  ChunkCache cache(m, 4);
  CHECK(cache.acquire(0, 0) != nullptr);
  CHECK(cache.acquire(5, 5) == nullptr);
  std::puts("ewmap_smoke: OK");
  return 0;
}
