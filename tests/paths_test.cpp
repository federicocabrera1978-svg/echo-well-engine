// Resolución de assets: pick_existing / pick_asset, sin GL.
#include "core/paths.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main() {
  using namespace ew::core;
  namespace fs = std::filesystem;
  const auto tmp = fs::temp_directory_path() / "ew_paths_test";
  fs::remove_all(tmp);
  fs::create_directories(tmp / "assets" / "atlas");
  const auto png = tmp / "assets" / "atlas" / "tiles.png";
  { std::ofstream f(png, std::ios::binary); f << "PNG"; }
  CHECK(file_exists(png.string()));
  CHECK(!file_exists((tmp / "nope").string()));
  CHECK(!file_exists(""));

  const auto hit = pick_existing({(tmp / "nope").string(), png.string(), "also-nope"});
  CHECK(hit == png.string());
  const auto miss = pick_existing({(tmp / "nope").string(), ""});
  CHECK(miss == (tmp / "nope").string());  // último no vacío

  // Preferencia: junto al "exe" (root) antes que cwd o compiled.
  const auto picked = pick_asset(tmp.string(), "assets/atlas/tiles.png", "/compiled/does-not-exist.png");
  CHECK(picked == png.string());

  // Si no está junto al exe, compiled (si existe).
  const auto compiled = tmp / "compiled.png";
  { std::ofstream f(compiled, std::ios::binary); f << "C"; }
  const auto fb = pick_asset((tmp / "empty").string(), "assets/atlas/nope.png", compiled.string());
  CHECK(fb == compiled.string());

  // join
  CHECK(join("a", "b") == "a/b");
  CHECK(join("a/", "b") == "a/b");
  CHECK(join("", "b") == "b");
  CHECK(parent_path("a/b/c") == "a/b");
  CHECK(parent_path("c") == ".");

  // exe_dir devuelve algo no vacío (este binario).
  CHECK(!exe_dir(nullptr).empty());

  fs::remove_all(tmp);
  std::puts("paths ok");
  return 0;
}
