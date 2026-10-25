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
bool split_kv(const std::string& s, std::string& k, std::string& v) {
  auto p = s.find(':');
  if (p == std::string::npos) return false;
  k = trim(s.substr(0, p));
  v = trim(s.substr(p + 1));
  return !k.empty();
}
bool parse_flow_map(std::string s, Dict& d) {
  s = trim(s);
  if (s.size() < 2 || s.front() != '{' || s.back() != '}') return false;
  std::stringstream ss(s.substr(1, s.size() - 2));
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (trim(item).empty()) continue;
    std::string k, v;
    if (!split_kv(item, k, v)) return false;
    d[k] = unquote(v);
  }
  return true;
}
} // namespace

bool parse_yaml_lite(const std::string& text, YamlDoc& out, std::string& err) {
  std::istringstream in(text);
  std::string raw, section;
  int lineno = 0;
  while (std::getline(in, raw)) {
    ++lineno;
    if (auto h = raw.find('#'); h != std::string::npos) raw = raw.substr(0, h);
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
