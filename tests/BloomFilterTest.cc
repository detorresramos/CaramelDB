#include <gtest/gtest.h>
#include <random>
#include <fstream>
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <src/construct/filter/BloomFilter.h>

namespace caramel::tests {

TEST(BloomFilterTest, SimpleAddAndCheck) {
  auto bf = BloomFilter::makeAutotuned(/* num_elements= */ 100, /* error_rate= */ 0.01, /* verbose= */ false);
  
  std::vector<std::string> inserted_keys = {
    "apple", "banana", "cherry", "date", "elderberry",
    "fig", "grape", "honeydew", "kiwi", "lemon",
    "mango", "nectarine", "orange", "papaya", "quince"
  };
  
  for (const auto& key : inserted_keys) {
    bf->add(key);
  }
  
  for (const auto& key : inserted_keys) {
    ASSERT_TRUE(bf->contains(key)) << "Key '" << key << "' must be in the filter (no false negatives)";
  }
  
  std::vector<std::string> not_inserted_keys = {
    "strawberry", "watermelon", "pear", "plum", "guava",
    "lychee", "dragonfruit", "passionfruit", "starfruit", "durian"
  };
  
  int false_positives = 0;
  for (const auto& key : not_inserted_keys) {
    if (bf->contains(key)) {
      false_positives++;
    }
  }
  
  ASSERT_LE(false_positives, 2) << "Too many false positives for a 0.01 error rate with " 
                                 << not_inserted_keys.size() << " queries";
}

TEST(BloomFilterTest, TestErrorRate) {
  size_t num_elements = 10000;
  std::vector<float> error_rates = {0.01, 0.02, 0.03, 0.04, 0.05, 0.1, 0.2};
  for (float error_rate : error_rates) {

    auto bf = BloomFilter::makeAutotuned(
        /* num_elements= */ num_elements, /* error_rate= */ error_rate, /* verbose= */ false);

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

    ASSERT_LE(num_errors, trial_number * error_rate);
  }
}

TEST(BloomFilterTest, VerifyOptimalNumHashes) {
  // Test that k = (m/n) * ln(2) is indeed optimal
  // We'll create m/n ratios that give optimal k values from 1 to 5
  
  struct TestCase {
    size_t m;  // bitarray size
    size_t n;  // number of elements
    size_t optimal_k;  // expected optimal number of hashes
  };
  
  std::vector<TestCase> test_cases = {
    {1450, 1000, 1},   // k ≈ 1.45 * 0.693 ≈ 1.00
    {2900, 1000, 2},   // k ≈ 2.90 * 0.693 ≈ 2.01  
    {4300, 1000, 3},   // k ≈ 4.30 * 0.693 ≈ 2.98
    {5800, 1000, 4},   // k ≈ 5.80 * 0.693 ≈ 4.02
    {7200, 1000, 5},   // k ≈ 7.20 * 0.693 ≈ 4.99
  };
  
  std::mt19937 rng(std::random_device{}());
  
  for (const auto& tc : test_cases) {
    // Calculate the theoretical optimal k
    size_t theoretical_k = std::round((static_cast<double>(tc.m) / tc.n) * log(2));
    ASSERT_EQ(theoretical_k, tc.optimal_k) 
        << "Test case setup error: m=" << tc.m << " n=" << tc.n;
    
    // Generate test data
    std::vector<std::string> keys_to_insert;
    std::vector<std::string> keys_to_test;
    
    for (size_t i = 0; i < tc.n; i++) {
      keys_to_insert.push_back("insert_" + std::to_string(i));
    }
    
    // Generate a large number of keys to test false positive rate
    size_t num_test_keys = 10000;
    for (size_t i = 0; i < num_test_keys; i++) {
      keys_to_test.push_back("test_" + std::to_string(i));
    }
    
    // Test different values of k and track false positive rates
    std::map<size_t, double> k_to_fp_rate;
    
    for (size_t k = 1; k <= 5; k++) {
      // Create bloom filter with specific m and k
      auto bf = BloomFilter::makeFixed(tc.m, k);
      
      // Insert all keys
      for (const auto& key : keys_to_insert) {
        bf->add(key);
      }
      
      // Count false positives
      size_t false_positives = 0;
      for (const auto& key : keys_to_test) {
        if (bf->contains(key)) {
          false_positives++;
        }
      }
      
      double fp_rate = static_cast<double>(false_positives) / num_test_keys;
      k_to_fp_rate[k] = fp_rate;
    }
    
    // Find k with minimum false positive rate
    size_t best_k = 1;
    double min_fp_rate = k_to_fp_rate[1];
    
    for (const auto& [k, fp_rate] : k_to_fp_rate) {
      if (fp_rate < min_fp_rate) {
        min_fp_rate = fp_rate;
        best_k = k;
      }
    }
    
    // Verify that the optimal k indeed gives the lowest false positive rate
    ASSERT_EQ(best_k, tc.optimal_k) 
        << "For m=" << tc.m << " n=" << tc.n 
        << ", expected optimal k=" << tc.optimal_k 
        << " but got k=" << best_k
        << " (FP rates: k=1:" << k_to_fp_rate[1]
        << ", k=2:" << k_to_fp_rate[2]
        << ", k=3:" << k_to_fp_rate[3]
        << ", k=4:" << k_to_fp_rate[4]
        << ", k=5:" << k_to_fp_rate[5] << ")";
  }
}

TEST(BloomFilterTest, SaveAndLoad) {
  std::string test_file = "/tmp/bloom_filter_test.bin";

  // Create and populate a bloom filter
  auto bf_original = BloomFilter::makeAutotuned(100, 0.01, false);

  std::vector<std::string> test_keys = {
      "apple", "banana", "cherry", "date", "elderberry",
      "fig", "grape", "honeydew", "kiwi", "lemon"};

  for (const auto &key : test_keys) {
    bf_original->add(key);
  }

  // Save to file
  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(*bf_original);
  }

  // Load from file
  auto bf_loaded = BloomFilter::makeFixed(1, 1); // Temporary, will be overwritten by deserialize
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*bf_loaded);
  }

  // Verify loaded filter works correctly
  for (const auto &key : test_keys) {
    ASSERT_TRUE(bf_loaded->contains(key))
        << "Loaded filter should contain key: " << key;
  }

  // Verify size and num_hashes match
  ASSERT_EQ(bf_loaded->size(), bf_original->size());
  ASSERT_EQ(bf_loaded->numHashes(), bf_original->numHashes());

  // Clean up
  std::remove(test_file.c_str());
}

} // namespace caramel::tests