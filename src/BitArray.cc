#include "BitArray.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <climits>
#include <stdexcept>

namespace caramel {

BitArray::BitArray(uint32_t num_bits) : _num_bits(num_bits) {
  if (num_bits < 1) {
    throw std::invalid_argument("Error: Bit Array must have at least 1 bit.");
  }

  _num_bytes = BITS_TO_CHARS(num_bits);

  _backing_array = std::vector<unsigned char>(_num_bytes, 0);
}

BitArray::BitArray(const BitArray &other) {
  _num_bits = other._num_bits;
  _num_bytes = other._num_bytes;
  _backing_array = other._backing_array;
}

std::shared_ptr<BitArray> BitArray::fromNumber(uint32_t number,
                                               uint32_t length) {
  if (length == 0) {
    throw std::invalid_argument("Length must not be 0.");
  }

  uint32_t one = 1;
  if (number >= (one << length)) {
    throw std::invalid_argument("int_value must be between 0 and " +
                                std::to_string(1 << length) + ", got " +
                                std::to_string(number) + ".");
  }

  BitArrayPtr array = make(length);

  if (number == 0) {
    return array;
  }

  for (uint32_t i = 0; i < length; i++) {
    // Get the ith bit by shifting right i bits and masking with 1
    uint32_t bit = (number >> i) & 1U;
    if (bit) {
      array->setBit(length - i - 1);
    }
  }

  return array;
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

BitArray BitArray::operator&(const BitArray &other) const {
  BitArray result(this->numBits());
  result = *this;
  result &= other;

  return result;
}

BitArray &BitArray::operator=(const BitArray &other) {
  if (*this == other) {
    /* don't do anything for a self assignment */
    return *this;
  }

  if (_num_bits != other.numBits()) {
    /* don't do assignment with different array sizes */
    return *this;
  }

  if ((_num_bits == 0) || (other.numBits() == 0)) {
    /* don't do assignment with unallocated array */
    return *this;
  }

  _backing_array = other._backing_array;

  return *this;
}

bool BitArray::operator==(const BitArray &other) const {
  if (_num_bits != other.numBits()) {
    /* unequal sizes */
    return false;
  }

  return (_backing_array == other._backing_array);
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

void BitArray::setAll() {
  /* set bits in all bytes to 1 */
  std::fill(_backing_array.begin(), _backing_array.end(), UCHAR_MAX);

  /* zero any spare bits so increment and decrement are consistent */
  int bits = _num_bits % CHAR_BIT;
  if (bits != 0) {
    unsigned char mask = UCHAR_MAX << (CHAR_BIT - bits);
    _backing_array[BIT_CHAR(_num_bits - 1)] = mask;
  }
}

uint32_t BitArray::numSetBits() const {
  uint32_t total = 0;
  for (auto block : _backing_array) {
    total += _set_bits_in_char[block];
  }
  return total;
}

bool BitArray::scalarProduct(const BitArrayPtr &bitarray1,
                             const BitArrayPtr &bitarray2) {
  if (bitarray1->numBits() != bitarray2->numBits()) {
    throw std::invalid_argument(
        "scalarProduct recieved two bitarrays of different sizes.");
  }

  auto temp_result = (*bitarray1) & (*bitarray2);

  return temp_result.numSetBits() % 2;
}

std::string BitArray::str() const {
  std::string output;
  for (uint32_t bit = 0; bit < _num_bits; bit++) {
    output += std::to_string((*this)[bit]);
  }
  return output;
}

template void BitArray::serialize(cereal::BinaryInputArchive &);
template void BitArray::serialize(cereal::BinaryOutputArchive &);

template <class Archive> void BitArray::serialize(Archive &archive) {
  archive(_num_bits, _num_bytes, _backing_array);
}

} // namespace caramel