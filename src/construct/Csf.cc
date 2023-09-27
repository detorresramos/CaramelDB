#include "Csf.h"
#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "SpookyHash.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <src/utils/SafeFileIO.h>

namespace caramel {

uint32_t Csf::query(const std::string &key) const {
  Uint128Signature signature = hashKey(key, _hash_store_seed);

  uint32_t bucket_id =
      getBucketID(signature, /* num_buckets= */ _solutions_and_seeds.size());

  auto [solution, construction_seed] = _solutions_and_seeds.at(bucket_id);

  uint32_t solution_size = solution->numBits();

  uint32_t max_codelength = _code_length_counts.size() - 1;

  std::vector<uint32_t> start_var_locations =
      getStartVarLocations(signature, construction_seed, solution_size);

  BitArrayPtr encoded_value = BitArray::make(max_codelength);
  for (auto location : start_var_locations) {
    BitArrayPtr temp = BitArray::make(max_codelength);
    for (uint32_t i = 0; i < max_codelength; i++) {
      if ((*solution)[location]) {
        temp->setBit(i);
      }
      if (location == solution_size - 1) {
        location = 0;
      } else {
        location++;
      }
    }
    *encoded_value ^= *temp;
  }

  return cannonicalDecode(encoded_value, _code_length_counts, _ordered_symbols);
}

void Csf::save(const std::string &filename) const {
  auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
  cereal::BinaryOutputArchive oarchive(output_stream);
  oarchive(*this);
}

CsfPtr Csf::load(const std::string &filename) {
  auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
  cereal::BinaryInputArchive iarchive(input_stream);
  CsfPtr deserialize_into(new Csf());
  iarchive(*deserialize_into);

  return deserialize_into;
}

template void Csf::serialize(cereal::BinaryInputArchive &);
template void Csf::serialize(cereal::BinaryOutputArchive &);

template <class Archive> void Csf::serialize(Archive &archive) {
  archive(_solutions_and_seeds, _code_length_counts, _ordered_symbols,
          _hash_store_seed);
}

uint32_t Csf::size() const { return 0; }

} // namespace caramel