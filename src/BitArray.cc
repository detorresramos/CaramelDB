#include "BitArray.h"

namespace caramel {

BitArray::BitArray(uint32_t num_bits) : _num_bits(num_bits) {
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

// get at index
void BitArray::clearBit(uint32_t index) {
  if (_num_bits <= index) {
    throw std::invalid_argument(
        "Index out of range for clearBit: " + std::to_string(index) + ".");
  }
  unsigned char mask = BIT_IN_CHAR(index);
  mask = ~mask;

  _backing_array[BIT_CHAR(index)] &= mask;
}

// sets bit at index to 1
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
    if (byte != 0) {
      for (uint32_t bit = 0; bit < CHAR_BIT; bit++) {
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

void BitArray::setAll() {
  std::fill_n(_backing_array, _num_bytes, UCHAR_MAX);

  // TODO: DONT UNDERSTAND THIS CODE HERE
  // /* zero any spare bits so increment and decrement are consistent */
  // bits = m_NumBits % CHAR_BIT;
  // if (bits != 0) {
  //   mask = UCHAR_MAX << (CHAR_BIT - bits);
  //   m_Array[BIT_CHAR(m_NumBits - 1)] = mask;
  // }
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