#include "BitArray.h"
#include <bit>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <climits>
#include <stdexcept>
#ifdef __aarch64__
#include <sse2neon/sse2neon.h>
#else
#include <xmmintrin.h>
#endif

namespace caramel {

BitArray::BitArray(uint32_t num_bits) : _num_bits(num_bits) {
  if (num_bits < 1) {
    throw std::invalid_argument("Error: Bit Array must have at least 1 bit.");
  }

  _num_blocks = ((num_bits - 1) / BLOCK_SIZE) + 1;

  _backing_array = std::vector<uint64_t>(_num_blocks, 0);
}

BitArray::BitArray(const BitArray &other) {
  _num_bits = other._num_bits;
  _backing_array = other._backing_array;
}

std::shared_ptr<BitArray> BitArray::fromNumber(uint64_t number,
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
  array->_backing_array[0] = number << (BLOCK_SIZE - length);
  return array;
}

void BitArray::clearBit(uint32_t index) {
  if (_num_bits <= index) {
    throw std::invalid_argument(
        "Index out of range for clearBit: " + std::to_string(index) + ".");
  }
  uint64_t mask = BIT_IN_BLOCK(index);
  mask = ~mask;

  _backing_array[BIT_BLOCK(index)] &= mask;
}

void BitArray::setBit(uint32_t index) {
  if (_num_bits <= index) {
    throw std::invalid_argument(
        "Index out of range for setBit: " + std::to_string(index) + ".");
  }

  _backing_array[BIT_BLOCK(index)] |= BIT_IN_BLOCK(index);
}

void BitArray::flipBit(uint32_t index) {
  if (_num_bits <= index) {
    throw std::invalid_argument(
        "Index out of range for setBit: " + std::to_string(index) + ".");
  }

  _backing_array[BIT_BLOCK(index)] ^= BIT_IN_BLOCK(index);
}

BitArray &BitArray::operator^=(const BitArray &other) {
  if (other._num_bits != this->_num_bits) {
    throw std::invalid_argument(
        "Trying to ^ two BitArrays of different sizes.");
  }

  size_t i = 0;
  for (; i + 1 < _num_blocks; i += 2) {
    __m128i a =
        _mm_loadu_si128(reinterpret_cast<const __m128i *>(&_backing_array[i]));
    __m128i b = _mm_loadu_si128(
        reinterpret_cast<const __m128i *>(&other._backing_array[i]));
    __m128i result = _mm_xor_si128(a, b);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(&_backing_array[i]), result);
  }

  // Handle the last element if the total number is odd
  if (i < _num_blocks) {
    _backing_array[i] ^= other._backing_array[i];
  }

  return *this;
}

BitArray &BitArray::operator&=(const BitArray &other) {
  if (other._num_bits != this->_num_bits) {
    throw std::invalid_argument(
        "Trying to ^ two BitArrays of different sizes.");
  }

  size_t i = 0;
  for (; i + 1 < _num_blocks; i += 2) {
    __m128i a =
        _mm_loadu_si128(reinterpret_cast<const __m128i *>(&_backing_array[i]));
    __m128i b = _mm_loadu_si128(
        reinterpret_cast<const __m128i *>(&other._backing_array[i]));
    __m128i result = _mm_and_si128(a, b);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(&_backing_array[i]), result);
  }

  // Handle the last element if the total number is odd
  if (i < _num_blocks) {
    _backing_array[i] &= other._backing_array[i];
  }

  return *this;
}

BitArray BitArray::operator^(const BitArray &other) const {
  BitArray result(this->numBits());
  result = *this;
  result ^= other;

  return result;
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
  _num_bits = other._num_bits;
  _num_blocks = other._num_blocks;

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
  for (uint32_t block = 0; block < _num_blocks; block++) {
    if (_backing_array[block] != 0) {
      return location + std::__countl_zero<uint64_t>(_backing_array[block]);
    } else {
      location += BLOCK_SIZE;
    }
  }
  return std::nullopt;
}

void BitArray::setAll() {
  /* set bits in all bytes to 1 */
  std::fill(_backing_array.begin(), _backing_array.end(), UINT64_MAX);

  /* zero any spare bits so increment and decrement are consistent */
  int bits = _num_bits % BLOCK_SIZE;
  if (bits != 0) {
    uint64_t mask = UINT64_MAX << (BLOCK_SIZE - bits);
    _backing_array[BIT_BLOCK(_num_bits - 1)] = mask;
  }
}

uint32_t BitArray::numSetBits() const {
  uint32_t total = 0;
  for (auto &block : _backing_array) {
    total += __builtin_popcountll(block);
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
  archive(_num_bits, _backing_array);
}

} // namespace caramel