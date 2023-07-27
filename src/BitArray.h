#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

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

  static std::shared_ptr<BitArray> make(uint32_t num_bits) {
    return std::make_shared<BitArray>(num_bits);
  }

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
  void clearAll() { std::fill_n(_backing_array, _num_bytes, 0); }

  static bool scalarProduct(const BitArrayPtr &bitarray1,
                            const BitArrayPtr &bitarray2);

  std::string str() const;

  ~BitArray() { delete[] _backing_array; }

private:
  uint32_t _num_bits;
  uint32_t _num_bytes;
  unsigned char *_backing_array;
};

} // namespace caramel