#pragma once

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <src/BitArray.h>
#include <src/construct/SpookyHash.h>
#include <vector>

namespace caramel {

class BloomFilter {
public:
  static BloomFilter autotuned(size_t num_elements, double error_rate,
                               bool verbose) {
    BloomFilter filter;

    size_t size =
        std::ceil(log2(std::exp(1)) * log2(std::exp(1)) *
                  log2(1.0 / error_rate) * static_cast<float>(num_elements));

    filter._bitarray = BitArray::make(size);

    float optimal_num_hashes = (static_cast<float>(size) * log(2)) /
                               (static_cast<float>(num_elements));

    filter._num_hashes = std::round(optimal_num_hashes);

    if (verbose) {
      std::cout << std::endl
                << "BloomFilter: size=" << size
                << " bits, optimal_hashes=" << optimal_num_hashes
                << ", num_hashes=" << filter._num_hashes << std::endl;
    }

    return filter;
  }

  static BloomFilter autotunedFixedK(size_t num_elements, double error_rate,
                                     size_t num_hashes, bool verbose = false) {
    BloomFilter filter;

    size_t size =
        std::ceil(log2(std::exp(1)) * log2(std::exp(1)) *
                  log2(1.0 / error_rate) * static_cast<float>(num_elements));

    filter._bitarray = BitArray::make(size);

    filter._num_hashes = num_hashes;

    if (verbose) {
      std::cout << std::endl
                << "BloomFilter: size=" << size
                << " bits, num_hashes=" << filter._num_hashes << " (fixed)"
                << std::endl;
    }

    return filter;
  }

  static BloomFilter fixed(size_t bitarray_size, size_t num_hashes) {
    BloomFilter filter;
    filter._bitarray = BitArray::make(bitarray_size);
    filter._num_hashes = num_hashes;
    return filter;
  }

  static std::shared_ptr<BloomFilter>
  makeAutotuned(size_t num_elements, double error_rate, bool verbose) {
    return std::make_shared<BloomFilter>(
        autotuned(num_elements, error_rate, verbose));
  }

  static std::shared_ptr<BloomFilter> makeFixed(size_t bitarray_size,
                                                size_t num_hashes) {
    return std::make_shared<BloomFilter>(fixed(bitarray_size, num_hashes));
  }

  static std::shared_ptr<BloomFilter> makeAutotunedFixedK(size_t num_elements,
                                                          double error_rate,
                                                          size_t num_hashes,
                                                          bool verbose) {
    return std::make_shared<BloomFilter>(
        autotunedFixedK(num_elements, error_rate, num_hashes, verbose));
  }

  void add(const std::string &key) {
    std::vector<uint64_t> hash_values = getHashValues(key);
    for (uint64_t hash : hash_values) {
      _bitarray->setBit(hash);
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

  size_t numHashes() const { return _num_hashes; }

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

  BitArrayPtr _bitarray = nullptr;
  size_t _num_hashes = 0;
};

using BloomFilterPtr = std::shared_ptr<BloomFilter>;

} // namespace caramel
