#pragma once

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "mr/config.hpp"

namespace mr {

/// Format a printf-style string into std::string.
inline std::string strprintf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_list copy;
  va_copy(copy, args);
  const int needed = std::vsnprintf(nullptr, 0, fmt, copy);
  va_end(copy);
  if (needed < 0) {
    va_end(args);
    return std::string();
  }
  std::string result(static_cast<std::size_t>(needed), '\0');
  std::vsnprintf(result.data(), result.size() + 1, fmt, args);
  va_end(args);
  return result;
}

/// Split a string on a delimiter into non-empty pieces.
inline std::vector<std::string> split(std::string_view text, char delimiter) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t pos = text.find(delimiter, start);
    if (pos == std::string_view::npos) {
      out.emplace_back(text.substr(start));
      break;
    }
    out.emplace_back(text.substr(start, pos - start));
    start = pos + 1;
  }
  return out;
}

/// Lowercase a string in-place.
inline void lower(std::string& s) {
  for (char& c : s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
}

} // namespace mr
