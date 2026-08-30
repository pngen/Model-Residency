#pragma once

#include <stdexcept>
#include <string>

#include "mr/config.hpp"

namespace mr {

enum class ErrorCode {
  None = 0,
  InvalidArgument,
  OutOfRange,
  CapacityExceeded,
  InvalidTransition,
  NotFound,
  AlreadyExists,
  StaleAuthority,
  GenerationMismatch,
  CompatibilityRejected,
  InvalidIdentity,
  InvalidState,
  PersistenceFormat,
  PersistenceChecksum,
  PersistenceTruncation,
  PersistenceCorruption,
  Protocol,
  ProtocolFrame,
  ProtocolTruncation,
  ProtocolMalformed,
  Io,
  Unexpected,
  NotSupported,
  Overflow,
  Underflow,
};

class Error : public std::runtime_error {
 public:
  Error(ErrorCode code, const std::string& message)
      : std::runtime_error(message), code_(code) {}

  explicit Error(const std::string& message)
      : std::runtime_error(message), code_(ErrorCode::Unexpected) {}

  [[nodiscard]] ErrorCode code() const noexcept { return code_; }

 private:
  ErrorCode code_;
};

[[noreturn]] inline void throw_error(ErrorCode code, const std::string& message) {
  throw Error(code, message);
}

inline void throw_if(bool condition, ErrorCode code, const std::string& message) {
  if (condition) {
    throw Error(code, message);
  }
}

inline void throw_unless(bool condition, ErrorCode code, const std::string& message) {
  if (!condition) {
    throw Error(code, message);
  }
}

} // namespace mr
