#pragma once
#include "SpookyHash.h"
#include <vector>

namespace caramel {

inline std::vector<uint32_t>
getStartVarLocations(const Uint128Signature &key_signature, uint32_t seed,
                     uint32_t num_variables, uint32_t bits_per_equation = 3) {
  std::vector<uint32_t> start_var_locations;
  while (start_var_locations.size() != bits_per_equation) {
    // TODO lets write a custom hash function that generates three 64 bit
    // hashes instead of hashing 3 times
    // TODO lets do sebastiano's trick here instead of modding
    uint32_t location = SpookyHash::Hash64(&key_signature.first,
                                           /* length = */ 8, seed++) %
                        num_variables;

    if (std::find(start_var_locations.begin(), start_var_locations.end(),
                  location) == start_var_locations.end()) {
      start_var_locations.push_back(location);
    }
  }
  return start_var_locations;
}

} // namespace caramel