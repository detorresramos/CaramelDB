#pragma once

#include <src/BitArray.h>
#include <unordered_map>
#include <vector>

namespace caramel {

using CodeDict = std::unordered_map<uint32_t, BitArrayPtr>;

std::tuple<CodeDict, std::vector<uint32_t>, std::vector<uint32_t>>
cannonicalHuffman(const std::vector<uint32_t> &symbols,
                  std::optional<uint32_t> length_limit = 20,
                  float entropy_threshold = 0.999);

uint32_t cannonicalDecode(const BitArrayPtr &bitarray,
                          const std::vector<uint32_t> &code_length_counts,
                          const std::vector<uint32_t> &symbols);

} // namespace caramel