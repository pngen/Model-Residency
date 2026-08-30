#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/error.hpp"

namespace mr {

/// A deterministic, ordered JSON value.
///
/// Object keys are stored in std::map, so a dump() always emits keys in
/// ascending lexicographic order — the deterministic ordering required for
/// reproducible, diffable output. All JSON emission is side-effect free and
/// never blocks.
class Json {
 public:
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json>;

  Json() : v_(nullptr) {}
  Json(std::nullptr_t) : v_(nullptr) {}
  Json(bool b) : v_(b) {}
  Json(int v) : v_(static_cast<std::int64_t>(v)) {}
  Json(std::uint32_t v) : v_(static_cast<std::int64_t>(v)) {}
  Json(std::uint64_t v) : v_(v) {}
  Json(std::int64_t v) : v_(v) {}
  Json(double d) : v_(d) {}
  Json(const char* s) : v_(std::string(s)) {}
  Json(std::string s) : v_(std::move(s)) {}

  static Json array() { Json j; j.v_ = Array{}; return j; }
  static Json object() { Json j; j.v_ = Object{}; return j; }

  [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(v_); }
  [[nodiscard]] bool is_object() const { return std::holds_alternative<Object>(v_); }
  [[nodiscard]] bool is_array() const { return std::holds_alternative<Array>(v_); }
  [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(v_); }

  Json& set(std::string key, Json value) {
    if (!is_object()) { v_ = Object{}; }
    std::get<Object>(v_)[std::move(key)] = std::move(value);
    return *this;
  }

  Json& push(Json value) {
    if (!is_array()) { v_ = Array{}; }
    std::get<Array>(v_).push_back(std::move(value));
    return *this;
  }

  [[nodiscard]] std::uint64_t array_size() const noexcept {
    return is_array() ? std::get<Array>(v_).size() : 0;
  }
  [[nodiscard]] const Json* get(const std::string& key) const {
    if (!is_object()) return nullptr;
    auto& o = std::get<Object>(v_);
    auto it = o.find(key);
    return it == o.end() ? nullptr : &it->second;
  }

  /// Deterministic serialization to compact JSON text.
  [[nodiscard]] std::string dump() const;

 private:
  std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double, std::string, Object, Array> v_;
};

/// Append a JSON object as a member of another object; returns *this.
inline Json& json_set(Json& dest, const std::string& key, Json value) {
  return dest.set(key, std::move(value));
}

} // namespace mr
