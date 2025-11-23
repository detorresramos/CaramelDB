#pragma once

#include "BitPackedXorFilter.h"
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <memory>
#include <src/construct/SpookyHash.h>
#include <variant>
#include <vector>
#include <xorfilter/xorfilter.h>

namespace caramel {

// Custom hasher for xor filter that just returns the key (we pre-hash strings)
class XorHasher {
public:
  uint64_t seed;
  XorHasher() : seed(0) {}
  inline uint64_t operator()(uint64_t key) const { return key; }
};

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

    // Create filter with appropriate fingerprint width
    // Use bit-packed filter for 1-7 bits, standard types for 8, 16, 32
    if (_fingerprint_width < 8) {
      // Bit-packed filter for sub-byte fingerprints (1-7 bits)
      auto filter = std::make_unique<BitPackedXorFilter>(_keys.size(), _fingerprint_width);
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != XorStatus::Ok) {
        throw std::runtime_error("Failed to build bit-packed xor filter");
      }
      _xor_filter = std::move(filter);
    } else if (_fingerprint_width == 8) {
      // Standard 8-bit filter (fastest queries, no bit-packing overhead)
      auto filter = std::make_unique<
          xorfilter::XorFilter<uint64_t, uint8_t, XorHasher>>(_keys.size());
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != xorfilter::Status::Ok) {
        throw std::runtime_error("Failed to build xor filter");
      }
      _xor_filter = std::move(filter);
    } else if (_fingerprint_width <= 16) {
      // Standard 16-bit filter for 9-16 bit fingerprints
      auto filter = std::make_unique<
          xorfilter::XorFilter<uint64_t, uint16_t, XorHasher>>(_keys.size());
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != xorfilter::Status::Ok) {
        throw std::runtime_error("Failed to build xor filter");
      }
      _xor_filter = std::move(filter);
    } else if (_fingerprint_width <= 32) {
      // Standard 32-bit filter for 17-32 bit fingerprints
      auto filter = std::make_unique<
          xorfilter::XorFilter<uint64_t, uint32_t, XorHasher>>(_keys.size());
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != xorfilter::Status::Ok) {
        throw std::runtime_error("Failed to build xor filter");
      }
      _xor_filter = std::move(filter);
    } else {
      throw std::runtime_error("Unsupported fingerprint width: " +
                                std::to_string(_fingerprint_width));
    }
    _is_built = true;
  }

  bool contains(const std::string &key) {
    if (!_is_built) {
      return false;
    }

    uint64_t hash = hashString(key);
    return std::visit(
        [hash](auto &&filter) -> bool {
          if (!filter) {
            return false;
          }

          using FilterType = std::decay_t<decltype(*filter)>;

          // Handle BitPackedXorFilter (uses XorStatus)
          if constexpr (std::is_same_v<FilterType, BitPackedXorFilter>) {
            auto status = filter->Contain(hash);
            return status == XorStatus::Ok;
          }
          // Handle standard XorFilter (uses xorfilter::Status)
          else {
            auto status = filter->Contain(hash);
            return status == xorfilter::Status::Ok;
          }
        },
        _xor_filter);
  }

  size_t size() const {
    return std::visit(
        [](auto &&filter) -> size_t {
          return filter ? filter->SizeInBytes() : 0;
        },
        _xor_filter);
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

    if (_is_built) {
      if (_fingerprint_width < 8) {
        // Bit-packed filter (1-7 bits)
        auto &filter = std::get<0>(_xor_filter);  // BitPackedXorFilter
        if (filter && filter->fingerprints) {
          size_t arrayLength = filter->fingerprints->numWords();
          int bitsPerElement = filter->fingerprints->bitsPerElement();
          archive(arrayLength, bitsPerElement);
          archive(cereal::binary_data(filter->fingerprints->data(),
                                      arrayLength * sizeof(uint64_t)));
          archive(filter->hashIndex);
        }
      } else if (_fingerprint_width == 8) {
        auto &filter = std::get<1>(_xor_filter);  // XorFilter8
        if (filter) {
          size_t arrayLength = filter->arrayLength;
          archive(arrayLength);
          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint8_t)));
          archive(filter->hashIndex);
        }
      } else if (_fingerprint_width <= 16) {
        auto &filter = std::get<2>(_xor_filter);  // XorFilter16
        if (filter) {
          size_t arrayLength = filter->arrayLength;
          archive(arrayLength);
          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint16_t)));
          archive(filter->hashIndex);
        }
      } else {
        auto &filter = std::get<3>(_xor_filter);  // XorFilter32
        if (filter) {
          size_t arrayLength = filter->arrayLength;
          archive(arrayLength);
          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint32_t)));
          archive(filter->hashIndex);
        }
      }
    }
  }

  template <class Archive> void load(Archive &archive) {
    archive(_num_elements, _error_rate, _fingerprint_width, _is_built);

    if (_is_built) {
      if (_fingerprint_width < 8) {
        // Bit-packed filter (1-7 bits)
        size_t arrayLength;
        int bitsPerElement;
        archive(arrayLength, bitsPerElement);

        auto filter = std::make_unique<BitPackedXorFilter>(_num_elements, _fingerprint_width);

        // Load fingerprint data
        archive(cereal::binary_data(filter->fingerprints->data(),
                                    arrayLength * sizeof(uint64_t)));
        archive(filter->hashIndex);
        _xor_filter = std::move(filter);
      } else if (_fingerprint_width == 8) {
        size_t arrayLength;
        archive(arrayLength);

        auto filter = std::make_unique<
            xorfilter::XorFilter<uint64_t, uint8_t, XorHasher>>(_num_elements);

        if (filter->arrayLength != arrayLength) {
          delete[] filter->fingerprints;
          filter->fingerprints = new uint8_t[arrayLength]();
        }

        filter->size = _num_elements;
        filter->arrayLength = arrayLength;
        filter->blockLength = arrayLength / 3;

        archive(cereal::binary_data(filter->fingerprints,
                                    arrayLength * sizeof(uint8_t)));
        archive(filter->hashIndex);
        _xor_filter = std::move(filter);
      } else if (_fingerprint_width <= 16) {
        size_t arrayLength;
        archive(arrayLength);

        auto filter = std::make_unique<
            xorfilter::XorFilter<uint64_t, uint16_t, XorHasher>>(_num_elements);

        if (filter->arrayLength != arrayLength) {
          delete[] filter->fingerprints;
          filter->fingerprints = new uint16_t[arrayLength]();
        }

        filter->size = _num_elements;
        filter->arrayLength = arrayLength;
        filter->blockLength = arrayLength / 3;

        archive(cereal::binary_data(filter->fingerprints,
                                    arrayLength * sizeof(uint16_t)));
        archive(filter->hashIndex);
        _xor_filter = std::move(filter);
      } else {
        size_t arrayLength;
        archive(arrayLength);

        auto filter = std::make_unique<
            xorfilter::XorFilter<uint64_t, uint32_t, XorHasher>>(_num_elements);

        if (filter->arrayLength != arrayLength) {
          delete[] filter->fingerprints;
          filter->fingerprints = new uint32_t[arrayLength]();
        }

        filter->size = _num_elements;
        filter->arrayLength = arrayLength;
        filter->blockLength = arrayLength / 3;

        archive(cereal::binary_data(filter->fingerprints,
                                    arrayLength * sizeof(uint32_t)));
        archive(filter->hashIndex);
        _xor_filter = std::move(filter);
      }
    }
    // _keys is not needed after deserialization since filter is already built
  }

  using XorFilterBitPacked = std::unique_ptr<BitPackedXorFilter>;
  using XorFilter8 =
      std::unique_ptr<xorfilter::XorFilter<uint64_t, uint8_t, XorHasher>>;
  using XorFilter16 =
      std::unique_ptr<xorfilter::XorFilter<uint64_t, uint16_t, XorHasher>>;
  using XorFilter32 =
      std::unique_ptr<xorfilter::XorFilter<uint64_t, uint32_t, XorHasher>>;

  std::variant<XorFilterBitPacked, XorFilter8, XorFilter16, XorFilter32> _xor_filter;
  std::vector<uint64_t> _keys;
  size_t _num_elements;
  float _error_rate;
  int _fingerprint_width;
  bool _is_built;
};

using XorFilterPtr = std::shared_ptr<XorFilter>;

} // namespace caramel
