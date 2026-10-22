#pragma once

#include "XorFilter.h"
#include "FilterTypes.h"
#include "PreFilter.h"
#include <src/utils/SafeFileIO.h>

namespace caramel {

template <typename T> class XORPreFilter;
template <typename T>
using XORPreFilterPtr = std::shared_ptr<XORPreFilter<T>>;

template <typename T> class XORPreFilter final : public PreFilter<T> {
public:
  static XORPreFilterPtr<T> build(int fingerprint_bits, T most_common_value,
                                  std::vector<uint64_t> key_hashes,
                                  bool verbose) {
    XORPreFilterPtr<T> filter(new XORPreFilter<T>());
    filter->_most_common_value = most_common_value;
    filter->_xor_filter =
        XorFilter::makeFixed(key_hashes.size(), fingerprint_bits, verbose);
    filter->_xor_filter->buildFromHashes(std::move(key_hashes));
    return filter;
  }

  bool containsHash(uint64_t key_hash) const override {
    return _xor_filter->containsHash(key_hash);
  }

  std::optional<FilterStats> getStats() const override {
    FilterStats fs;
    fs.type = "xor";
    fs.size_bytes = _xor_filter->size();
    fs.num_elements = _xor_filter->numElements();
    fs.fingerprint_bits = _xor_filter->fingerprintWidth();
    return fs;
  }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static XORPreFilterPtr<T> load(const std::string &filename) {
    XORPreFilterPtr<T> filter(new XORPreFilter<T>());
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    cereal::BinaryInputArchive iarchive(input_stream);
    iarchive(*filter);
    return filter;
  }

private:
  XORPreFilter() = default;

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _xor_filter);
  }

  XorFilterPtr _xor_filter;
};

} // namespace caramel

CARAMEL_REGISTER_PREFILTER(XORPreFilter)
