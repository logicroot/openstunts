#pragma once
// Little-endian bounds-checked binary reader for asset parsing.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace s3d {

class ByteReader {
 public:
  ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  size_t pos() const { return pos_; }
  size_t size() const { return size_; }
  size_t remaining() const { return pos_ <= size_ ? size_ - pos_ : 0; }
  bool ok() const { return ok_; }
  bool eof() const { return pos_ >= size_; }

  void seek(size_t p) {
    if (p > size_) {
      fail();
      return;
    }
    pos_ = p;
  }
  void skip(size_t n) {
    if (!have(n)) return;
    pos_ += n;
  }

  uint8_t u8() {
    if (!have(1)) return 0;
    return data_[pos_++];
  }
  int8_t i8() { return static_cast<int8_t>(u8()); }

  uint16_t u16() {
    if (!have(2)) return 0;
    const uint16_t v = static_cast<uint16_t>(data_[pos_]) |
                       (static_cast<uint16_t>(data_[pos_ + 1]) << 8);
    pos_ += 2;
    return v;
  }
  int16_t i16() { return static_cast<int16_t>(u16()); }

  uint32_t u32() {
    if (!have(4)) return 0;
    const uint32_t v = static_cast<uint32_t>(data_[pos_]) |
                       (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
                       (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
                       (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return v;
  }
  int32_t i32() { return static_cast<int32_t>(u32()); }

  float f32() {
    const uint32_t bits = u32();
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
  }

  // Copies n bytes out.
  std::vector<uint8_t> bytes(size_t n) {
    if (!have(n)) return {};
    std::vector<uint8_t> out(data_ + pos_, data_ + pos_ + n);
    pos_ += n;
    return out;
  }

  // Returns a pointer into the buffer and advances; nullptr if out of range.
  const uint8_t* take(size_t n) {
    if (!have(n)) return nullptr;
    const uint8_t* p = data_ + pos_;
    pos_ += n;
    return p;
  }

  bool have(size_t n) {
    if (!ok_) return false;
    if (pos_ + n > size_) {
      fail();
      return false;
    }
    return true;
  }

 private:
  void fail() { ok_ = false; }

  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
  size_t pos_ = 0;
  bool ok_ = true;
};

}  // namespace s3d
