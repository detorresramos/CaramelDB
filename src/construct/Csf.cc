#include "Csf.h"
#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "SpookyHash.h"

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

uint32_t Csf::size() const { return 0; }

} // namespace caramel