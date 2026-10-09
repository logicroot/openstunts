#include "assets/res_file.h"

#include <fstream>

#include "assets/binary_reader.h"
#include "core/log.h"

namespace s3d {

bool ResFile::load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    S3D_ERROR("RES: cannot open '%s'", path.c_str());
    return false;
  }
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
  return load(buf.data(), buf.size());
}

bool ResFile::load(const uint8_t* data, size_t size) {
  data_.clear();
  chunks_.clear();
  file_size_ = 0;
  data_offset_ = 0;

  if (data == nullptr || size < 6) {
    S3D_ERROR("RES: buffer too small (%zu bytes)", size);
    return false;
  }

  ByteReader r(data, size);
  file_size_ = r.u32();
  const uint16_t count = r.u16();
  if (!r.ok() || count == 0 || count > 256) {
    S3D_ERROR("RES: invalid chunk count %u", count);
    return false;
  }

  std::vector<std::string> ids(count);
  for (uint16_t i = 0; i < count; ++i) {
    char id[5] = {0, 0, 0, 0, 0};
    for (int c = 0; c < 4; ++c) id[c] = static_cast<char>(r.u8());
    ids[i] = std::string(id);
  }

  std::vector<uint32_t> offsets(count);
  for (uint16_t i = 0; i < count; ++i) offsets[i] = r.u32();
  if (!r.ok()) {
    S3D_ERROR("RES: truncated header");
    return false;
  }

  data_offset_ = 6u + static_cast<uint32_t>(count) * 8u;
  if (file_size_ > size) file_size_ = static_cast<uint32_t>(size);

  chunks_.resize(count);
  for (uint16_t i = 0; i < count; ++i) {
    // Find the smallest offset strictly greater than this chunk's offset.
    uint32_t next = file_size_ - data_offset_;
    bool found = false;
    for (uint16_t j = 0; j < count; ++j) {
      if (offsets[j] > offsets[i] && (!found || offsets[j] < next)) {
        next = offsets[j];
        found = true;
      }
    }
    chunks_[i].id = ids[i];
    chunks_[i].offset = offsets[i];
    chunks_[i].size = (next > offsets[i]) ? (next - offsets[i]) : 0;
  }

  data_.assign(data, data + size);
  return true;
}

const ResFile::Chunk* ResFile::find(const std::string& id) const {
  for (const auto& c : chunks_) {
    if (c.id == id) return &c;
  }
  return nullptr;
}

std::vector<uint8_t> ResFile::read(const std::string& id) const {
  const Chunk* c = find(id);
  if (c == nullptr) return {};
  const size_t begin = static_cast<size_t>(data_offset_) + c->offset;
  const size_t end = begin + c->size;
  if (end > data_.size()) return {};
  return std::vector<uint8_t>(data_.begin() + begin, data_.begin() + end);
}

}  // namespace s3d
