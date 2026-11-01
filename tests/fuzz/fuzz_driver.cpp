// Driver standalone (sin libFuzzer, p. ej. GCC): corre el target sobre cada
// archivo/dir de corpus y luego N mutaciones aleatorias deterministas.
// Uso: fuzz_x [-runs=N] [-seed=S] corpus...
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static std::vector<uint8_t> read_file(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}

int main(int argc, char** argv) {
  unsigned long runs = 20000, seed = 1;
  std::vector<std::vector<uint8_t>> corpus;
  for (int i = 1; i < argc; ++i) {
    if (!std::strncmp(argv[i], "-runs=", 6)) { runs = std::strtoul(argv[i] + 6, nullptr, 10); continue; }
    if (!std::strncmp(argv[i], "-seed=", 6)) { seed = std::strtoul(argv[i] + 6, nullptr, 10); continue; }
    if (argv[i][0] == '-') continue;  // ignora flags de libFuzzer
    std::filesystem::path p(argv[i]);
    if (std::filesystem::is_directory(p)) {
      for (auto& e : std::filesystem::directory_iterator(p))
        if (e.is_regular_file()) corpus.push_back(read_file(e.path()));
    } else if (std::filesystem::exists(p)) {
      corpus.push_back(read_file(p));
    }
  }
  if (corpus.empty()) corpus.push_back({});
  for (auto& c : corpus) LLVMFuzzerTestOneInput(c.data(), c.size());
  std::mt19937_64 rng(seed);
  for (unsigned long r = 0; r < runs; ++r) {
    auto in = corpus[rng() % corpus.size()];
    const int muts = 1 + int(rng() % 8);
    for (int k = 0; k < muts; ++k) {
      switch (rng() % 5) {
        case 0: if (!in.empty()) in[rng() % in.size()] ^= uint8_t(1u << (rng() % 8)); break;
        case 1: if (!in.empty()) in[rng() % in.size()] = uint8_t(rng()); break;
        case 2: in.insert(in.begin() + (in.empty() ? 0 : rng() % in.size()), uint8_t(rng())); break;
        case 3: if (!in.empty()) in.resize(rng() % in.size()); break;
        case 4: if (in.size() >= 4) { uint32_t v = uint32_t(rng()); std::memcpy(&in[rng() % (in.size() - 3)], &v, 4); } break;
      }
    }
    LLVMFuzzerTestOneInput(in.data(), in.size());
  }
  std::printf("fuzz_driver: %zu seeds + %lu mutaciones OK\n", corpus.size(), runs);
  return 0;
}
