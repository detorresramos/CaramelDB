#pragma once

#include "SpookyHash.h"
#include <string>
#include <vector>

namespace caramel {

std::tuple<std::vector<std::vector<__uint128_t>>,
           std::vector<std::vector<uint32_t>>, uint64_t>
partitionToBuckets(const std::vector<std::string> &keys,
                   const std::vector<uint32_t> &values,
                   uint32_t bucket_size = 256, uint32_t num_attempts = 3);

uint32_t getBucketID(__uint128_t signature, uint32_t num_buckets);

__uint128_t hashKey(const std::string &key, uint64_t seed);

} // namespace caramel