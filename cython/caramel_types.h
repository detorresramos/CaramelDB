#pragma once

#include <array>
#include <memory>
#include <src/construct/filter/BinaryFusePreFilter.h>
#include <src/construct/filter/BloomPreFilter.h>
#include <src/construct/filter/PreFilter.h>
#include <src/construct/filter/XORPreFilter.h>
#include <stdexcept>
#include <string>

using Char10 = std::array<char, 10>;
using Char12 = std::array<char, 12>;

template <typename T>
void savePreFilter(const std::shared_ptr<caramel::PreFilter<T>> &filter,
                   const std::string &filename) {
  if (auto p =
          std::dynamic_pointer_cast<caramel::BloomPreFilter<T>>(filter)) {
    p->save(filename);
  } else if (auto p =
                 std::dynamic_pointer_cast<caramel::XORPreFilter<T>>(filter)) {
    p->save(filename);
  } else if (auto p = std::dynamic_pointer_cast<caramel::BinaryFusePreFilter<T>>(
                 filter)) {
    p->save(filename);
  } else {
    throw std::runtime_error("Unknown filter type, cannot save");
  }
}
