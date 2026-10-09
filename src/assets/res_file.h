#pragma once
// Generic chunked resource container (.RES files, e.g. CAR*.RES).
//
// Layout (little-endian):
//   u32            total file size
//   u16            chunk count N
//   char[4] * N    chunk ids
//   u32    * N     chunk offsets, relative to `data_offset`
//   data_offset = 6 + N * 8
//
// Chunk lengths are implicit: a chunk ends where the next (greater) offset
// begins, or at EOF for the last chunk. Chunks are NOT necessarily stored in
// the order of the id table (e.g. CAR*.RES stores "simd" first).
//
// Reference: reference/restunts/src/resfile/resfile.cpp

#include <cstdint>
#include <string>
#include <vector>

namespace s3d {

class ResFile {
 public:
  struct Chunk {
    std::string id;  // up to 4 characters
    uint32_t offset = 0;
    uint32_t size = 0;
  };

  bool load(const std::string& path);
  bool load(const uint8_t* data, size_t size);

  // All chunks, in id-table order.
  const std::vector<Chunk>& chunks() const { return chunks_; }
  const Chunk* find(const std::string& id) const;

  // Copies a chunk's bytes into a new vector (empty if not found).
  std::vector<uint8_t> read(const std::string& id) const;

  uint32_t file_size() const { return file_size_; }
  uint32_t data_offset() const { return data_offset_; }

 private:
  std::vector<uint8_t> data_;
  std::vector<Chunk> chunks_;
  uint32_t file_size_ = 0;
  uint32_t data_offset_ = 0;
};

}  // namespace s3d
