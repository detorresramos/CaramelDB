#pragma once

#include "SpookyHash.h"
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <memory>
#include <src/BitArray.h>
#include <vector>

namespace caramel {

class BloomFilter {
public:
  BloomFilter(size_t num_elements, float error_rate) {
    // TODO(david) validate formula and check rounding stuff
    std::cout << "num_elements " << num_elements << std::endl;
    std::cout << "error_rate " << error_rate << std::endl;

    size_t size = std::ceil((1 / log(2)) * log2(1.0 / error_rate) *
                            static_cast<float>(num_elements));

    std::cout << "size " << size << std::endl;

    _bitarray = BitArray::make(size);

    std::cout << (static_cast<float>(size) * log(2)) /
                     (static_cast<float>(num_elements))
              << std::endl;

    _num_hashes = std::floor((static_cast<float>(size) * log(2)) /
                            (static_cast<float>(num_elements)));

    std::cout << "_num_hashes " << _num_hashes << std::endl;
  }

  static std::shared_ptr<BloomFilter> make(size_t num_elements,
                                           double error_rate) {
    return std::make_shared<BloomFilter>(num_elements, error_rate);
  }

  void add(const std::string &key) {
    std::vector<uint64_t> hash_values = getHashValues(key);
    for (uint64_t hash : hash_values) {
      _bitarray->setBit(hash % _bitarray->numBits());
    }
  }

  bool contains(const std::string &key) {
    std::vector<uint64_t> hash_values = getHashValues(key);
    for (uint64_t hash : hash_values) {
      if (!(*_bitarray)[hash]) {
        return false;
      }
    }
    return true;
  }

  size_t size() const { return _bitarray->numBits(); }

private:
  std::vector<uint64_t> getHashValues(const std::string &key) {
    std::vector<uint64_t> hash_values;
    hash_values.reserve(_num_hashes);
    for (size_t i = 0; i < _num_hashes; i++) {
      uint64_t hash = hashWithSeed(key, i);
      hash_values.push_back(hash % size());
    }
    return hash_values;
  }

  uint64_t hashWithSeed(const std::string &key, uint64_t seed) {
    const void *msgPtr = static_cast<const void *>(key.data());
    size_t length = key.size();
    return SpookyHash::Hash64(msgPtr, length, seed);
  }

  // Private constructor for cereal
  BloomFilter() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive) {
    archive(_bitarray, _num_hashes);
  }

  BitArrayPtr _bitarray;
  size_t _num_hashes;
};

using BloomFilterPtr = std::shared_ptr<BloomFilter>;

} // namespace caramel
