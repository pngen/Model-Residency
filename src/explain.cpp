#include "mr/explain.hpp"

#include <cstdio>
#include <utility>

namespace mr {

void Explanation::add_bytes_factor(std::string key, Bytes value, std::string note) {
  ExplanationFactor f;
  f.key = std::move(key);
  f.value = static_cast<double>(value.value());
  f.note = std::move(note);
  f.kind = AccountingKind::Derived;
  factors_.push_back(std::move(f));
}

Json Explanation::to_json() const {
  Json root = Json::object();
  Json arr = Json::array();
  for (const auto& f : factors_) {
    Json jf = Json::object();
    jf.set("key", f.key);
    jf.set("value", f.value);
    jf.set("weight", f.weight);
    jf.set("kind", accounting_kind_name(f.kind));
    jf.set("note", f.note);
    arr.push(std::move(jf));
  }
  root.set("factors", std::move(arr));
  Json narr = Json::array();
  for (const auto& line : narrative_) { narr.push(line); }
  root.set("narrative", std::move(narr));
  return root;
}

std::string Explanation::to_text() const {
  std::string out;
  for (const auto& line : narrative_) {
    out += line;
    out += '\n';
  }
  for (const auto& f : factors_) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "  %-28s = %.6g", f.key.c_str(), f.value);
    out += buf;
    if (!f.note.empty()) { out += "  (" + f.note + ")"; }
    out += '\n';
  }
  return out;
}

} // namespace mr
