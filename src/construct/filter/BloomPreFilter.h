#pragma once

#include "BloomFilter.h"
#include "FilterTypes.h"
#include "PreFilter.h"
#include <iostream>
#include <src/utils/SafeFileIO.h>

namespace caramel {

template <typename T> class BloomPreFilter;
template <typename T>
using BloomPreFilterPtr = std::shared_ptr<BloomPreFilter<T>>;

template <typename T> class BloomPreFilter final : public PreFilter<T> {
public:
  static BloomPreFilterPtr<T> build(size_t bits_per_element, size_t num_hashes,
                                    T most_common_value,
                                    const std::vector<uint64_t> &key_hashes,
                                    bool verbose) {
    BloomPreFilterPtr<T> filter(new BloomPreFilter<T>());
    filter->_most_common_value = most_common_value;
    size_t total_bits = std::max(bits_per_element * key_hashes.size(), size_t(1));
    filter->_bloom_filter = BloomFilter::makeFixed(total_bits, num_hashes);
    for (uint64_t h : key_hashes) {
      filter->_bloom_filter->addHash(h);
    }
    if (verbose) {
      std::cout << "BloomPreFilter: bits_per_element=" << bits_per_element
                << ", num_minority=" << key_hashes.size()
                << ", num_hashes=" << num_hashes << std::endl;
    }
    return filter;
  }

  bool containsHash(uint64_t key_hash) const override {
    return _bloom_filter->containsHash(key_hash);
  }

  std::optional<FilterStats> getStats() const override {
    FilterStats fs;
    fs.type = "bloom";
    fs.size_bits = _bloom_filter->size();
    fs.size_bytes = (_bloom_filter->size() + 7) / 8;
    fs.num_hashes = _bloom_filter->numHashes();
    fs.num_elements = 0;
    return fs;
  }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static BloomPreFilterPtr<T> load(const std::string &filename) {
    BloomPreFilterPtr<T> filter(new BloomPreFilter<T>());
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    cereal::BinaryInputArchive iarchive(input_stream);
    iarchive(*filter);
    return filter;
  }

private:
  BloomPreFilter() = default;

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _bloom_filter);
  }

  BloomFilterPtr _bloom_filter;
};

} // namespace caramel

CARAMEL_REGISTER_PREFILTER(BloomPreFilter)
