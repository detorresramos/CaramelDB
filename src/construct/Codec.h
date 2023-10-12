#pragma once

#include <src/BitArray.h>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T> using CodeDict = std::unordered_map<T, BitArrayPtr>;

template <typename T>
std::tuple<CodeDict<T>, std::vector<uint32_t>, std::vector<T>>
cannonicalHuffman(const std::vector<T> &values);

template <typename T>
T cannonicalDecode(const BitArrayPtr &bitarray,
                   const std::vector<uint32_t> &code_length_counts,
                   const std::vector<T> &symbols);

} // namespace caramel