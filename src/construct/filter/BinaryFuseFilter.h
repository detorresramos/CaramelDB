#pragma once

#include "BitPackedBinaryFuseFilter.h"
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <memory>
#include <src/construct/SpookyHash.h>
#include <vector>

namespace caramel {

// Utility functions for fingerprint width calculation
namespace BinaryFuseFilterUtils {

// Calculate number of bits needed for given error rate
// For Binary Fuse filters: ε ≈ 1 / (2^bits)
inline int calculateFingerprintBits(float error_rate) {
  if (error_rate <= 0.0f || error_rate >= 1.0f) {
    return 8; // Default to 8 bits
  }
  // bits = -log2(ε)
  int bits = static_cast<int>(std::ceil(-std::log2(error_rate)));
  return std::max(1, std::min(32, bits)); // Clamp to [1, 32]
}

// Choose appropriate fingerprint width based on error rate
// Now returns EXACT bit count (1-32) for optimal space usage!
inline int chooseFingerprintWidth(float error_rate) {
  return calculateFingerprintBits(error_rate);
}

} // namespace BinaryFuseFilterUtils

class BinaryFuseFilter {
public:
  static BinaryFuseFilter create(size_t num_elements,
                                  float error_rate = 0.0039f,
                                  bool verbose = false) {
    BinaryFuseFilter filter;
    filter._num_elements = num_elements;
    filter._error_rate = error_rate;
    filter._fingerprint_width =
        BinaryFuseFilterUtils::chooseFingerprintWidth(error_rate);
    filter._keys.reserve(num_elements);

    if (verbose) {
      std::cout << "BinaryFuseFilter: num_elements=" << num_elements
                << ", target_ε=" << error_rate << ", using "
                << filter._fingerprint_width << "-bit fingerprints"
                << " (actual FPR≈"
                << (1.0f / (1ULL << filter._fingerprint_width)) << ")"
                << std::endl;
    }

    return filter;
  }

  static std::shared_ptr<BinaryFuseFilter> make(size_t num_elements,
                                                 float error_rate = 0.0039f,
                                                 bool verbose = false) {
    return std::make_shared<BinaryFuseFilter>(
        create(num_elements, error_rate, verbose));
  }

  static BinaryFuseFilter createFixed(size_t num_elements, int fingerprint_bits,
                                       bool verbose = false) {
    BinaryFuseFilter filter;
    filter._num_elements = num_elements;
    filter._fingerprint_width = std::max(1, std::min(32, fingerprint_bits));
    filter._error_rate = 1.0f / (1ULL << filter._fingerprint_width);
    filter._keys.reserve(num_elements);

    if (verbose) {
      std::cout << "BinaryFuseFilter (fixed): num_elements=" << num_elements
                << ", fingerprint_bits=" << filter._fingerprint_width
                << " (FPR≈" << filter._error_rate << ")" << std::endl;
    }

    return filter;
  }

  static std::shared_ptr<BinaryFuseFilter> makeFixed(size_t num_elements,
                                                      int fingerprint_bits,
                                                      bool verbose = false) {
    return std::make_shared<BinaryFuseFilter>(
        createFixed(num_elements, fingerprint_bits, verbose));
  }

  void add(const std::string &key) {
    uint64_t hash = hashString(key);
    _keys.push_back(hash);
  }

  void build() {
    if (_keys.empty()) {
      return;
    }

    // Update _num_elements to reflect actual number of keys added
    _num_elements = _keys.size();

    if (_num_elements <= 10) {
      throw std::invalid_argument(
          "BinaryFuseFilter requires more than 10 elements. Got " +
          std::to_string(_num_elements) +
          " elements. Binary fuse filter construction is probabilistic and "
          "fails with very small key counts.");
    }

    // Create filter with appropriate fingerprint width (1-32 bits supported)
    auto filter = std::make_unique<BitPackedBinaryFuseFilter>(_keys.size(), _fingerprint_width);
    auto status = filter->AddAll(_keys.data(), 0, _keys.size());
    if (status != BinaryFuseStatus::Ok) {
      throw std::runtime_error("Failed to build binary fuse filter");
    }
    _binary_fuse_filter = std::move(filter);
    _is_built = true;
  }

  bool contains(const std::string &key) {
    if (!_is_built || !_binary_fuse_filter) {
      return false;
    }

    uint64_t hash = hashString(key);
    auto status = _binary_fuse_filter->Contain(hash);
    return status == BinaryFuseStatus::Ok;
  }

  size_t size() const {
    return _binary_fuse_filter ? _binary_fuse_filter->SizeInBytes() : 0;
  }

  size_t numElements() const { return _num_elements; }

  int fingerprintWidth() const { return _fingerprint_width; }

private:
  uint64_t hashString(const std::string &key) const {
    const void *msgPtr = static_cast<const void *>(key.data());
    size_t length = key.size();
    return SpookyHash::Hash64(msgPtr, length, 0);
  }

  BinaryFuseFilter()
      : _num_elements(0), _error_rate(0.0039f), _fingerprint_width(8),
        _is_built(false) {}

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_num_elements, _error_rate, _fingerprint_width, _is_built);

    if (_is_built && _binary_fuse_filter && _binary_fuse_filter->fingerprints) {
      size_t numWords = _binary_fuse_filter->fingerprints->numWords();
      int bitsPerElement = _binary_fuse_filter->fingerprints->bitsPerElement();
      archive(numWords, bitsPerElement);
      archive(_binary_fuse_filter->arrayLength,
              _binary_fuse_filter->segmentCount,
              _binary_fuse_filter->segmentCountLength,
              _binary_fuse_filter->segmentLength,
              _binary_fuse_filter->segmentLengthMask);
      archive(cereal::binary_data(_binary_fuse_filter->fingerprints->data(),
                                  numWords * sizeof(uint64_t)));
      archive(_binary_fuse_filter->hashIndex);
      archive(_binary_fuse_filter->hasher->seed);
    }
  }

  template <class Archive> void load(Archive &archive) {
    archive(_num_elements, _error_rate, _fingerprint_width, _is_built);

    if (_is_built) {
      size_t numWords;
      int bitsPerElement;
      archive(numWords, bitsPerElement);

      size_t arrayLength, segmentCount, segmentCountLength, segmentLength, segmentLengthMask;
      archive(arrayLength, segmentCount, segmentCountLength, segmentLength, segmentLengthMask);

      auto filter = std::make_unique<BitPackedBinaryFuseFilter>(_num_elements, _fingerprint_width);

      // Override calculated values with saved values
      filter->arrayLength = arrayLength;
      filter->segmentCount = segmentCount;
      filter->segmentCountLength = segmentCountLength;
      filter->segmentLength = segmentLength;
      filter->segmentLengthMask = segmentLengthMask;

      // Load fingerprint data
      archive(cereal::binary_data(filter->fingerprints->data(),
                                  numWords * sizeof(uint64_t)));
      archive(filter->hashIndex);
      archive(filter->hasher->seed);

      filter->size = _num_elements;
      _binary_fuse_filter = std::move(filter);
    }
  }

  std::unique_ptr<BitPackedBinaryFuseFilter> _binary_fuse_filter;
  std::vector<uint64_t> _keys;
  size_t _num_elements;
  float _error_rate;
  int _fingerprint_width;
  bool _is_built;
};

using BinaryFuseFilterPtr = std::shared_ptr<BinaryFuseFilter>;

} // namespace caramel
