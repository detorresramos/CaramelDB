#include "BloomFilter.h"

namespace caramel {

template void BloomFilter::serialize(cereal::BinaryInputArchive &);
template void BloomFilter::serialize(cereal::BinaryOutputArchive &);

template <class Archive> void BloomFilter::serialize(Archive &archive) {
  archive(_bitarray, _num_hashes);
}

} // namespace caramel