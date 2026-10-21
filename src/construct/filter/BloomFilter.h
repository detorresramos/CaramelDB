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

  void add(const std::string &key) { add(key.data(), key.size()); }

  void add(const char *data, size_t length) {
    addHash(SpookyHash::Hash64(static_cast<const void *>(data), length, 0));
  }

  // Probe i is h + i * step (Kirsch-Mitzenmacher double hashing), so the
  // filter is built from and queried with one 64-bit key hash.
  void addHash(uint64_t key_hash) {
    const uint64_t m = size();
    const uint64_t step = probeStep(key_hash);
    for (size_t i = 0; i < _num_hashes; i++) {
      _bitarray->setBit((key_hash + i * step) % m);
    }
  }

  bool contains(const std::string &key) const {
    return contains(key.data(), key.size());
  }

  bool contains(const char *data, size_t length) const {
    return containsHash(
        SpookyHash::Hash64(static_cast<const void *>(data), length, 0));
  }

  bool containsHash(uint64_t key_hash) const {
    const uint64_t m = size();
    const uint64_t step = probeStep(key_hash);
    for (size_t i = 0; i < _num_hashes; i++) {
      if (!(*_bitarray)[(key_hash + i * step) % m]) {
        return false;
      }
    }
    return true;
  }

  size_t size() const { return _bitarray->numBits(); }

  size_t numHashes() const { return _num_hashes; }

private:
  static uint64_t probeStep(uint64_t key_hash) {
    uint64_t h = key_hash;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    return h | 1;
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
