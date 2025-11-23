#pragma once

#include "BitPackedBinaryFuseFilter.h"
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <memory>
#include <src/construct/SpookyHash.h>
#include <variant>
#include <vector>
#include <xorfilter/4wise_xor_binary_fuse_filter_lowmem.h>

namespace caramel {

// Custom hasher for binary fuse filter that just returns the key (we pre-hash
// strings)
class BinaryFuseHasher {
public:
  uint64_t seed;
  BinaryFuseHasher() : seed(0) {}
  inline uint64_t operator()(uint64_t key) const { return key; }
};

// Utility functions for fingerprint width calculation (shared with XOR filter)
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

  void add(const std::string &key) {
    uint64_t hash = hashString(key);
    _keys.push_back(hash);
  }

  void build() {
    if (_keys.empty()) {
      return;
    }

    if (_num_elements <= 10) {
      throw std::invalid_argument(
          "BinaryFuseFilter requires more than 10 elements. Got " +
          std::to_string(_num_elements) +
          " elements. Binary fuse filter construction is probabilistic and "
          "fails with very small key counts.");
    }

    // Create filter with appropriate fingerprint width
    if (_fingerprint_width < 8) {
      // Bit-packed filter for sub-byte fingerprints (1-7 bits)
      auto filter = std::make_unique<BitPackedBinaryFuseFilter>(_keys.size(), _fingerprint_width);
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != BinaryFuseStatus::Ok) {
        throw std::runtime_error("Failed to build bit-packed binary fuse filter");
      }
      _binary_fuse_filter = std::move(filter);
    } else if (_fingerprint_width == 8) {
      auto filter = std::make_unique<
          xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
              uint64_t, uint8_t, BinaryFuseHasher>>(_keys.size());
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != xorbinaryfusefilter_lowmem4wise::Status::Ok) {
        throw std::runtime_error("Failed to build binary fuse filter");
      }
      _binary_fuse_filter = std::move(filter);
    } else if (_fingerprint_width <= 16) {
      auto filter = std::make_unique<
          xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
              uint64_t, uint16_t, BinaryFuseHasher>>(_keys.size());
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != xorbinaryfusefilter_lowmem4wise::Status::Ok) {
        throw std::runtime_error("Failed to build binary fuse filter");
      }
      _binary_fuse_filter = std::move(filter);
    } else if (_fingerprint_width <= 32) {
      auto filter = std::make_unique<
          xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
              uint64_t, uint32_t, BinaryFuseHasher>>(_keys.size());
      auto status = filter->AddAll(_keys.data(), 0, _keys.size());
      if (status != xorbinaryfusefilter_lowmem4wise::Status::Ok) {
        throw std::runtime_error("Failed to build binary fuse filter");
      }
      _binary_fuse_filter = std::move(filter);
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

          // Handle BitPackedBinaryFuseFilter (uses BinaryFuseStatus)
          if constexpr (std::is_same_v<FilterType, BitPackedBinaryFuseFilter>) {
            auto status = filter->Contain(hash);
            return status == BinaryFuseStatus::Ok;
          }
          // Handle standard BinaryFuseFilter (uses xorbinaryfusefilter_lowmem4wise::Status)
          else {
            auto status = filter->Contain(hash);
            return status == xorbinaryfusefilter_lowmem4wise::Status::Ok;
          }
        },
        _binary_fuse_filter);
  }

  size_t size() const {
    return std::visit(
        [](auto &&filter) -> size_t {
          return filter ? filter->SizeInBytes() : 0;
        },
        _binary_fuse_filter);
  }

  size_t numElements() const { return _num_elements; }

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

    if (_is_built) {
      if (_fingerprint_width < 8) {
        // Bit-packed filter (1-7 bits)
        auto &filter = std::get<0>(_binary_fuse_filter);  // BitPackedBinaryFuseFilter
        if (filter && filter->fingerprints) {
          size_t arrayLength = filter->fingerprints->numWords();
          int bitsPerElement = filter->fingerprints->bitsPerElement();
          archive(arrayLength, bitsPerElement);
          archive(cereal::binary_data(filter->fingerprints->data(),
                                      arrayLength * sizeof(uint64_t)));
          archive(filter->hashIndex);
          archive(filter->segmentCount, filter->segmentCountLength,
                  filter->segmentLength, filter->segmentLengthMask);
        }
      } else if (_fingerprint_width == 8) {
        auto &filter = std::get<1>(_binary_fuse_filter);
        if (filter) {
          size_t arrayLength = filter->arrayLength;
          size_t segmentCount = filter->segmentCount;
          size_t segmentCountLength = filter->segmentCountLength;
          size_t segmentLength = filter->segmentLength;
          size_t segmentLengthMask = filter->segmentLengthMask;

          archive(arrayLength, segmentCount, segmentCountLength, segmentLength,
                  segmentLengthMask);
          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint8_t)));
          archive(filter->hashIndex);
        }
      } else if (_fingerprint_width <= 16) {
        auto &filter = std::get<2>(_binary_fuse_filter);
        if (filter) {
          size_t arrayLength = filter->arrayLength;
          size_t segmentCount = filter->segmentCount;
          size_t segmentCountLength = filter->segmentCountLength;
          size_t segmentLength = filter->segmentLength;
          size_t segmentLengthMask = filter->segmentLengthMask;

          archive(arrayLength, segmentCount, segmentCountLength, segmentLength,
                  segmentLengthMask);
          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint16_t)));
          archive(filter->hashIndex);
        }
      } else if (_fingerprint_width <= 32) {
        auto &filter = std::get<3>(_binary_fuse_filter);
        if (filter) {
          size_t arrayLength = filter->arrayLength;
          size_t segmentCount = filter->segmentCount;
          size_t segmentCountLength = filter->segmentCountLength;
          size_t segmentLength = filter->segmentLength;
          size_t segmentLengthMask = filter->segmentLengthMask;

          archive(arrayLength, segmentCount, segmentCountLength, segmentLength,
                  segmentLengthMask);
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

        auto filter = std::make_unique<BitPackedBinaryFuseFilter>(_num_elements, _fingerprint_width);

        // Load fingerprint data
        archive(cereal::binary_data(filter->fingerprints->data(),
                                    arrayLength * sizeof(uint64_t)));
        archive(filter->hashIndex);
        archive(filter->segmentCount, filter->segmentCountLength,
                filter->segmentLength, filter->segmentLengthMask);

        filter->size = _num_elements;
        filter->arrayLength = filter->fingerprints->size();
        _binary_fuse_filter = std::move(filter);
      } else {
        size_t arrayLength, segmentCount, segmentCountLength, segmentLength,
            segmentLengthMask;
        archive(arrayLength, segmentCount, segmentCountLength, segmentLength,
                segmentLengthMask);

        // Reconstruct binary fuse filter with correct fingerprint width
        if (_fingerprint_width == 8) {
        auto filter = std::make_unique<
            xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
                uint64_t, uint8_t, BinaryFuseHasher>>(_num_elements);

        if (filter->arrayLength != arrayLength) {
          delete[] filter->fingerprints;
          filter->fingerprints = new uint8_t[arrayLength]();
        }

        filter->size = _num_elements;
        filter->arrayLength = arrayLength;
        filter->segmentCount = segmentCount;
        filter->segmentCountLength = segmentCountLength;
        filter->segmentLength = segmentLength;
        filter->segmentLengthMask = segmentLengthMask;

          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint8_t)));
          archive(filter->hashIndex);
          _binary_fuse_filter = std::move(filter);
        } else if (_fingerprint_width <= 16) {
          auto filter = std::make_unique<
              xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
                  uint64_t, uint16_t, BinaryFuseHasher>>(_num_elements);

          if (filter->arrayLength != arrayLength) {
            delete[] filter->fingerprints;
            filter->fingerprints = new uint16_t[arrayLength]();
          }

          filter->size = _num_elements;
          filter->arrayLength = arrayLength;
          filter->segmentCount = segmentCount;
          filter->segmentCountLength = segmentCountLength;
          filter->segmentLength = segmentLength;
          filter->segmentLengthMask = segmentLengthMask;

          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint16_t)));
          archive(filter->hashIndex);
          _binary_fuse_filter = std::move(filter);
        } else if (_fingerprint_width <= 32) {
          auto filter = std::make_unique<
              xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
                  uint64_t, uint32_t, BinaryFuseHasher>>(_num_elements);

          if (filter->arrayLength != arrayLength) {
            delete[] filter->fingerprints;
            filter->fingerprints = new uint32_t[arrayLength]();
          }

          filter->size = _num_elements;
          filter->arrayLength = arrayLength;
          filter->segmentCount = segmentCount;
          filter->segmentCountLength = segmentCountLength;
          filter->segmentLength = segmentLength;
          filter->segmentLengthMask = segmentLengthMask;

          archive(cereal::binary_data(filter->fingerprints,
                                      arrayLength * sizeof(uint32_t)));
          archive(filter->hashIndex);
          _binary_fuse_filter = std::move(filter);
        }
      }
    }
  }

  using BinaryFuseFilterBitPacked = std::unique_ptr<BitPackedBinaryFuseFilter>;
  using BinaryFuseFilter8 = std::unique_ptr<
      xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
          uint64_t, uint8_t, BinaryFuseHasher>>;
  using BinaryFuseFilter16 = std::unique_ptr<
      xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
          uint64_t, uint16_t, BinaryFuseHasher>>;
  using BinaryFuseFilter32 = std::unique_ptr<
      xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
          uint64_t, uint32_t, BinaryFuseHasher>>;

  std::variant<BinaryFuseFilterBitPacked, BinaryFuseFilter8, BinaryFuseFilter16, BinaryFuseFilter32>
      _binary_fuse_filter;
  std::vector<uint64_t> _keys;
  size_t _num_elements;
  float _error_rate;
  int _fingerprint_width;
  bool _is_built;
};

using BinaryFuseFilterPtr = std::shared_ptr<BinaryFuseFilter>;

} // namespace caramel
