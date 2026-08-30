#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mr/config.hpp"
#include "mr/core/byte_size.hpp"
#include "mr/core/error.hpp"
#include "mr/core/uid.hpp"

namespace mr {

// ===========================================================================
// BinWriter
// ===========================================================================
/// A little-endian, length-prefixed binary writer. The output is deterministic
/// for a given input, which is a requirement for persistence and protocol
/// frames. Writer does not and must not do any network, file, or device-heavy
/// work.
class BinWriter {
 public:
  BinWriter() = default;

  BinWriter& u8(std::uint8_t v) { push(&v, 1); return *this; }
  BinWriter& u16(std::uint16_t v) { push(&v, 2); return *this; }
  BinWriter& u32(std::uint32_t v) { push(&v, 4); return *this; }
  BinWriter& u64(std::uint64_t v) { push(&v, 8); return *this; }
  BinWriter& i8(std::int8_t v) { push(&v, 1); return *this; }
  BinWriter& i16(std::int16_t v) { push(&v, 2); return *this; }
  BinWriter& i32(std::int32_t v) { push(&v, 4); return *this; }
  BinWriter& i64(std::int64_t v) { push(&v, 8); return *this; }
  BinWriter& f32(float v) { push(&v, 4); return *this; }
  BinWriter& f64(double v) { push(&v, 8); return *this; }

  BinWriter& boolean(bool v) { return u8(v ? 1 : 0); }

  BinWriter& bytes(Bytes v) { return u64(v.value()); }

  BinWriter& uid(const Uid128& id) {
    std::uint8_t raw[16];
    id.to_bytes(raw);
    return raw_bytes(raw, 16);
  }

  BinWriter& generation_u64(std::uint64_t v) { return u64(v); }

  /// Write a length-prefixed string (u32 length, then bytes). The length must
  /// fit in 32 bits.
  BinWriter& string(std::string_view s) {
    throw_if(s.size() > 0xFFFFFFFFull, ErrorCode::OutOfRange, "string length exceeds u32");
    u32(static_cast<std::uint32_t>(s.size()));
    if (!s.empty()) { raw_bytes(s.data(), s.size()); }
    return *this;
  }

  BinWriter& raw_bytes(const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    buf_.insert(buf_.end(), p, p + len);
    return *this;
  }

  [[nodiscard]] std::size_t size() const noexcept { return buf_.size(); }
  [[nodiscard]] const std::uint8_t* data() const noexcept { return buf_.data(); }
  [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return buf_; }

  std::vector<std::uint8_t> take() { return std::move(buf_); }

 private:
  void push(const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    buf_.insert(buf_.end(), p, p + len);
  }
  std::vector<std::uint8_t> buf_;
};

// ===========================================================================
// BinReader
// ===========================================================================
/// A bounded, endian-exact binary reader. Every read is bounds-checked. Reads
/// past the end throw ErrorCode::ProtocolTruncation, which persistence and
/// protocol layers treat as a malformed-frame failure. This is what rejects
/// truncation and malformed lengths. Bad enum/field values are rejected by an
/// optional validator callback at the persistence layer.
class BinReader {
 public:
  BinReader() = default;
  BinReader(const void* data, std::size_t size) : data_(static_cast<const std::uint8_t*>(data)), size_(size) {}

  [[nodiscard]] std::size_t remaining() const noexcept { return size_ - pos_; }
  [[nodiscard]] std::size_t position() const noexcept { return pos_; }
  [[nodiscard]] bool at_end() const noexcept { return pos_ == size_; }

  std::uint8_t u8() { need(1); return data_[pos_++]; }
  std::uint16_t u16() { need(2); return read_le16(); }
  std::uint32_t u32() { need(4); return read_le32(); }
  std::uint64_t u64() { need(8); return read_le64(); }
  std::int8_t i8() { return static_cast<std::int8_t>(u8()); }
  std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
  std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
  std::int64_t i64() { return static_cast<std::int64_t>(u64()); }
  float f32() { need(4); std::uint32_t v = read_le32(); float r; std::memcpy(&r, &v, 4); return r; }
  double f64() { need(8); std::uint64_t v = read_le64(); double r; std::memcpy(&r, &v, 8); return r; }

  std::uint64_t generation_u64() { return u64(); }

  bool boolean() { const std::uint8_t v = u8(); return v != 0; }

  Bytes bytes() { return Bytes(u64()); }

  Uid128 uid() {
    std::uint8_t raw[16];
    raw_bytes(raw, 16);
    return Uid128::from_bytes(raw);
  }

  std::string string() {
    const std::uint32_t len = u32();
    if (static_cast<std::uint64_t>(len) > remaining()) {
      throw Error(ErrorCode::ProtocolTruncation, "string length exceeds remaining bytes");
    }
    std::string result(reinterpret_cast<const char*>(data_ + pos_), len);
    pos_ += len;
    return result;
  }

  void raw_bytes(void* out, std::size_t len) {
    need(len);
    std::memcpy(out, data_ + pos_, len);
    pos_ += len;
  }

  std::span<const std::uint8_t> raw_span(std::size_t len) {
    need(len);
    auto sp = std::span<const std::uint8_t>(data_ + pos_, len);
    pos_ += len;
    return sp;
  }

  /// Strict float read: rejects NaN and infinity, which the persistence layer
  /// must never accept as a coherent value.
  float f32_strict() {
    float v = f32();
    throw_if(!std::isfinite(v), ErrorCode::PersistenceCorruption, "non-finite f32 in payload");
    return v;
  }
  double f64_strict() {
    double v = f64();
    throw_if(!std::isfinite(v), ErrorCode::PersistenceCorruption, "non-finite f64 in payload");
    return v;
  }

 private:
  void need(std::size_t n) {
    if (n > remaining()) {
      throw Error(ErrorCode::ProtocolTruncation, "read past end of buffer");
    }
  }
  std::uint16_t read_le16() {
    const std::uint16_t lo = data_[pos_], hi = data_[pos_ + 1];
    pos_ += 2;
    return static_cast<std::uint16_t>(lo | (hi << 8));
  }
  std::uint32_t read_le32() {
    const std::uint32_t b0 = data_[pos_], b1 = data_[pos_ + 1], b2 = data_[pos_ + 2], b3 = data_[pos_ + 3];
    pos_ += 4;
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
  }
  std::uint64_t read_le64() {
    std::uint64_t r = 0;
    for (int i = 0; i < 8; ++i) { r |= static_cast<std::uint64_t>(data_[pos_ + i]) << (8 * i); }
    pos_ += 8;
    return r;
  }

  const std::uint8_t* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t pos_ = 0;
};

} // namespace mr
