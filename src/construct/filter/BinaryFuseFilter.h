#pragma once

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <memory>
#include <src/construct/SpookyHash.h>
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

class BinaryFuseFilter {
public:
  static BinaryFuseFilter create(size_t num_elements, bool verbose = false) {
    BinaryFuseFilter filter;
    filter._num_elements = num_elements;
    filter._keys.reserve(num_elements);

    if (verbose) {
      std::cout << "BinaryFuseFilter: num_elements=" << num_elements
                << ", FPR~0.39% (fixed)" << std::endl;
    }

    return filter;
  }

  static std::shared_ptr<BinaryFuseFilter> make(size_t num_elements,
                                                bool verbose = false) {
    return std::make_shared<BinaryFuseFilter>(create(num_elements, verbose));
  }

  void add(const std::string &key) {
    uint64_t hash = hashString(key);
    _keys.push_back(hash);
  }

  void build() {
    if (_keys.empty()) {
      return;
    }

    _binary_fuse_filter =
        std::make_unique<xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
            uint64_t, uint8_t, BinaryFuseHasher>>(_keys.size());

    auto status = _binary_fuse_filter->AddAll(_keys.data(), 0, _keys.size());
    if (status != xorbinaryfusefilter_lowmem4wise::Status::Ok) {
      throw std::runtime_error("Failed to build binary fuse filter");
    }
    _is_built = true;
  }

  bool contains(const std::string &key) {
    if (!_binary_fuse_filter || !_is_built) {
      return false;
    }

    uint64_t hash = hashString(key);
    auto status = _binary_fuse_filter->Contain(hash);
    return status == xorbinaryfusefilter_lowmem4wise::Status::Ok;
  }

  size_t size() const {
    return _binary_fuse_filter ? _binary_fuse_filter->SizeInBytes() : 0;
  }

  size_t numElements() const { return _num_elements; }

private:
  uint64_t hashString(const std::string &key) const {
    const void *msgPtr = static_cast<const void *>(key.data());
    size_t length = key.size();
    return SpookyHash::Hash64(msgPtr, length, 0);
  }

  BinaryFuseFilter() : _num_elements(0), _is_built(false) {}

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_num_elements, _is_built);

    if (_is_built && _binary_fuse_filter) {
      // Serialize the binary fuse filter's fingerprints and metadata
      size_t arrayLength = _binary_fuse_filter->arrayLength;
      size_t segmentCount = _binary_fuse_filter->segmentCount;
      size_t segmentCountLength = _binary_fuse_filter->segmentCountLength;
      size_t segmentLength = _binary_fuse_filter->segmentLength;
      size_t segmentLengthMask = _binary_fuse_filter->segmentLengthMask;

      archive(arrayLength, segmentCount, segmentCountLength, segmentLength,
              segmentLengthMask);

      archive(cereal::binary_data(_binary_fuse_filter->fingerprints,
                                  arrayLength * sizeof(uint8_t)));
      archive(_binary_fuse_filter->hashIndex);
    }
  }

  template <class Archive> void load(Archive &archive) {
    archive(_num_elements, _is_built);

    if (_is_built) {
      size_t arrayLength, segmentCount, segmentCountLength, segmentLength,
          segmentLengthMask;
      archive(arrayLength, segmentCount, segmentCountLength, segmentLength,
              segmentLengthMask);

      // Reconstruct binary fuse filter - constructor calculates its own
      // arrayLength
      _binary_fuse_filter =
          std::make_unique<xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
              uint64_t, uint8_t, BinaryFuseHasher>>(_num_elements);

      // Reallocate fingerprints array to match saved size
      if (_binary_fuse_filter->arrayLength != arrayLength) {
        delete[] _binary_fuse_filter->fingerprints;
        _binary_fuse_filter->fingerprints = new uint8_t[arrayLength]();
      }

      // Restore all metadata fields for correct queries
      _binary_fuse_filter->size = _num_elements;
      _binary_fuse_filter->arrayLength = arrayLength;
      _binary_fuse_filter->segmentCount = segmentCount;
      _binary_fuse_filter->segmentCountLength = segmentCountLength;
      _binary_fuse_filter->segmentLength = segmentLength;
      _binary_fuse_filter->segmentLengthMask = segmentLengthMask;

      archive(cereal::binary_data(_binary_fuse_filter->fingerprints,
                                  arrayLength * sizeof(uint8_t)));
      archive(_binary_fuse_filter->hashIndex);
    }
  }

  std::unique_ptr<xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<
      uint64_t, uint8_t, BinaryFuseHasher>>
      _binary_fuse_filter;
  std::vector<uint64_t> _keys;
  size_t _num_elements;
  bool _is_built;
};

using BinaryFuseFilterPtr = std::shared_ptr<BinaryFuseFilter>;

} // namespace caramel
