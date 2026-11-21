// Test sin GL: fondos por chunk (sección BACKDROP opcional del .ewmap).
//   backdrop_test slice.ewmap hello.ewmap backdrop_ok.ewmap
// - nombres ↔ ids, saneo de ids desconocidos
// - slice: un tema por sala; vecindario 3×3 de Scene (fuera del mapa = cueva)
// - hello: mapa sin sección BACKDROP → cueva en todos lados (retrocompatible)
// - backdrop_ok: chunk sin registro = cueva, seed explícita
// - .ewmap con un tema fuera de rango (CRC recalculado) → se dibuja como cueva
// - .ewmap con la sección BACKDROP renombrada a un tipo desconocido → carga igual, sin fondos
#include "game/scene.hpp"
#include "map/backdrop.hpp"
#include "map/crc32.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

using namespace ew::map;
using BT = BackdropTheme;

namespace {
std::vector<std::byte> read_all(const char* p) {
  std::ifstream f(p, std::ios::binary);
  std::vector<char> c((std::istreambuf_iterator<char>(f)), {});
  std::vector<std::byte> b(c.size());
  if (!c.empty()) std::memcpy(b.data(), c.data(), c.size());
  return b;
}
// Devuelve el SectionDesc (mutable) del tipo pedido, o nullptr.
SectionDesc* find_desc(std::vector<std::byte>& b, SectionType t) {
  EwMapHeader h;
  std::memcpy(&h, b.data(), sizeof h);
  auto* d = reinterpret_cast<SectionDesc*>(b.data() + h.section_table_offset);
  for (uint32_t i = 0; i < h.section_count; ++i)
    if (d[i].type == uint32_t(t)) return &d[i];
  return nullptr;
}
void fix_crc(std::vector<std::byte>& b) {
  SectionDesc* c = find_desc(b, SectionType::CRC);
  if (!c) return;
  const uint32_t v = crc32({b.data(), std::size_t(c->offset)});
  std::memcpy(b.data() + c->offset, &v, 4);
}
bool same(BackdropCell c, BT t) { return c.theme == uint16_t(t); }
}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) { std::fprintf(stderr, "uso: backdrop_test slice.ewmap hello.ewmap backdrop_ok.ewmap\n"); return 2; }

  // --- tabla de nombres -------------------------------------------------------
  CHECK(kBackdropCount == 14);
  CHECK(kBackdropNames.size() == kBackdropCount);
  for (uint16_t i = 0; i < kBackdropCount; ++i) {
    CHECK(backdrop_from_name(kBackdropNames[i]) == i);
    CHECK(backdrop_name(i) == kBackdropNames[i]);
    CHECK(backdrop_sanitize(i) == i);
  }
  CHECK(backdrop_from_name("cripta") == int(BT::Cripta));
  CHECK(backdrop_from_name("guarida") == int(BT::Guarida));
  CHECK(backdrop_from_name("santuario") == int(BT::Santuario));
  CHECK(backdrop_from_name("abismo") == int(BT::Abismo));
  CHECK(backdrop_from_name("lava") == -1);
  CHECK(backdrop_from_name("") == -1);
  CHECK(backdrop_from_name("Pozo") == -1);  // sensible a mayúsculas, como el resto del YAML
  CHECK(backdrop_name(999) == "cueva");
  CHECK(backdrop_sanitize(kBackdropCount) == 0 && backdrop_sanitize(0xFFFF) == 0);
  static_assert(backdrop_from_name("arena") == int(BT::Arena));
  static_assert(backdrop_from_name("abismo") == int(BT::Abismo));

  // --- slice: un fondo por sala ------------------------------------------------
  {
    ew::game::Scene s;
    std::string err;
    if (!s.load(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    CHECK(s.map().backdrops().size() == 8);
    const BT expect[8] = {BT::Pozo, BT::Raices, BT::Grieta, BT::Cisterna,
                          BT::Nido, BT::Engranajes, BT::Invertido, BT::Arena};
    for (int cx = 0; cx < 8; ++cx) CHECK(same(s.backdrop(cx, 0), expect[cx]));
    CHECK(same(s.backdrop(8, 0), BT::Cueva));   // fuera del mapa
    CHECK(same(s.backdrop(-1, 0), BT::Cueva));
    // Spawn en el chunk 0: fila de arriba y abajo fuera del mapa, izquierda fuera.
    CHECK(s.chunk_x() == 0);
    const auto& nb = s.active_backdrops();
    for (int i : {0, 1, 2, 3, 6, 7, 8}) CHECK(same(nb[std::size_t(i)], BT::Cueva));
    CHECK(same(nb[4], BT::Pozo) && same(nb[5], BT::Raices));
    // Al pasar a la sala del boss el vecindario se rearma.
    const uint32_t ver = s.chunk_version();
    s.follow(2264, 152);
    CHECK(s.chunk_x() == 7 && s.chunk_version() != ver);
    CHECK(same(s.active_backdrops()[3], BT::Invertido));
    CHECK(same(s.active_backdrops()[4], BT::Arena));
    CHECK(same(s.active_backdrops()[5], BT::Cueva));
    // seed por defecto = cx*7 + cy*13 (cada sala con otra disposición)
    CHECK(s.backdrop(3, 0).seed == 21 && s.backdrop(7, 0).seed == 49);
  }

  // --- hello: sin sección BACKDROP (mapa "viejo") -------------------------------
  {
    ew::game::Scene s;
    std::string err;
    if (!s.load(argv[2], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    CHECK(s.map().backdrops().empty());
    CHECK(s.map().section(SectionType::BACKDROP).empty());
    for (const auto& c : s.active_backdrops()) CHECK(same(c, BT::Cueva));
    CHECK(same(s.backdrop(1, 0), BT::Cueva));
  }

  // --- backdrop_ok: registro explícito + chunk sin registro ----------------------
  {
    ew::game::Scene s;
    std::string err;
    if (!s.load(argv[3], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    CHECK(s.map().backdrops().size() == 1);
    CHECK(same(s.backdrop(0, 0), BT::Cueva));
    CHECK(same(s.backdrop(1, 0), BT::Invertido) && s.backdrop(1, 0).seed == 42);
  }

  // --- tema fuera de rango en el binario → cueva --------------------------------
  {
    auto b = read_all(argv[3]);
    CHECK(!b.empty());
    SectionDesc* d = find_desc(b, SectionType::BACKDROP);
    CHECK(d && d->size == 8 + sizeof(BackdropRecord));
    BackdropRecord r;
    std::memcpy(&r, b.data() + d->offset + 8, sizeof r);
    r.theme = 777;
    std::memcpy(b.data() + d->offset + 8, &r, sizeof r);
    fix_crc(b);
    MapLoader m;
    CHECK(m.load_bytes(b) == MapError::None);
    CHECK(m.backdrops().size() == 1 && m.backdrops()[0].theme == 777);
    CHECK(same(backdrop_at(m.backdrops(), 1, 0), BT::Cueva));
  }

  // --- sección de tipo desconocido (p. ej. un mapa futuro) → se ignora ------------
  {
    auto b = read_all(argv[1]);
    SectionDesc* d = find_desc(b, SectionType::BACKDROP);
    CHECK(d != nullptr);
    d->type = 99;
    fix_crc(b);
    MapLoader m;
    CHECK(m.load_bytes(b) == MapError::None);
    CHECK(m.backdrops().empty());
    CHECK(m.entities().size() == 22 && m.chunk_count() == 8);
  }

  // --- count corrupto (más registros que bytes) → vacío, sin leer fuera -----------
  {
    auto b = read_all(argv[3]);
    SectionDesc* d = find_desc(b, SectionType::BACKDROP);
    CHECK(d != nullptr);
    const uint32_t huge = 0x10000000u;
    std::memcpy(b.data() + d->offset, &huge, 4);
    fix_crc(b);
    MapLoader m;
    CHECK(m.load_bytes(b) == MapError::None);
    CHECK(m.backdrops().empty());
  }

  // --- level2: fondos dracónicos (cripta, abismo, guarida, santuario) -----------
  if (argc >= 5) {
    ew::game::Scene s;
    std::string err;
    if (!s.load(argv[4], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    CHECK(same(s.backdrop(0, 0), BT::Cripta));
    CHECK(same(s.backdrop(1, 0), BT::Abismo));
    CHECK(same(s.backdrop(2, 0), BT::Invertido));
    CHECK(same(s.backdrop(3, 0), BT::Guarida));
    CHECK(same(s.backdrop(0, 1), BT::Cisterna));
    CHECK(same(s.backdrop(1, 1), BT::Pozo));
    CHECK(same(s.backdrop(2, 1), BT::Engranajes));
    CHECK(same(s.backdrop(3, 1), BT::Santuario));
    s.follow(160, 90);
    CHECK(same(s.active_backdrops()[4], BT::Cripta));
    CHECK(same(s.active_backdrops()[5], BT::Abismo));
  }

  std::printf("backdrop_test OK\n");
  return 0;
}
