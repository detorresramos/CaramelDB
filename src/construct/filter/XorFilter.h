#pragma once

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <memory>
#include <src/construct/SpookyHash.h>
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

class XorFilter {
public:
  static XorFilter create(size_t num_elements, bool verbose = false) {
    XorFilter filter;
    filter._num_elements = num_elements;
    filter._keys.reserve(num_elements);

    if (verbose) {
      std::cout << "XorFilter: num_elements=" << num_elements
                << ", FPR~0.39% (fixed)" << std::endl;
    }

    return filter;
  }

  static std::shared_ptr<XorFilter> make(size_t num_elements,
                                         bool verbose = false) {
    return std::make_shared<XorFilter>(create(num_elements, verbose));
  }

  void add(const std::string &key) {
    uint64_t hash = hashString(key);
    _keys.push_back(hash);
  }

  void build() {
    if (_keys.empty()) {
      return;
    }

    _xor_filter = std::make_unique<
        xorfilter::XorFilter<uint64_t, uint8_t, XorHasher>>(_keys.size());

    auto status = _xor_filter->AddAll(_keys.data(), 0, _keys.size());
    if (status != xorfilter::Status::Ok) {
      throw std::runtime_error("Failed to build xor filter");
    }
    _is_built = true;
  }

  bool contains(const std::string &key) {
    if (!_xor_filter || !_is_built) {
      return false;
    }

    uint64_t hash = hashString(key);
    auto status = _xor_filter->Contain(hash);
    return status == xorfilter::Status::Ok;
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

  XorFilter() : _num_elements(0), _is_built(false) {}

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_num_elements, _is_built);

    if (_is_built && _xor_filter) {
      // Serialize the xor filter's fingerprints
      size_t arrayLength = _xor_filter->arrayLength;
      archive(arrayLength);
      archive(cereal::binary_data(_xor_filter->fingerprints,
                                  arrayLength * sizeof(uint8_t)));
      archive(_xor_filter->hashIndex);
    }
  }

  template <class Archive> void load(Archive &archive) {
    archive(_num_elements, _is_built);

    if (_is_built) {
      size_t arrayLength;
      archive(arrayLength);

      // Reconstruct xor filter with correct size
      // Note: we need arrayLength/1.23 to get original size, but we stored _num_elements
      _xor_filter = std::make_unique<
          xorfilter::XorFilter<uint64_t, uint8_t, XorHasher>>(_num_elements);

      // Reallocate fingerprints array if size doesn't match
      if (_xor_filter->arrayLength != arrayLength) {
        delete[] _xor_filter->fingerprints;
        _xor_filter->fingerprints = new uint8_t[arrayLength]();
      }

      // Restore all fields to prevent out-of-bounds access
      _xor_filter->size = _num_elements;
      _xor_filter->arrayLength = arrayLength;
      _xor_filter->blockLength = arrayLength / 3;  // CRITICAL: blockLength must match arrayLength

      archive(cereal::binary_data(_xor_filter->fingerprints,
                                  arrayLength * sizeof(uint8_t)));
      archive(_xor_filter->hashIndex);
    }
    // _keys is not needed after deserialization since filter is already built
  }

  std::unique_ptr<xorfilter::XorFilter<uint64_t, uint8_t, XorHasher>>
      _xor_filter;
  std::vector<uint64_t> _keys;
  size_t _num_elements;
  bool _is_built;
};

using XorFilterPtr = std::shared_ptr<XorFilter>;

} // namespace caramel
