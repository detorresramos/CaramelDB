#pragma once

#include <algorithm>
#include <cereal/access.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace caramel {

/* make CHAR_BIT 8 if it's not defined in limits.h */
#ifndef CHAR_BIT
#warning CHAR_BIT not defined.  Assuming 8 bits.
#define CHAR_BIT 8
#endif

/* array index for character containing bit */
#define BIT_CHAR(bit) ((bit) / CHAR_BIT)

/* position of bit within character */
#define BIT_IN_CHAR(bit) (1 << (CHAR_BIT - 1 - ((bit) % CHAR_BIT)))

/* number of characters required to contain number of bits */
#define BITS_TO_CHARS(bits) ((((bits)-1) / CHAR_BIT) + 1)

class BitArray;
using BitArrayPtr = std::shared_ptr<BitArray>;

class BitArray {
public:
  BitArray(uint32_t num_bits);

  BitArray(const BitArray &other);

  static std::shared_ptr<BitArray> make(uint32_t num_bits) {
    return std::make_shared<BitArray>(num_bits);
  }

  static std::shared_ptr<BitArray> fromNumber(uint32_t int_value,
                                              uint32_t length);

  uint32_t numBits() const { return _num_bits; }

  bool operator[](const uint32_t index) const {
    return ((_backing_array[BIT_CHAR(index)] & BIT_IN_CHAR(index)) != 0);
  }

  void clearBit(uint32_t index);

  void setBit(uint32_t index);

  BitArray &operator^=(const BitArray &other);

  BitArray &operator&=(const BitArray &other);

  BitArray operator&(const BitArray &other) const;

  BitArray &operator=(const BitArray &other);

  bool operator==(const BitArray &other) const;

  // index of first nonzero bit, std::nullopt otherwise
  // TODO unit test this one
  std::optional<uint32_t> find() const;

  // true if any bits are 1 and false otherwise
  bool any() const { return find().has_value(); }

  void setAll();

  // set all bits to 0
  void clearAll() {
    std::fill(_backing_array.begin(), _backing_array.end(), 0);
  }

  uint32_t numSetBits() const;

  static bool scalarProduct(const BitArrayPtr &bitarray1,
                            const BitArrayPtr &bitarray2);

  std::string str() const;

private:
  // Private constructor for cereal
  BitArray() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive);

  uint32_t _num_bits;
  uint32_t _num_bytes;

  // This is a vector because cereal doesn't allow serialization of pointers
  // (ex. char*). Not sure if this will inhibit adding SIMD operations
  std::vector<unsigned char> _backing_array;

  static const std::vector<int> _set_bits_in_char;
};

inline const std::vector<int> BitArray::_set_bits_in_char = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
    3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

} // namespace caramel