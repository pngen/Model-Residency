#include "mr/core/json.hpp"

#include <cstdio>
#include <type_traits>

namespace mr {
namespace {

void append_escaped(std::string& out, const std::string& s) {
  out.push_back('"');
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
}

} // namespace

std::string Json::dump() const {
  std::string out;
  // Recursive dispatch over the variant.
  std::visit(
      [&out](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
          out += "null";
        } else if constexpr (std::is_same_v<T, bool>) {
          out += v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          out += std::to_string(v);
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
          out += std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
          throw_if(!std::isfinite(v), ErrorCode::InvalidArgument, "cannot dump non-finite double as JSON");
          char buf[40];
          std::snprintf(buf, sizeof(buf), "%.17g", v);
          out += buf;
        } else if constexpr (std::is_same_v<T, std::string>) {
          append_escaped(out, v);
        } else if constexpr (std::is_same_v<T, Array>) {
          out += '[';
          bool first = true;
          for (const auto& item : v) {
            if (!first) out += ',';
            out += item.dump();
            first = false;
          }
          out += ']';
        } else {
          out += '{';
          bool first = true;
          for (const auto& [key, value] : v) {
            if (!first) out += ',';
            append_escaped(out, key);
            out += ':';
            out += value.dump();
            first = false;
          }
          out += '}';
        }
      },
      v_);
  return out;
}

} // namespace mr
