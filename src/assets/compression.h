#pragma once
// Multi-pass RLE/VLE decompression used by Stunts .PRE/.PVS/.PES/.COD files.
//
// A compressed file starts with a 4-byte header:
//    u8  passes_or_type   (bit7 set => multi-pass; low7 = pass count)
//    u16 sizel
//    u8  sizeh            final_size = sizel | (sizeh << 16)
//
// If multi-pass, the pass stream begins at +4 and each pass output is itself a
// compressed stream for the next pass. Each pass begins with its own 4-byte
// header whose first byte is the type: 1 = RLE, 2 = VLE.
//
// Reference: restunts fileio.c (file_decomp_rle/vle) and the format notes in
// docs/formats.md. Every shipping file uses VLE, and .PVS/.PES use two passes.

#include <cstdint>
#include <string>
#include <vector>

namespace s3d {

// Decompresses `data` into `out`. Returns false on malformed input.
bool decompress(const uint8_t* data, size_t size, std::vector<uint8_t>* out);

// Reads and decompresses a whole file.
bool decompress_file(const std::string& path, std::vector<uint8_t>* out);

}  // namespace s3d
