#pragma once

#include <cstdint>
#include <optional>

namespace caramel {

// TODO: should we just typedef a char* and have static methods?
class BitArray {
public:
  BitArray(uint32_t num_bits) : _num_bits(num_bits) {}

  // get at index
  uint32_t operator[](uint32_t index) const;

  // set at index
  uint32_t &operator[](uint32_t index);

  BitArray operator^(const BitArray &other) const;

  BitArray operator&(const BitArray &other) const;

  // index of first nonzero bit, std::nullopt otherwise
  std::optional<uint32_t> find(uint32_t var) const;

  // true if any bits are 1 and false otherwise
  bool any() const;

  // set all bits to 1
  void setAll();

  // set all bits to 0
  void clearAll();

private:
  uint32_t _num_bits;
};

uint32_t scalarProduct(const BitArray &bitarray1, const BitArray &bitarray2);

} // namespace caramel