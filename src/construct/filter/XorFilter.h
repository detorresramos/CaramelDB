#pragma once

#include "BitPackedXorFilter.h"
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <memory>
#include <src/construct/SpookyHash.h>
#include <vector>

namespace caramel {

// Utility functions for fingerprint width calculation
namespace XorFilterUtils {

// Calculate number of bits needed for given error rate
// For XOR filters: ε ≈ 1 / (2^bits)
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

} // namespace XorFilterUtils

class XorFilter {
public:
  static XorFilter create(size_t num_elements, float error_rate = 0.0039f,
                          bool verbose = false) {
    XorFilter filter;
    filter._num_elements = num_elements;
    filter._error_rate = error_rate;
    filter._fingerprint_width =
        XorFilterUtils::chooseFingerprintWidth(error_rate);
    filter._keys.reserve(num_elements);

    if (verbose) {
      std::cout << "XorFilter: num_elements=" << num_elements
                << ", target_ε=" << error_rate
                << ", using " << filter._fingerprint_width << "-bit fingerprints"
                << " (actual FPR≈"
                << (1.0f / (1ULL << filter._fingerprint_width)) << ")"
                << std::endl;
    }

    return filter;
  }

  static std::shared_ptr<XorFilter> make(size_t num_elements,
                                         float error_rate = 0.0039f,
                                         bool verbose = false) {
    return std::make_shared<XorFilter>(
        create(num_elements, error_rate, verbose));
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

    // Create filter with appropriate fingerprint width (1-32 bits supported)
    auto filter = std::make_unique<BitPackedXorFilter>(_keys.size(), _fingerprint_width);
    auto status = filter->AddAll(_keys.data(), 0, _keys.size());
    if (status != XorStatus::Ok) {
      throw std::runtime_error("Failed to build xor filter");
    }
    _xor_filter = std::move(filter);
    _is_built = true;
  }

  bool contains(const std::string &key) {
    if (!_is_built || !_xor_filter) {
      return false;
    }

    uint64_t hash = hashString(key);
    auto status = _xor_filter->Contain(hash);
    return status == XorStatus::Ok;
  }

  size_t size() const {
    return _xor_filter ? _xor_filter->SizeInBytes() : 0;
  }

  size_t numElements() const { return _num_elements; }

private:
  uint64_t hashString(const std::string &key) const {
    const void *msgPtr = static_cast<const void *>(key.data());
    size_t length = key.size();
    return SpookyHash::Hash64(msgPtr, length, 0);
  }

  XorFilter()
      : _num_elements(0), _error_rate(0.0039f), _fingerprint_width(8),
        _is_built(false) {}

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_num_elements, _error_rate, _fingerprint_width, _is_built);

    if (_is_built && _xor_filter && _xor_filter->fingerprints) {
      size_t numWords = _xor_filter->fingerprints->numWords();
      int bitsPerElement = _xor_filter->fingerprints->bitsPerElement();
      archive(numWords, bitsPerElement);
      archive(_xor_filter->arrayLength, _xor_filter->blockLength);
      archive(cereal::binary_data(_xor_filter->fingerprints->data(),
                                  numWords * sizeof(uint64_t)));
      archive(_xor_filter->hashIndex);
    }
  }

  template <class Archive> void load(Archive &archive) {
    archive(_num_elements, _error_rate, _fingerprint_width, _is_built);

    if (_is_built) {
      size_t numWords;
      int bitsPerElement;
      archive(numWords, bitsPerElement);

      size_t arrayLength, blockLength;
      archive(arrayLength, blockLength);

      auto filter = std::make_unique<BitPackedXorFilter>(_num_elements, _fingerprint_width);
      filter->arrayLength = arrayLength;
      filter->blockLength = blockLength;

      // Load fingerprint data
      archive(cereal::binary_data(filter->fingerprints->data(),
                                  numWords * sizeof(uint64_t)));
      archive(filter->hashIndex);
      _xor_filter = std::move(filter);
    }
  }

  std::unique_ptr<BitPackedXorFilter> _xor_filter;
  std::vector<uint64_t> _keys;
  size_t _num_elements;
  float _error_rate;
  int _fingerprint_width;
  bool _is_built;
};

using XorFilterPtr = std::shared_ptr<XorFilter>;

} // namespace caramel
