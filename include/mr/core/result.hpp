#pragma once

#include <optional>
#include <utility>

#include "mr/config.hpp"
#include "mr/core/error.hpp"

namespace mr {

template <typename T>
class Result {
 public:
  Result(T value) : value_(std::move(value)) {}
  Result(Error error) : error_(std::move(error)) {}

  [[nodiscard]] bool ok() const noexcept { return value_.has_value(); }
  [[nodiscard]] bool failed() const noexcept { return !value_.has_value(); }

  [[nodiscard]] const T& value() const {
    if (!value_) { throw *error_; }
    return *value_;
  }
  [[nodiscard]] T& value() {
    if (!value_) { throw *error_; }
    return *value_;
  }

  [[nodiscard]] const Error& error() const {
    if (value_) { throw Error(ErrorCode::Unexpected, "result holds a value"); }
    return *error_;
  }

  [[nodiscard]] T take() {
    if (!value_) { throw *error_; }
    return std::move(*value_);
  }

 private:
  std::optional<T> value_;
  std::optional<Error> error_;
};

template <>
class Result<void> {
 public:
  Result() = default;
  Result(Error error) : error_(std::move(error)) {}

  [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
  [[nodiscard]] bool failed() const noexcept { return error_.has_value(); }
  [[nodiscard]] const Error& error() const {
    if (!error_) { throw Error(ErrorCode::Unexpected, "result holds success"); }
    return *error_;
  }

 private:
  std::optional<Error> error_;
};

} // namespace mr
