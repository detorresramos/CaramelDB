#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

namespace caramel {

#ifndef CHAR_BIT
#warning CHAR_BIT not defined.  Assuming 8 bits.
#define CHAR_BIT 8
#endif

#define BIT_CHAR(bit) ((bit) / CHAR_BIT)

#define BIT_IN_CHAR(bit) (1 << (CHAR_BIT - 1 - ((bit) % CHAR_BIT)))

// TODO: should we just typedef a char* and have static methods?
// TODO move implementations to .cc file
class BitArray {
public:
  BitArray(uint32_t num_bits);

  uint32_t numBits() const { return _num_bits; }

  bool operator[](const uint32_t index) const {
    return ((_backing_array[BIT_CHAR(index)] & BIT_IN_CHAR(index)) != 0);
  }

  // get at index
  void clearBit(uint32_t index) const;

  // sets bit at index to 1
  void setBit(uint32_t index);

  BitArray &operator^=(const BitArray &other);

  BitArray &operator&=(const BitArray &other);

  // index of first nonzero bit, std::nullopt otherwise
  // TODO unit test this one
  std::optional<uint32_t> find() const;

  // true if any bits are 1 and false otherwise
  bool any() const { return find().has_value(); }

  // set all bits to 1
  void setAll();

  // set all bits to 0
  void clearAll() { std::fill_n(_backing_array, _num_bytes, 0); }

  ~BitArray() { delete[] _backing_array; }

private:
  uint32_t _num_bits;
  uint32_t _num_bytes;
  unsigned char *_backing_array;
};

uint32_t scalarProduct(const BitArray &bitarray1, const BitArray &bitarray2);

} // namespace caramel