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
  BitArray(uint32_t num_bits) : _num_bits(num_bits) {
    if (num_bits < 1) {
      throw std::invalid_argument("Error: Bit Array must have at least 1 bit.");
    }

    // TODO are there any rounding errors/edge cases we can test
    // TODO make a macro for dividing by 8
    _num_bytes = num_bits / CHAR_BIT;

    /* allocate space for bit array */
    _backing_array = new unsigned char[_num_bytes];

    clearAll();
  }

  uint32_t numBits() const { return _num_bits; }

  bool operator[](const uint32_t index) const {
    return ((_backing_array[BIT_CHAR(index)] & BIT_IN_CHAR(index)) != 0);
  }

  // get at index
  void clearBit(uint32_t index) const {
    if (_num_bits <= index) {
      throw std::invalid_argument(
          "Index out of range for clearBit: " + std::to_string(index) + ".");
    }
    unsigned char mask = BIT_IN_CHAR(index);
    mask = ~mask;

    _backing_array[BIT_CHAR(index)] &= mask;
  }

  // sets bit at index to 1
  void setBit(uint32_t index) {
    if (_num_bits <= index) {
      throw std::invalid_argument(
          "Index out of range for setBit: " + std::to_string(index) + ".");
    }

    _backing_array[BIT_CHAR(index)] |= BIT_IN_CHAR(index);
  }

  BitArray &operator^=(const BitArray &other) {
    if (other.numBits() != _num_bits) {
      throw std::invalid_argument(
          "Trying to & two BitArray of different sizes.");
    }

    for (uint32_t i = 0; i < _num_bytes; i++) {
      _backing_array[i] = _backing_array[i] ^ other._backing_array[i];
    }

    return *this;
  }

  BitArray &operator&=(const BitArray &other) {
    if (other.numBits() != _num_bits) {
      throw std::invalid_argument(
          "Trying to & two BitArray of different sizes.");
    }

    for (uint32_t i = 0; i < _num_bytes; i++) {
      _backing_array[i] = _backing_array[i] & other._backing_array[i];
    }

    return *this;
  }

  // index of first nonzero bit, std::nullopt otherwise
  // TODO unit test this one
  std::optional<uint32_t> find() const {
    uint32_t location = 0;
    for (uint32_t byte = 0; byte < _num_bytes; byte++) {
      if (byte != 0) {
        for (uint32_t i = 0; i < CHAR_BIT; i++) {
          _backing_array[byte] >>= 1;
          if (_backing_array[byte] == 0) {
            return location;
          }
          location++;
        }
      } else {
        location += CHAR_BIT;
      }
    }
    return std::nullopt;
  }

  // true if any bits are 1 and false otherwise
  bool any() const { return find().has_value(); }

  // set all bits to 1
  void setAll() {
    std::fill_n(_backing_array, _num_bytes, UCHAR_MAX);

    // TODO: DONT UNDERSTAND THIS CODE HERE
    // /* zero any spare bits so increment and decrement are consistent */
    // bits = m_NumBits % CHAR_BIT;
    // if (bits != 0) {
    //   mask = UCHAR_MAX << (CHAR_BIT - bits);
    //   m_Array[BIT_CHAR(m_NumBits - 1)] = mask;
    // }
  }

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