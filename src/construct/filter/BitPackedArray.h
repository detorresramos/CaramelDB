#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace caramel {

/**
 * Bit-packed array for storing n-bit fingerprints (1-32 bits per element).
 * Optimized for fast random access with minimal memory overhead.
 *
 * Storage format: Fingerprints are packed into 64-bit words with no padding.
 * Example: 5-bit fingerprints are stored as: [fp0][fp1][fp2]...[fp12][fp13][--]
 *          where each [fpN] is 5 bits and [--] is unused padding.
 */
class BitPackedArray {
public:
  /**
   * Create a bit-packed array for storing n-bit values.
   * @param num_elements Number of elements to store
   * @param bits_per_element Bits per element (1-32)
   */
  BitPackedArray(size_t num_elements, int bits_per_element)
      : _num_elements(num_elements), _bits_per_element(bits_per_element) {

    if (bits_per_element < 1 || bits_per_element > 32) {
      throw std::invalid_argument("bits_per_element must be in [1, 32]");
    }

    // Calculate number of 64-bit words needed
    size_t total_bits = num_elements * bits_per_element;
    size_t num_words = (total_bits + 63) / 64;  // Round up

    _data.resize(num_words, 0);  // Initialize to zero
    _mask = (1ULL << bits_per_element) - 1;  // Bitmask for extracting values
  }

  /**
   * Get the value at the given index.
   * @param index Element index
   * @return Value at index (fits in bits_per_element bits)
   */
  inline uint64_t get(size_t index) const {
    size_t bit_pos = index * _bits_per_element;
    size_t word_index = bit_pos >> 6;  // Divide by 64
    size_t bit_offset = bit_pos & 63;  // Modulo 64

    // Extract value from current word
    uint64_t value = (_data[word_index] >> bit_offset) & _mask;

    // Handle case where value spans two words
    size_t bits_in_first_word = 64 - bit_offset;
    if (_bits_per_element > bits_in_first_word) {
      // Need bits from next word
      size_t bits_from_next = _bits_per_element - bits_in_first_word;
      uint64_t next_bits = _data[word_index + 1] & ((1ULL << bits_from_next) - 1);
      value |= (next_bits << bits_in_first_word);
    }

    return value;
  }

  /**
   * Set the value at the given index.
   * @param index Element index
   * @param value Value to store (must fit in bits_per_element bits)
   */
  inline void set(size_t index, uint64_t value) {
    value &= _mask;  // Ensure value fits in allocated bits

    size_t bit_pos = index * _bits_per_element;
    size_t word_index = bit_pos >> 6;
    size_t bit_offset = bit_pos & 63;

    // Clear old value and set new value in current word
    uint64_t clear_mask = ~(_mask << bit_offset);
    _data[word_index] = (_data[word_index] & clear_mask) | (value << bit_offset);

    // Handle case where value spans two words
    size_t bits_in_first_word = 64 - bit_offset;
    if (_bits_per_element > bits_in_first_word) {
      // Write remaining bits to next word
      size_t bits_from_next = _bits_per_element - bits_in_first_word;
      uint64_t next_clear_mask = ~((1ULL << bits_from_next) - 1);
      _data[word_index + 1] = (_data[word_index + 1] & next_clear_mask)
                            | (value >> bits_in_first_word);
    }
  }

  /**
   * XOR a value with the existing value at the given index.
   * Useful for XOR filter construction.
   * @param index Element index
   * @param value Value to XOR with
   */
  inline void xorAt(size_t index, uint64_t value) {
    uint64_t current = get(index);
    set(index, current ^ value);
  }

  /**
   * Get number of elements in the array.
   */
  size_t size() const { return _num_elements; }

  /**
   * Get bits per element.
   */
  int bitsPerElement() const { return _bits_per_element; }

  /**
   * Get size in bytes.
   */
  size_t sizeInBytes() const { return _data.size() * sizeof(uint64_t); }

  /**
   * Get raw data pointer (for serialization).
   */
  const uint64_t* data() const { return _data.data(); }

  /**
   * Get raw data pointer (for deserialization).
   */
  uint64_t* data() { return _data.data(); }

  /**
   * Get number of 64-bit words used.
   */
  size_t numWords() const { return _data.size(); }

private:
  std::vector<uint64_t> _data;  // Storage in 64-bit words
  size_t _num_elements;         // Number of elements
  int _bits_per_element;        // Bits per element
  uint64_t _mask;               // Mask for extracting values
};

} // namespace caramel
