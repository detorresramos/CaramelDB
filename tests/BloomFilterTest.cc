#include "TestUtils.h"
#include <gtest/gtest.h>
#include <random>
#include <src/construct/BloomFilter.h>

namespace caramel::tests {

TEST(BloomFilterTest, TestErrorRate) {
  size_t num_elements = 10000;
  std::vector<float> error_rates = {0.01, 0.02, 0.03, 0.04, 0.05, 0.1, 0.2};
  for (float error_rate : error_rates) {

    auto bf = BloomFilter::make(
        /* num_elements= */ num_elements, /* error_rate= */ error_rate);

    for (size_t i = 0; i < num_elements; i++) {
      std::string key = "key_" + std::to_string(i);
      bf->add(key);
    }

    size_t num_errors = 0;
    size_t trial_number = 10000;
    for (size_t i = num_elements; i < num_elements + trial_number; i++) {
      std::string key = "key_" + std::to_string(i);
      if (bf->contains(key)) {
        num_errors++;
      }
    }

    // TODO(david) can we figure out how to fix the error rate?
    ASSERT_LE(num_errors, trial_number * error_rate * 1.1);
  }
}

} // namespace caramel::tests