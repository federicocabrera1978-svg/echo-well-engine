#include "yaml_lite.hpp"
#include <sstream>

namespace ewmapc {
namespace {
std::string trim(const std::string& s) {
  auto b = s.find_first_not_of(" \t\r");
  if (b == std::string::npos) return {};
  auto e = s.find_last_not_of(" \t\r");
  return s.substr(b, e - b + 1);
}
std::string unquote(std::string s) {
  s = trim(s);
  if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front())
    return s.substr(1, s.size() - 2);
  return s;
}
std::string strip_comment(const std::string& raw) {
  char q = 0;
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const char c = raw[i];
    if (q == 0) {
      if (c == '"' || c == '\'') q = c;
      else if (c == '#') return raw.substr(0, i);
    } else if (c == q) {
      q = 0;
    }
  }
  return raw;
}
bool split_kv(const std::string& s, std::string& k, std::string& v) {
  char q = 0;
  std::size_t p = std::string::npos;
  for (std::size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (q == 0) {
      if (c == '"' || c == '\'') q = c;
      else if (c == ':') { p = i; break; }
    } else if (c == q) {
      q = 0;
    }
  }
  if (p == std::string::npos) return false;
  k = trim(s.substr(0, p));
  v = trim(s.substr(p + 1));
  return !k.empty();
}
bool parse_flow_map(std::string s, Dict& d) {
  s = trim(s);
  if (s.size() < 2 || s.front() != '{' || s.back() != '}') return false;
  const std::string inner = s.substr(1, s.size() - 2);
  char q = 0;
  std::size_t start = 0;
  auto push_item = [&](std::size_t end) -> bool {
    const std::string item = trim(inner.substr(start, end - start));
    if (item.empty()) return true;
    std::string k, v;
    if (!split_kv(item, k, v)) return false;
    d[k] = unquote(v);
    return true;
  };
  for (std::size_t i = 0; i < inner.size(); ++i) {
    const char c = inner[i];
    if (q == 0) {
      if (c == '"' || c == '\'') q = c;
      else if (c == ',') {
        if (!push_item(i)) return false;
        start = i + 1;
      }
    } else if (c == q) {
      q = 0;
    }
  }
  if (q != 0) return false;
  return push_item(inner.size());
}
} // namespace

bool parse_yaml_lite(const std::string& text, YamlDoc& out, std::string& err) {
  std::istringstream in(text);
  std::string raw, section;
  int lineno = 0;
  while (std::getline(in, raw)) {
    ++lineno;
    raw = strip_comment(raw);
    if (trim(raw).empty()) continue;
    const bool indented = raw[0] == ' ' || raw[0] == '\t';
    const std::string line = trim(raw);
    auto fail = [&](const char* why) { err = "line " + std::to_string(lineno) + ": " + why; return false; };

    if (!indented) {
      std::string k, v;
      if (!split_kv(line, k, v)) return fail("expected 'key: value'");
      if (v.empty()) { section = k; continue; }
      section.clear();
      if (v == "[]") out.lists[k];
      else out.scalars[k] = unquote(v);
      continue;
    }
    if (section.empty()) return fail("indented line without parent key");
    if (line.rfind("- ", 0) == 0) {
      Dict d;
      if (!parse_flow_map(line.substr(2), d)) return fail("list items must be flow maps: - {k: v}");
      out.lists[section].push_back(std::move(d));
    } else {
      std::string k, v;
      if (!split_kv(line, k, v)) return fail("expected 'key: value'");
      out.maps[section][k] = unquote(v);
    }
  }
  return true;
}
} // namespace ewmapc
