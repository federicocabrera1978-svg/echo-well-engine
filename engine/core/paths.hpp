#pragma once
// Resolución de assets portables: junto al ejecutable, cwd, o ruta de compile-time.
#include <fstream>
#include <initializer_list>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ew::core {

inline bool file_exists(const std::string& p) {
  if (p.empty()) return false;
  std::ifstream f(p, std::ios::binary);
  return bool(f);
}

inline std::string join(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  const char c = a.back();
  if (c == '/' || c == '\\') return a + b;
  return a + '/' + b;
}

inline std::string parent_path(const std::string& p) {
  const auto sl = p.find_last_of("/\\");
  return sl == std::string::npos ? std::string(".") : p.substr(0, sl);
}

inline std::string exe_dir(const char* argv0 = nullptr) {
#ifdef _WIN32
  char buf[MAX_PATH]{};
  const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  std::string s = (n > 0 && n < MAX_PATH) ? std::string(buf) : (argv0 ? argv0 : "");
#else
  char buf[4096];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  std::string s;
  if (n > 0) {
    buf[n] = 0;
    s = buf;
  } else if (argv0) {
    s = argv0;
  }
#endif
  const auto sl = s.find_last_of("/\\");
  return sl == std::string::npos ? std::string(".") : s.substr(0, sl);
}

// Primera ruta existente. Si ninguna existe, devuelve el último candidato no vacío.
inline std::string pick_existing(std::initializer_list<std::string> cands) {
  std::string last;
  for (const auto& p : cands) {
    if (p.empty()) continue;
    last = p;
    if (file_exists(p)) return p;
  }
  return last;
}

// Busca `rel` junto al ejecutable, en el cwd y como último recurso `compiled`.
inline std::string pick_asset(const std::string& root, const std::string& rel,
                              const std::string& compiled = {}) {
  return pick_existing({join(root, rel), rel, compiled});
}

}  // namespace ew::core
