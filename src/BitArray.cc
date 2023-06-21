#include "BitArray.h"

namespace caramel {

BitArray::BitArray(uint32_t num_bits) : _num_bits(num_bits) {
  if (num_bits < 1) {
    throw std::invalid_argument("Error: Bit Array must have at least 1 bit.");
  }

  _num_bytes = BITS_TO_CHARS(num_bits);

  /* allocate space for bit array */
  _backing_array = new unsigned char[_num_bytes];

  clearAll();
}

void BitArray::clearBit(uint32_t index) {
  if (_num_bits <= index) {
    throw std::invalid_argument(
        "Index out of range for clearBit: " + std::to_string(index) + ".");
  }
  unsigned char mask = BIT_IN_CHAR(index);
  mask = ~mask;

  _backing_array[BIT_CHAR(index)] &= mask;
}

void BitArray::setBit(uint32_t index) {
  if (_num_bits <= index) {
    throw std::invalid_argument(
        "Index out of range for setBit: " + std::to_string(index) + ".");
  }

  _backing_array[BIT_CHAR(index)] |= BIT_IN_CHAR(index);
}

BitArray &BitArray::operator^=(const BitArray &other) {
  if (other.numBits() != _num_bits) {
    throw std::invalid_argument("Trying to & two BitArray of different sizes.");
  }

  for (uint32_t i = 0; i < _num_bytes; i++) {
    _backing_array[i] = _backing_array[i] ^ other._backing_array[i];
  }

  return *this;
}

BitArray &BitArray::operator&=(const BitArray &other) {
  if (other.numBits() != _num_bits) {
    throw std::invalid_argument("Trying to & two BitArray of different sizes.");
  }

  for (uint32_t i = 0; i < _num_bytes; i++) {
    _backing_array[i] = _backing_array[i] & other._backing_array[i];
  }

  return *this;
}

std::optional<uint32_t> BitArray::find() const {
  uint32_t location = 0;
  for (uint32_t byte = 0; byte < _num_bytes; byte++) {
    if (_backing_array[byte] != 0) {
      auto temp = _backing_array[byte];
      for (uint32_t bit = 0; bit < CHAR_BIT; bit++) {
        temp >>= 1;
        if (temp == 0) {
          // we invert the position here within the byte because of endianness
          return location + CHAR_BIT - 1 - bit;
        }
      }
    } else {
      location += CHAR_BIT;
    }
  }
  return std::nullopt;
}

bool BitArray::scalarProduct(const BitArray &bitarray1,
                             const BitArray &bitarray2) {
  if (bitarray1.numBits() != bitarray2.numBits()) {
    throw std::invalid_argument(
        "scalarProduct recieved two bitarrays of different sizes.");
  }

  bool product = false;
  for (uint32_t bit = 0; bit < bitarray1.numBits(); bit++) {
    product ^ bitarray1[bit] && bitarray2[bit];
  }

  return product;
}

} // namespace caramel