// Carga un .ewmap y verifica la estructura (cualquier mapa).
// Opcional: --name STR --chunks N --ents N --links N --water N --ropes N --has TYPE
//   ewmap_smoke hello.ewmap --name hello --chunks 2 --ents 3 --links 1 --water 2 --ropes 2 --has player_spawn
//   ewmap_smoke slice.ewmap --name slice --chunks 3 --ents 4 --has goal
#include "map/chunk_cache.hpp"
#include "map/map_loader.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace ew::map;

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s\n", #c); return 1; } } while (0)

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: ewmap_smoke FILE [--name STR] [--chunks N] [--ents N] "
                         "[--links N] [--water N] [--ropes N] [--has TYPE]...\n");
    return 2;
  }
  const char* path = argv[1];
  const char* expect_name = nullptr;
  int exp_chunks = -1, exp_ents = -1, exp_links = -1, exp_water = -1, exp_ropes = -1;
  std::vector<std::string> has;
  for (int i = 2; i < argc; ++i) {
    auto eq = [&](const char* s) { return std::strcmp(argv[i], s) == 0; };
    auto need = [&](int& dst) {
      if (i + 1 >= argc) { std::fprintf(stderr, "falta valor para %s\n", argv[i]); std::exit(2); }
      dst = std::atoi(argv[++i]);
    };
    if (eq("--name") && i + 1 < argc) expect_name = argv[++i];
    else if (eq("--chunks")) need(exp_chunks);
    else if (eq("--ents")) need(exp_ents);
    else if (eq("--links")) need(exp_links);
    else if (eq("--water")) need(exp_water);
    else if (eq("--ropes")) need(exp_ropes);
    else if (eq("--has") && i + 1 < argc) has.emplace_back(argv[++i]);
    else { std::fprintf(stderr, "flag desconocido: %s\n", argv[i]); return 2; }
  }

  MapLoader m;
  MapError e = m.load_file(path);
  if (e != MapError::None) {
    std::fprintf(stderr, "load: %.*s\n", int(to_string(e).size()), to_string(e).data());
    return 1;
  }
  const auto& h = m.header();
  CHECK(h.tile_size == kTileSize && h.chunk_w == kChunkW && h.chunk_h == kChunkH);
  CHECK(h.version_major == kVersionMajor);
  if (h.flags & kFlagCrc) CHECK(m.section(SectionType::CRC).size() >= 4);
  const uint32_t nchunks = uint32_t(h.map_w_chunks) * uint32_t(h.map_h_chunks);
  CHECK(m.chunk_count() == nchunks && nchunks > 0);
  // Todas las capas de todos los chunks son accesibles y del tamaño correcto.
  for (uint16_t cy = 0; cy < h.map_h_chunks; ++cy)
    for (uint16_t cx = 0; cx < h.map_w_chunks; ++cx)
      for (uint32_t l = 0; l < kLayerCount; ++l) {
        auto layer = m.chunk_layer(cx, cy, Layer(l));
        CHECK(layer && layer->size() == kTilesPerChunk);
      }

  // Entidades: strings válidos; hay al menos un player_spawn si hay entidades.
  bool has_spawn = false;
  for (const auto& ent : m.entities()) {
    CHECK(!m.string_at(ent.type_str).empty());
    if (m.string_at(ent.type_str) == "player_spawn") has_spawn = true;
  }
  if (!m.entities().empty()) CHECK(has_spawn);

  // Links apuntan a índices válidos.
  const auto nent = uint32_t(m.entities().size());
  for (const auto& lk : m.links()) CHECK(lk.from_entity < nent && lk.to_entity < nent);

  // Agua y cuerdas con geometría coherente (los mismos límites que ewmapc).
  for (const auto& w : m.water()) CHECK(w.w > 0 && w.h > 0);
  for (const auto& r : m.ropes()) CHECK(r.segments >= 1 && r.segments <= 256 && r.seg_len > 0);

  // Nombre del mapa = primer string de STRTAB (offset 1).
  const auto map_name = m.string_at(1);
  CHECK(!map_name.empty());
  if (expect_name) CHECK(map_name == expect_name);

  ChunkCache cache(m, 9);
  CHECK(cache.acquire(0, 0) != nullptr);
  CHECK(cache.acquire(uint16_t(h.map_w_chunks + 1), 0) == nullptr);
  CHECK(cache.acquire(0, uint16_t(h.map_h_chunks + 1)) == nullptr);

  if (exp_chunks >= 0) CHECK(int(m.chunk_count()) == exp_chunks);
  if (exp_ents >= 0) CHECK(int(m.entities().size()) == exp_ents);
  if (exp_links >= 0) CHECK(int(m.links().size()) == exp_links);
  if (exp_water >= 0) CHECK(int(m.water().size()) == exp_water);
  if (exp_ropes >= 0) CHECK(int(m.ropes().size()) == exp_ropes);
  for (const auto& t : has) {
    bool found = false;
    for (const auto& ent : m.entities())
      if (m.string_at(ent.type_str) == t) { found = true; break; }
    if (!found) { std::fprintf(stderr, "FAIL: falta entidad type=%s\n", t.c_str()); return 1; }
  }

  std::printf("ewmap_smoke: %s (%s, %u chunks, %zu ents, %zu links, %zu water, %zu ropes) OK\n",
              path, map_name.data(), m.chunk_count(), m.entities().size(), m.links().size(),
              m.water().size(), m.ropes().size());
  return 0;
}
