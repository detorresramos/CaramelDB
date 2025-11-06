#include <gtest/gtest.h>
#include <fstream>
#include <cmath>
#include <cereal/archives/binary.hpp>
#include <src/construct/filter/XorFilter.h>

namespace caramel::tests {

// Helper function to create test keys
std::vector<std::string> getTestKeys(size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; i++) {
    keys.push_back("key_" + std::to_string(i));
  }
  return keys;
}

// Helper function to create non-inserted keys
std::vector<std::string> getNonInsertedKeys(size_t count, size_t offset) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; i++) {
    keys.push_back("not_inserted_" + std::to_string(i + offset));
  }
  return keys;
}

// Test 1-bit fingerprints (FPR ≈ 50%)
TEST(BitPackedXorFilterTest, OneBitFingerprint) {
  // For 1-bit: ε ≈ 1/2^1 = 0.5 (50% FPR)
  float error_rate = 0.5f;
  auto xf = XorFilter::create(100, error_rate);

  auto inserted_keys = getTestKeys(100);
  for (const auto &key : inserted_keys) {
    xf.add(key);
  }
  xf.build();

  // No false negatives
  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(xf.contains(key))
        << "1-bit filter must not have false negatives";
  }

  // Check FPR (with 1-bit, expect ~50% false positives)
  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (xf.contains(key)) {
      false_positives++;
    }
  }
  float actual_fpr = static_cast<float>(false_positives) / not_inserted.size();

  // 1-bit should have FPR around 0.5 (allow ±0.15 for variance)
  ASSERT_GT(actual_fpr, 0.35f) << "1-bit FPR too low";
  ASSERT_LT(actual_fpr, 0.65f) << "1-bit FPR too high";
}

// Test 2-bit fingerprints (FPR ≈ 25%)
TEST(BitPackedXorFilterTest, TwoBitFingerprint) {
  // For 2-bit: ε ≈ 1/2^2 = 0.25 (25% FPR)
  float error_rate = 0.25f;
  auto xf = XorFilter::create(100, error_rate);

  auto inserted_keys = getTestKeys(100);
  for (const auto &key : inserted_keys) {
    xf.add(key);
  }
  xf.build();

  // No false negatives
  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(xf.contains(key))
        << "2-bit filter must not have false negatives";
  }

  // Check FPR
  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (xf.contains(key)) {
      false_positives++;
    }
  }
  float actual_fpr = static_cast<float>(false_positives) / not_inserted.size();

  // 2-bit should have FPR around 0.25 (allow ±0.1 for variance)
  ASSERT_GT(actual_fpr, 0.15f) << "2-bit FPR too low";
  ASSERT_LT(actual_fpr, 0.35f) << "2-bit FPR too high";
}

// Test 4-bit fingerprints (FPR ≈ 6.25%)
TEST(BitPackedXorFilterTest, FourBitFingerprint) {
  // For 4-bit: ε ≈ 1/2^4 = 0.0625 (6.25% FPR)
  float error_rate = 0.0625f;
  auto xf = XorFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    xf.add(key);
  }
  xf.build();

  // No false negatives
  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(xf.contains(key))
        << "4-bit filter must not have false negatives";
  }

  // Check FPR
  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (xf.contains(key)) {
      false_positives++;
    }
  }
  float actual_fpr = static_cast<float>(false_positives) / not_inserted.size();

  // 4-bit should have FPR around 0.0625 (allow ±0.05 for variance)
  ASSERT_GT(actual_fpr, 0.01f) << "4-bit FPR too low";
  ASSERT_LT(actual_fpr, 0.12f) << "4-bit FPR too high";
}

// Test 7-bit fingerprints (max bit-packed, FPR ≈ 0.78%)
TEST(BitPackedXorFilterTest, SevenBitFingerprint) {
  // For 7-bit: ε ≈ 1/2^7 = 0.0078 (0.78% FPR)
  float error_rate = 0.0078f;
  auto xf = XorFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    xf.add(key);
  }
  xf.build();

  // No false negatives
  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(xf.contains(key))
        << "7-bit filter must not have false negatives";
  }

  // Check FPR (should be low)
  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (xf.contains(key)) {
      false_positives++;
    }
  }

  // With 0.78% FPR and 1000 queries, expect 0-20 false positives
  ASSERT_LE(false_positives, 25)
      << "7-bit filter has too many false positives";
}

// Test 8-bit fingerprints (standard uint8_t, FPR ≈ 0.39%)
TEST(BitPackedXorFilterTest, EightBitFingerprint) {
  // For 8-bit: ε ≈ 1/2^8 = 0.0039 (0.39% FPR)
  float error_rate = 0.0039f;
  auto xf = XorFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    xf.add(key);
  }
  xf.build();

  // No false negatives
  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(xf.contains(key))
        << "8-bit filter must not have false negatives";
  }

  // Check FPR (should be very low)
  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (xf.contains(key)) {
      false_positives++;
    }
  }

  // With 0.39% FPR and 1000 queries, expect 0-15 false positives
  ASSERT_LE(false_positives, 15)
      << "8-bit filter has too many false positives";
}

// Test 16-bit fingerprints (standard uint16_t)
TEST(BitPackedXorFilterTest, SixteenBitFingerprint) {
  float error_rate = 0.00002f; // Requires 16 bits
  auto xf = XorFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    xf.add(key);
  }
  xf.build();

  // No false negatives
  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(xf.contains(key))
        << "16-bit filter must not have false negatives";
  }

  // Check FPR (should be extremely low)
  auto not_inserted = getNonInsertedKeys(10000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (xf.contains(key)) {
      false_positives++;
    }
  }

  // With such low FPR, expect 0-5 false positives in 10k queries
  ASSERT_LE(false_positives, 10)
      << "16-bit filter has too many false positives";
}

// Test save and load for 1-bit fingerprints
TEST(BitPackedXorFilterTest, SaveAndLoadOneBit) {
  std::string test_file = "/tmp/xor_filter_1bit_test.bin";

  auto test_keys = getTestKeys(100);

  // Create and save
  auto xf_original = XorFilter::create(100, 0.5f); // 1-bit
  for (const auto &key : test_keys) {
    xf_original.add(key);
  }
  xf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(xf_original);
  }

  // Load
  auto xf_loaded = XorFilter::make(1); // Temporary, will be overwritten by deserialize
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*xf_loaded);
  }

  // Verify
  for (const auto &key : test_keys) {
    ASSERT_TRUE(xf_loaded->contains(key))
        << "Loaded 1-bit filter should contain: " << key;
  }

  ASSERT_EQ(xf_loaded->size(), xf_original.size());
  ASSERT_EQ(xf_loaded->numElements(), xf_original.numElements());

  std::remove(test_file.c_str());
}

// Test save and load for 4-bit fingerprints
TEST(BitPackedXorFilterTest, SaveAndLoadFourBit) {
  std::string test_file = "/tmp/xor_filter_4bit_test.bin";

  auto test_keys = getTestKeys(150);

  // Create and save
  auto xf_original = XorFilter::create(150, 0.0625f); // 4-bit
  for (const auto &key : test_keys) {
    xf_original.add(key);
  }
  xf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(xf_original);
  }

  // Load
  auto xf_loaded = XorFilter::make(1); // Temporary, will be overwritten by deserialize
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*xf_loaded);
  }

  // Verify
  for (const auto &key : test_keys) {
    ASSERT_TRUE(xf_loaded->contains(key))
        << "Loaded 4-bit filter should contain: " << key;
  }

  ASSERT_EQ(xf_loaded->size(), xf_original.size());
  ASSERT_EQ(xf_loaded->numElements(), xf_original.numElements());

  std::remove(test_file.c_str());
}

// Test save and load for 7-bit fingerprints
TEST(BitPackedXorFilterTest, SaveAndLoadSevenBit) {
  std::string test_file = "/tmp/xor_filter_7bit_test.bin";

  auto test_keys = getTestKeys(150);

  // Create and save
  auto xf_original = XorFilter::create(150, 0.0078f); // 7-bit
  for (const auto &key : test_keys) {
    xf_original.add(key);
  }
  xf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(xf_original);
  }

  // Load
  auto xf_loaded = XorFilter::make(1); // Temporary, will be overwritten by deserialize
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*xf_loaded);
  }

  // Verify
  for (const auto &key : test_keys) {
    ASSERT_TRUE(xf_loaded->contains(key))
        << "Loaded 7-bit filter should contain: " << key;
  }

  ASSERT_EQ(xf_loaded->size(), xf_original.size());
  ASSERT_EQ(xf_loaded->numElements(), xf_original.numElements());

  std::remove(test_file.c_str());
}

// Test save and load for 8-bit fingerprints
TEST(BitPackedXorFilterTest, SaveAndLoadEightBit) {
  std::string test_file = "/tmp/xor_filter_8bit_test.bin";

  auto test_keys = getTestKeys(150);

  // Create and save
  auto xf_original = XorFilter::create(150, 0.0039f); // 8-bit
  for (const auto &key : test_keys) {
    xf_original.add(key);
  }
  xf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(xf_original);
  }

  // Load
  auto xf_loaded = XorFilter::make(1); // Temporary, will be overwritten by deserialize
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*xf_loaded);
  }

  // Verify
  for (const auto &key : test_keys) {
    ASSERT_TRUE(xf_loaded->contains(key))
        << "Loaded 8-bit filter should contain: " << key;
  }

  ASSERT_EQ(xf_loaded->size(), xf_original.size());
  ASSERT_EQ(xf_loaded->numElements(), xf_original.numElements());

  std::remove(test_file.c_str());
}

// Test save and load for 16-bit fingerprints
TEST(BitPackedXorFilterTest, SaveAndLoadSixteenBit) {
  std::string test_file = "/tmp/xor_filter_16bit_test.bin";

  auto test_keys = getTestKeys(150);

  // Create and save
  auto xf_original = XorFilter::create(150, 0.00002f); // 16-bit
  for (const auto &key : test_keys) {
    xf_original.add(key);
  }
  xf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(xf_original);
  }

  // Load
  auto xf_loaded = XorFilter::make(1); // Temporary, will be overwritten by deserialize
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*xf_loaded);
  }

  // Verify
  for (const auto &key : test_keys) {
    ASSERT_TRUE(xf_loaded->contains(key))
        << "Loaded 16-bit filter should contain: " << key;
  }

  ASSERT_EQ(xf_loaded->size(), xf_original.size());
  ASSERT_EQ(xf_loaded->numElements(), xf_original.numElements());

  std::remove(test_file.c_str());
}

// Test edge case: empty filter
TEST(BitPackedXorFilterTest, EmptyFilter) {
  auto xf = XorFilter::create(1, 0.0039f);

  // Don't add any keys, just build
  xf.build();

  // Should not contain any keys
  ASSERT_FALSE(xf.contains("test"));
  ASSERT_FALSE(xf.contains("another"));
}

// Test edge case: single element
TEST(BitPackedXorFilterTest, SingleElement) {
  auto xf = XorFilter::create(1, 0.0625f); // 4-bit

  xf.add("single_key");
  xf.build();

  ASSERT_TRUE(xf.contains("single_key"));

  // Check that it doesn't match everything (some FPR is expected)
  int false_positives = 0;
  auto not_inserted = getNonInsertedKeys(100, 0);
  for (const auto &key : not_inserted) {
    if (xf.contains(key)) {
      false_positives++;
    }
  }

  // Should have some false positives but not match everything
  ASSERT_LT(false_positives, 50) << "Too many false positives for single element";
}

} // namespace caramel::tests
