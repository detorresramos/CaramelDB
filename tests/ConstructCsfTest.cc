#include <gtest/gtest.h>
#include <memory>
#include <src/construct/Construct.h>

namespace caramel::tests {

TEST(ConstructCsfTest, QueriesDecodeCorrectly) {
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  std::vector<uint32_t> values = {1, 2, 3};
  CsfPtr<uint32_t> csf = constructCsf(keys, values);

  for (uint32_t i = 0; i < keys.size(); i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
}

} // namespace caramel::tests