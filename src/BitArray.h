#pragma once

#include <algorithm>
#include <cereal/access.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace caramel {

#define BLOCK_SIZE 64

/* array index for block containing bit */
#define BIT_BLOCK(bit) ((bit) / BLOCK_SIZE)

/* position of bit within character */
#define BIT_IN_BLOCK(bit) (1ULL << (BLOCK_SIZE - 1ULL - (bit % BLOCK_SIZE)))

class BitArray;
using BitArrayPtr = std::shared_ptr<BitArray>;

class BitArray {
public:
  BitArray(uint32_t num_bits);

  BitArray(const BitArray &other);

  BitArray(uint64_t *backing_array, uint64_t num_bits, uint64_t num_blocks);

  static BitArray fromNumber(uint64_t int_value, uint32_t length);

  static std::shared_ptr<BitArray> make(uint32_t num_bits) {
    return std::make_shared<BitArray>(num_bits);
  }

  inline uint32_t numBits() const { return _num_bits; }

  bool operator[](const uint32_t index) const {
    return ((_backing_array[BIT_BLOCK(index)] & BIT_IN_BLOCK(index)) != 0);
  }

  void clearBit(uint32_t index);

  void setBit(uint32_t index);

  void flipBit(uint32_t index);

  BitArray &operator^=(const BitArray &other);

  BitArray &operator&=(const BitArray &other);

  BitArray operator^(const BitArray &other) const;

  BitArray operator&(const BitArray &other) const;

  BitArray &operator=(const BitArray &other);

  bool operator==(const BitArray &other) const;

  // index of first nonzero bit, std::nullopt otherwise
  std::optional<uint32_t> find() const;

  // true if any bits are 1 and false otherwise
  bool any() const {
    return !std::all_of(_backing_array, _backing_array + _num_blocks,
                        [](uint64_t block) { return block == 0; });
  }

  void setAll();

  // set all bits to 0
  void clearAll() {
    std::fill(_backing_array, _backing_array + _num_blocks, 0);
  }

  uint32_t numSetBits() const;

  static bool scalarProduct(const BitArray &bitarray1,
                            const BitArray &bitarray2);

  inline uint64_t getuint64(uint32_t pos, uint32_t width) const {
    if (pos + width > _num_bits) {
      throw std::invalid_argument("Cannot get slice starting at pos " +
                                  std::to_string(pos) + " of width " +
                                  std::to_string(width) + " in bitarray of " +
                                  std::to_string(_num_bits) + " bits.");
    }

    const int l = 64 - width;
    const uint64_t start_word = pos / 64;
    const int start_bit = pos % 64;
    if (start_bit <= l) {
      return _backing_array[start_word] << start_bit >> l;
    }
    return _backing_array[start_word] << start_bit >> l |
           _backing_array[start_word + 1] >> (128 - width - start_bit);
  }

  std::string str() const;

  std::vector<uint64_t> backingArray() const {
    std::vector<uint64_t> vector(_backing_array, _backing_array + _num_blocks);
    return vector;
  }

  ~BitArray() noexcept;

private:
  // Private constructor for cereal
  BitArray() {}

  void copyFrom(const BitArray &other);

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const;

  template <class Archive> void load(Archive &archive);

  uint32_t _num_bits;
  uint32_t _num_blocks;
  bool _owns_data;
  uint64_t *_backing_array;
};

} // namespace caramel