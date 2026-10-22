#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

namespace ro::resource {

// RFC 1950 (zlib) and RFC 1951 (deflate) in-memory decompressor
class Inflater {
public:
    static bool decompress_zlib(std::span<const uint8_t> source, std::vector<uint8_t>& dest);
};

} // namespace ro::resource
