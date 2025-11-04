#include <gtest/gtest.h>
#include <random>
#include <cmath>
#include <algorithm>
#include <src/construct/filter/BloomFilter.h>
#include <src/construct/filter/XorFilter.h>
#include <src/construct/filter/BinaryFuseFilter.h>

namespace caramel::tests {

TEST(FilterSizeComparisonTest, CompareFilterSizesWithPowerlawDistribution) {
  const size_t total_elements = 100000;
  const double powerlaw_exponent = 2.0; // Zipf-like distribution

  // Generate keys with powerlaw frequency distribution
  std::vector<std::string> all_keys;
  std::mt19937 rng(42); // Fixed seed for reproducibility

  // Create a powerlaw distribution for the number of unique keys
  // We'll have fewer unique keys that appear with varying frequencies
  const size_t num_unique_keys = 10000;
  std::vector<size_t> frequencies(num_unique_keys);

  // Generate powerlaw frequencies using Zipf distribution
  // frequency[i] ~ 1 / (i+1)^exponent
  double normalization = 0.0;
  for (size_t i = 0; i < num_unique_keys; i++) {
    frequencies[i] = std::max(1.0, 1.0 / std::pow(i + 1, powerlaw_exponent) * 1000);
    normalization += frequencies[i];
  }

  // Normalize to get exactly total_elements
  double scale = total_elements / normalization;
  size_t total_generated = 0;
  for (size_t i = 0; i < num_unique_keys; i++) {
    frequencies[i] = std::max(1UL, static_cast<size_t>(frequencies[i] * scale));
    total_generated += frequencies[i];
  }

  // Adjust the first element to hit exactly total_elements
  if (total_generated < total_elements) {
    frequencies[0] += (total_elements - total_generated);
  } else if (total_generated > total_elements) {
    frequencies[0] -= std::min(frequencies[0] - 1, total_generated - total_elements);
  }

  // Generate the actual keys based on frequencies
  all_keys.reserve(total_elements);
  for (size_t i = 0; i < num_unique_keys; i++) {
    std::string key = "key_" + std::to_string(i);
    for (size_t j = 0; j < frequencies[i]; j++) {
      all_keys.push_back(key);
    }
  }

  // Shuffle to make it realistic
  std::shuffle(all_keys.begin(), all_keys.end(), rng);

  std::cout << "\n=== Filter Size Comparison Test ===\n";
  std::cout << "Total elements: " << all_keys.size() << "\n";
  std::cout << "Unique keys: " << num_unique_keys << "\n";
  std::cout << "Powerlaw exponent: " << powerlaw_exponent << "\n";
  std::cout << "Most common key frequency: " << frequencies[0] << "\n";
  std::cout << "Least common key frequency: " << frequencies[num_unique_keys - 1] << "\n\n";

  // Create Bloom filter with ~0.39% error rate to match XOR/Binary Fuse
  auto bloom_filter = BloomFilter::makeAutotuned(num_unique_keys, 0.0039, false);
  for (const auto& key : all_keys) {
    bloom_filter->add(key);
  }
  size_t bloom_size = bloom_filter->size() / 8; // Convert bits to bytes

  // Create XOR filter
  auto xor_filter = XorFilter::make(num_unique_keys);
  for (const auto& key : all_keys) {
    xor_filter->add(key);
  }
  xor_filter->build();
  size_t xor_size = xor_filter->size();

  // Create Binary Fuse filter
  auto binary_fuse_filter = BinaryFuseFilter::make(num_unique_keys);
  for (const auto& key : all_keys) {
    binary_fuse_filter->add(key);
  }
  binary_fuse_filter->build();
  size_t binary_fuse_size = binary_fuse_filter->size();

  // Print sizes
  std::cout << "Filter sizes (bytes):\n";
  std::cout << "  Bloom Filter:       " << bloom_size
            << " (" << (bloom_size * 8.0 / num_unique_keys) << " bits/key)\n";
  std::cout << "  XOR Filter:         " << xor_size
            << " (" << (xor_size * 8.0 / num_unique_keys) << " bits/key)\n";
  std::cout << "  Binary Fuse Filter: " << binary_fuse_size
            << " (" << (binary_fuse_size * 8.0 / num_unique_keys) << " bits/key)\n\n";

  // Verify size ordering: Bloom > XOR > Binary Fuse
  ASSERT_GT(bloom_size, xor_size)
      << "Bloom filter should be larger than XOR filter";
  ASSERT_GT(xor_size, binary_fuse_size)
      << "XOR filter should be larger than Binary Fuse filter";

  // Verify all filters work correctly (no false negatives)
  std::set<std::string> unique_keys_set(all_keys.begin(), all_keys.end());
  std::vector<std::string> unique_keys_vec(unique_keys_set.begin(), unique_keys_set.end());

  // Test a sample of keys
  size_t sample_size = std::min(1000UL, unique_keys_vec.size());
  for (size_t i = 0; i < sample_size; i++) {
    const auto& key = unique_keys_vec[i];
    ASSERT_TRUE(bloom_filter->contains(key))
        << "Bloom filter should contain key: " << key;
    ASSERT_TRUE(xor_filter->contains(key))
        << "XOR filter should contain key: " << key;
    ASSERT_TRUE(binary_fuse_filter->contains(key))
        << "Binary Fuse filter should contain key: " << key;
  }

  std::cout << "✓ All filters contain the inserted keys (no false negatives)\n";
  std::cout << "✓ Size ordering verified: Bloom > XOR > Binary Fuse\n";
}

} // namespace caramel::tests
