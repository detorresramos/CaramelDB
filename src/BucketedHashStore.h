#include <string>
#include <vector>

namespace caramel {

struct Uint128Signature {
  uint64_t first;
  uint64_t second;

  bool operator==(const Uint128Signature &other) const {
    return first == other.first && second == other.second;
  }
};

std::tuple<std::vector<std::vector<Uint128Signature>>,
           std::vector<std::vector<uint32_t>>, uint64_t>
partitionToBuckets(const std::vector<std::string> &keys,
                   const std::vector<uint32_t> &values,
                   uint32_t bucket_size = 256, uint32_t num_attempts = 3);

} // namespace caramel