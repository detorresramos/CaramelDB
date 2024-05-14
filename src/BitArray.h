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
#define BIT_IN_BLOCK(bit) (1ULL << (BLOCK_SIZE - 1ULL - ((bit) % BLOCK_SIZE)))

class BitArray;
using BitArrayPtr = std::shared_ptr<BitArray>;

class BitArray {
public:
  BitArray(uint32_t num_bits);

  BitArray(const BitArray &other);

  static std::shared_ptr<BitArray> make(uint32_t num_bits) {
    return std::make_shared<BitArray>(num_bits);
  }

  static std::shared_ptr<BitArray> fromNumber(uint64_t int_value,
                                              uint32_t length);

  uint32_t numBits() const { return _num_bits; }

  bool operator[](const uint32_t index) const {
    return ((_backing_array[BIT_BLOCK(index)] & BIT_IN_BLOCK(index)) != 0);
  }

  void clearBit(uint32_t index);

  void setBit(uint32_t index);

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
    return !std::all_of(_backing_array.begin(), _backing_array.end(),
                       [](uint64_t block) { return block == 0; });
  }

  void setAll();

  // set all bits to 0
  void clearAll() {
    std::fill(_backing_array.begin(), _backing_array.end(), 0);
  }

  uint32_t numSetBits() const;

  static bool scalarProduct(const BitArrayPtr &bitarray1,
                            const BitArrayPtr &bitarray2);

  uint64_t getuint64(uint32_t from, uint32_t to) const;

  std::string str() const;

  std::vector<uint64_t> backingArray() const { return _backing_array; }

private:
  // Private constructor for cereal
  BitArray() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive);

  uint32_t _num_bits;

  // Use a vector since cereal doesn't allow serialization of pointers
  std::vector<uint64_t> _backing_array;
};

} // namespace caramel