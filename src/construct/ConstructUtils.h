#pragma once

#include "SpookyHash.h"
#include <array>
#include <functional>

namespace std {
template <std::size_t N> struct hash<std::array<char, N>> {
  size_t operator()(const std::array<char, N> &arr) const {
    size_t hash = 0;
    for (const char &c : arr) {
      hash = hash * 31 + c;
    }
    return hash;
  }
};

} // namespace std