#pragma once

#include "BinaryFuseFilter.h"
#include "FilterTypes.h"
#include "PreFilter.h"
#include <src/utils/SafeFileIO.h>

namespace caramel {

template <typename T> class BinaryFusePreFilter;
template <typename T>
using BinaryFusePreFilterPtr = std::shared_ptr<BinaryFusePreFilter<T>>;

template <typename T> class BinaryFusePreFilter final : public PreFilter<T> {
public:
  static BinaryFusePreFilterPtr<T> build(int fingerprint_bits, T most_common_value,
                                  std::vector<uint64_t> key_hashes,
                                  bool verbose) {
    BinaryFusePreFilterPtr<T> filter(new BinaryFusePreFilter<T>());
    filter->_most_common_value = most_common_value;
    filter->_binary_fuse_filter =
        BinaryFuseFilter::makeFixed(key_hashes.size(), fingerprint_bits, verbose);
    filter->_binary_fuse_filter->buildFromHashes(std::move(key_hashes));
    return filter;
  }

  bool containsHash(uint64_t key_hash) const override {
    return _binary_fuse_filter->containsHash(key_hash);
  }

  std::optional<FilterStats> getStats() const override {
    FilterStats fs;
    fs.type = "binary_fuse";
    fs.size_bytes = _binary_fuse_filter->size();
    fs.num_elements = _binary_fuse_filter->numElements();
    fs.fingerprint_bits = _binary_fuse_filter->fingerprintWidth();
    return fs;
  }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static BinaryFusePreFilterPtr<T> load(const std::string &filename) {
    BinaryFusePreFilterPtr<T> filter(new BinaryFusePreFilter<T>());
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    cereal::BinaryInputArchive iarchive(input_stream);
    iarchive(*filter);
    return filter;
  }

private:
  BinaryFusePreFilter() = default;

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _binary_fuse_filter);
  }

  BinaryFuseFilterPtr _binary_fuse_filter;
};

} // namespace caramel

CARAMEL_REGISTER_PREFILTER(BinaryFusePreFilter)
