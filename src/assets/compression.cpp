#include "assets/compression.h"

#include <cstdio>
#include <cstring>

#include "core/log.h"

namespace s3d {
namespace {

constexpr size_t kComprHeaderSize = 4;
constexpr size_t kRleSubHeaderSize = 5;   // srcsizel(2)+srcsizeh(1)+unk(1)+esclen(1)
constexpr size_t kRleEscSeqPos = 1;
constexpr size_t kVleEscLen = 16;
constexpr size_t kVleAlphLen = 256;
constexpr size_t kVleEscWidth = 0x40;
constexpr unsigned kVleNumSymb = 0x80;
constexpr unsigned kVleExtCodeStartWidth = 7;
constexpr size_t kReadPad = 64;

uint32_t read_size24(const uint8_t* p) {
  return static_cast<uint32_t>(p[1]) | (static_cast<uint32_t>(p[2]) << 8) |
         (static_cast<uint32_t>(p[3]) << 16);
}

// Sequential byte-run pass. Returns the expanded sequence stream.
std::vector<uint8_t> rle_seq(const uint8_t* src, size_t srclen, uint8_t esc) {
  std::vector<uint8_t> out;
  out.reserve(srclen * 2);
  size_t i = 0;
  while (i < srclen) {
    const uint8_t cur = src[i++];
    if (cur == esc) {
      const size_t seqstart = i;
      while (i < srclen && src[i] != esc) out.push_back(src[i++]);
      if (i < srclen) ++i;  // consume closing escape
      if (i >= srclen) break;
      const uint8_t rep = static_cast<uint8_t>(src[i++] - 1);  // extra repeats
      const size_t seqend = i;
      for (uint8_t r = 0; r < rep; ++r) {
        size_t j = seqstart;
        while (j + 2 < seqend) out.push_back(src[j++]);
      }
    } else {
      out.push_back(cur);
    }
  }
  return out;
}

// Single byte-run pass. Expands `len` bytes, truncating any overshoot.
void rle_single(const uint8_t* src, size_t srclen, uint32_t len,
                const uint8_t* esclookup, std::vector<uint8_t>* out) {
  out->clear();
  out->reserve(len);
  size_t i = 0;
  auto read = [&](void) -> uint8_t { return (i < srclen) ? src[i++] : 0; };
  while (out->size() < len) {
    const uint8_t cur = read();
    const uint8_t code = esclookup[cur];
    if (code != 0) {
      if (code == 1) {
        uint8_t rep = read();
        const uint8_t val = read();
        while (rep--) out->push_back(val);
      } else if (code == 3) {
        const uint16_t repw = static_cast<uint16_t>(read() | (read() << 8));
        const uint8_t val = read();
        for (uint16_t r = 0; r < repw; ++r) out->push_back(val);
      } else {
        uint8_t rep = static_cast<uint8_t>(code - 1);
        const uint8_t val = read();
        while (rep--) out->push_back(val);
      }
    } else {
      out->push_back(cur);
    }
  }
  if (out->size() > len) out->resize(len);
}

bool decode_rle(const std::vector<uint8_t>& in, std::vector<uint8_t>* out) {
  if (in.size() < kComprHeaderSize + kRleSubHeaderSize) return false;
  const uint32_t len = read_size24(in.data());
  const uint8_t* sub = in.data() + kComprHeaderSize;
  const uint32_t srclen =
      static_cast<uint32_t>(sub[0]) | (static_cast<uint32_t>(sub[1]) << 8) |
      (static_cast<uint32_t>(sub[2]) << 16);
  uint8_t esclen = sub[4];
  const uint8_t* escapes = sub + kRleSubHeaderSize;

  const bool skipseq = esclen > 0x80;
  esclen &= 0x7f;

  uint8_t esclookup[256];
  std::memset(esclookup, 0, sizeof(esclookup));
  for (uint8_t k = 0; k < esclen; ++k) esclookup[escapes[k]] = static_cast<uint8_t>(k + 1);

  const size_t data_off = kComprHeaderSize + kRleSubHeaderSize + esclen;
  if (data_off > in.size()) return false;
  const uint8_t* data = in.data() + data_off;
  size_t data_len = in.size() - data_off;

  std::vector<uint8_t> seq;
  if (!skipseq) {
    const size_t usable = (srclen < data_len) ? srclen : data_len;
    seq = rle_seq(data, usable, escapes[kRleEscSeqPos]);
    if (len == 0) {
      out->clear();
      return true;
    }
    rle_single(seq.data(), seq.size(), len, esclookup, out);
  } else {
    rle_single(data, data_len, len, esclookup, out);
  }
  return true;
}

struct VleDecoder {
  uint16_t esc_off[16] = {0};
  uint16_t esc_limit[16] = {0};
  uint8_t alphabet[256] = {0};
  uint8_t symbols[256] = {0};
  uint8_t widths[256] = {0};
  bool additive = false;
};

bool decode_vle(const std::vector<uint8_t>& in, std::vector<uint8_t>* out) {
  // Pad the input so the bit reader can over-read the final symbol safely.
  std::vector<uint8_t> buf(in);
  buf.resize(in.size() + kReadPad, 0);

  if (buf.size() < kComprHeaderSize + 1) return false;
  const uint32_t len = read_size24(buf.data());
  size_t p = kComprHeaderSize;

  VleDecoder dec;
  uint8_t esc_len_byte = buf[p++];
  dec.additive = (esc_len_byte & 0x80) != 0;
  const uint8_t esc_len = static_cast<uint8_t>(esc_len_byte & 0x7f);
  if (esc_len > kVleEscLen) return false;

  const size_t wdt_pos = p;
  // Build the canonical code limits and alphabet offsets for each width.
  {
    uint16_t alph_len = 0, code_limit = 0;
    for (size_t i = 0; i < esc_len; ++i) {
      dec.esc_off[i] = static_cast<uint16_t>(alph_len - code_limit);
      const uint8_t count = buf[p++];
      code_limit = static_cast<uint16_t>(code_limit + count);
      alph_len = static_cast<uint16_t>(alph_len + count);
      dec.esc_limit[i] = code_limit;
      code_limit = static_cast<uint16_t>(code_limit * 2);
    }
    for (uint16_t i = 0; i < alph_len; ++i) dec.alphabet[i] = buf[p++];
  }

  // Build the symbol/width lookup from the width data.
  {
    size_t w = wdt_pos;
    unsigned numsymb = kVleNumSymb;
    size_t index = 0, alph_index = 0;
    const unsigned width_distr = (esc_len >= 8) ? 8 : esc_len;
    for (unsigned width = 1; width <= width_distr; ++width, numsymb >>= 1) {
      for (uint8_t symb_width = buf[w++]; symb_width > 0;
           --symb_width, ++alph_index) {
        for (unsigned left = numsymb; left > 0; --left, ++index) {
          if (index >= kVleAlphLen) break;
          dec.symbols[index] = dec.alphabet[alph_index];
          dec.widths[index] = static_cast<uint8_t>(width);
        }
      }
    }
    for (; index < kVleAlphLen; ++index) dec.widths[index] = kVleEscWidth;
  }

  out->clear();
  out->reserve(len);

  uint16_t curword = static_cast<uint16_t>((buf[p] << 8) | buf[p + 1]);
  p += 2;
  uint8_t curwdt = 8;
  uint8_t cursymb = 0;

  uint32_t lenleft = len;
  while (lenleft) {
    uint8_t code = static_cast<uint8_t>(curword >> 8);
    uint8_t nextwdt = dec.widths[code];
    if (nextwdt > 8) {
      code = static_cast<uint8_t>(curword & 0xFF);
      curword >>= 8;
      size_t i = kVleExtCodeStartWidth;
      bool resolved = false;
      while (i < kVleEscLen) {
        if (curwdt == 0) {
          code = buf[p++];
          curwdt = 8;
        }
        curword = static_cast<uint16_t>((curword << 1) | ((code & 0x80) ? 1 : 0));
        code = static_cast<uint8_t>(code << 1);
        --curwdt;
        ++i;
        if (curword < dec.esc_limit[i]) {
          curword = static_cast<uint16_t>(curword + dec.esc_off[i]);
          cursymb = dec.additive
                        ? static_cast<uint8_t>(cursymb + dec.alphabet[curword])
                        : dec.alphabet[curword];
          out->push_back(cursymb);
          --lenleft;
          resolved = true;
          break;
        }
      }
      if (!resolved) return false;
      curword = static_cast<uint16_t>((code << curwdt) | buf[p++]);
      nextwdt = static_cast<uint8_t>(8 - curwdt);
      curwdt = 8;
    } else {
      cursymb = dec.additive
                    ? static_cast<uint8_t>(cursymb + dec.symbols[code])
                    : dec.symbols[code];
      out->push_back(cursymb);
      --lenleft;
      if (curwdt < nextwdt) {
        curword = static_cast<uint16_t>(curword << curwdt);
        nextwdt = static_cast<uint8_t>(nextwdt - curwdt);
        curwdt = 8;
        curword = static_cast<uint16_t>(curword | buf[p++]);
      }
    }
    curword = static_cast<uint16_t>(curword << nextwdt);
    curwdt = static_cast<uint8_t>(curwdt - nextwdt);
  }
  return true;
}

}  // namespace

bool decompress(const uint8_t* data, size_t size, std::vector<uint8_t>* out) {
  if (data == nullptr || size < kComprHeaderSize) return false;

  const uint8_t first = data[0];
  uint32_t passes;
  size_t pos;
  if (first & 0x80) {
    passes = first & 0x7f;
    pos = kComprHeaderSize;
  } else {
    passes = 1;
    pos = 0;
  }
  if (passes == 0) return false;

  std::vector<uint8_t> cur(data + pos, data + size);
  for (uint32_t pass = 0; pass < passes; ++pass) {
    if (cur.empty()) return false;
    std::vector<uint8_t> next;
    bool ok = false;
    switch (cur[0]) {
      case 1: ok = decode_rle(cur, &next); break;
      case 2: ok = decode_vle(cur, &next); break;
      default:
        S3D_ERROR("decompress: invalid pack type %u", cur[0]);
        return false;
    }
    if (!ok) {
      S3D_ERROR("decompress: pass %u failed", pass);
      return false;
    }
    cur.swap(next);
  }
  *out = std::move(cur);
  return true;
}

bool decompress_file(const std::string& path, std::vector<uint8_t>* out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) {
    S3D_ERROR("decompress: cannot open '%s'", path.c_str());
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n < 0) {
    std::fclose(f);
    return false;
  }
  std::vector<uint8_t> buf(static_cast<size_t>(n));
  const size_t got = std::fread(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  if (got != buf.size()) return false;
  return decompress(buf.data(), buf.size(), out);
}

}  // namespace s3d
