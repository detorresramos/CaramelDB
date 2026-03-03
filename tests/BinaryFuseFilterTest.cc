#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <cereal/archives/binary.hpp>
#include <src/construct/filter/BinaryFuseFilter.h>

namespace caramel::tests {

namespace {

std::vector<std::string> getTestKeys(size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; i++) {
    keys.push_back("key_" + std::to_string(i));
  }
  return keys;
}

std::vector<std::string> getNonInsertedKeys(size_t count, size_t offset) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; i++) {
    keys.push_back("not_inserted_" + std::to_string(i + offset));
  }
  return keys;
}

std::string tmpFile(const std::string &name) {
  std::ostringstream oss;
  oss << "/tmp/" << name << "_" << getpid() << ".bin";
  return oss.str();
}

} // namespace

TEST(BinaryFuseFilterTest, SimpleAddAndCheck) {
  auto bf = BinaryFuseFilter::make(100);

  std::vector<std::string> inserted_keys = {
      "apple",  "banana", "cherry",   "date",   "elderberry",
      "fig",    "grape",  "honeydew", "kiwi",   "lemon",
      "mango",  "nectarine", "orange", "papaya", "quince"};

  for (const auto &key : inserted_keys) {
    bf->add(key);
  }

  bf->build();

  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(bf->contains(key))
        << "Key '" << key << "' must be in the filter (no false negatives)";
  }

  std::vector<std::string> not_inserted_keys = {
      "strawberry", "watermelon", "pear",   "plum",         "guava",
      "lychee",     "dragonfruit", "passionfruit", "starfruit", "durian"};

  int false_positives = 0;
  for (const auto &key : not_inserted_keys) {
    if (bf->contains(key)) {
      false_positives++;
    }
  }

  ASSERT_LE(false_positives, 2)
      << "Too many false positives for binary fuse filter with "
      << not_inserted_keys.size() << " queries";
}

TEST(BinaryFuseFilterTest, SaveAndLoad) {
  std::string test_file = tmpFile("binary_fuse_filter_test");

  auto bf_original = BinaryFuseFilter::make(100);

  std::vector<std::string> test_keys = {
      "apple",  "banana", "cherry",   "date",   "elderberry",
      "fig",    "grape",  "honeydew", "kiwi",   "lemon",
      "mango",  "nectarine", "orange", "papaya", "quince"};

  for (const auto &key : test_keys) {
    bf_original->add(key);
  }

  bf_original->build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(*bf_original);
  }

  auto bf_loaded = BinaryFuseFilter::make(1);
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*bf_loaded);
  }

  for (const auto &key : test_keys) {
    ASSERT_TRUE(bf_loaded->contains(key))
        << "Loaded filter should contain key: " << key;
  }

  ASSERT_EQ(bf_loaded->size(), bf_original->size());
  ASSERT_EQ(bf_loaded->numElements(), bf_original->numElements());

  std::remove(test_file.c_str());
}

// --- FPR sweep tests ---

TEST(BinaryFuseFilterTest, FourBitFingerprint) {
  float error_rate = 0.0625f;
  auto bf = BinaryFuseFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    bf.add(key);
  }
  bf.build();

  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(bf.contains(key))
        << "4-bit filter must not have false negatives";
  }

  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (bf.contains(key)) {
      false_positives++;
    }
  }
  float actual_fpr = static_cast<float>(false_positives) / not_inserted.size();
  ASSERT_GT(actual_fpr, 0.01f) << "4-bit FPR too low";
  ASSERT_LT(actual_fpr, 0.12f) << "4-bit FPR too high";
}

TEST(BinaryFuseFilterTest, SevenBitFingerprint) {
  float error_rate = 0.0078f;
  auto bf = BinaryFuseFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    bf.add(key);
  }
  bf.build();

  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(bf.contains(key))
        << "7-bit filter must not have false negatives";
  }

  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (bf.contains(key)) {
      false_positives++;
    }
  }
  ASSERT_LE(false_positives, 25) << "7-bit filter has too many false positives";
}

TEST(BinaryFuseFilterTest, EightBitFingerprint) {
  float error_rate = 0.0039f;
  auto bf = BinaryFuseFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    bf.add(key);
  }
  bf.build();

  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(bf.contains(key))
        << "8-bit filter must not have false negatives";
  }

  auto not_inserted = getNonInsertedKeys(1000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (bf.contains(key)) {
      false_positives++;
    }
  }
  ASSERT_LE(false_positives, 15) << "8-bit filter has too many false positives";
}

TEST(BinaryFuseFilterTest, SixteenBitFingerprint) {
  float error_rate = 0.00002f;
  auto bf = BinaryFuseFilter::create(200, error_rate);

  auto inserted_keys = getTestKeys(200);
  for (const auto &key : inserted_keys) {
    bf.add(key);
  }
  bf.build();

  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(bf.contains(key))
        << "16-bit filter must not have false negatives";
  }

  auto not_inserted = getNonInsertedKeys(10000, 0);
  int false_positives = 0;
  for (const auto &key : not_inserted) {
    if (bf.contains(key)) {
      false_positives++;
    }
  }
  ASSERT_LE(false_positives, 10)
      << "16-bit filter has too many false positives";
}

// --- Save/Load per width ---

TEST(BinaryFuseFilterTest, SaveAndLoadFourBit) {
  std::string test_file = tmpFile("bf_4bit_test");
  auto test_keys = getTestKeys(150);

  auto bf_original = BinaryFuseFilter::create(150, 0.0625f);
  for (const auto &key : test_keys) {
    bf_original.add(key);
  }
  bf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(bf_original);
  }

  auto bf_loaded = BinaryFuseFilter::make(1);
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*bf_loaded);
  }

  for (const auto &key : test_keys) {
    ASSERT_TRUE(bf_loaded->contains(key))
        << "Loaded 4-bit filter should contain: " << key;
  }
  ASSERT_EQ(bf_loaded->size(), bf_original.size());
  ASSERT_EQ(bf_loaded->numElements(), bf_original.numElements());

  std::remove(test_file.c_str());
}

TEST(BinaryFuseFilterTest, SaveAndLoadSevenBit) {
  std::string test_file = tmpFile("bf_7bit_test");
  auto test_keys = getTestKeys(150);

  auto bf_original = BinaryFuseFilter::create(150, 0.0078f);
  for (const auto &key : test_keys) {
    bf_original.add(key);
  }
  bf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(bf_original);
  }

  auto bf_loaded = BinaryFuseFilter::make(1);
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*bf_loaded);
  }

  for (const auto &key : test_keys) {
    ASSERT_TRUE(bf_loaded->contains(key))
        << "Loaded 7-bit filter should contain: " << key;
  }
  ASSERT_EQ(bf_loaded->size(), bf_original.size());
  ASSERT_EQ(bf_loaded->numElements(), bf_original.numElements());

  std::remove(test_file.c_str());
}

TEST(BinaryFuseFilterTest, SaveAndLoadEightBit) {
  std::string test_file = tmpFile("bf_8bit_test");
  auto test_keys = getTestKeys(150);

  auto bf_original = BinaryFuseFilter::create(150, 0.0039f);
  for (const auto &key : test_keys) {
    bf_original.add(key);
  }
  bf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(bf_original);
  }

  auto bf_loaded = BinaryFuseFilter::make(1);
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*bf_loaded);
  }

  for (const auto &key : test_keys) {
    ASSERT_TRUE(bf_loaded->contains(key))
        << "Loaded 8-bit filter should contain: " << key;
  }
  ASSERT_EQ(bf_loaded->size(), bf_original.size());
  ASSERT_EQ(bf_loaded->numElements(), bf_original.numElements());

  std::remove(test_file.c_str());
}

TEST(BinaryFuseFilterTest, SaveAndLoadSixteenBit) {
  std::string test_file = tmpFile("bf_16bit_test");
  auto test_keys = getTestKeys(150);

  auto bf_original = BinaryFuseFilter::create(150, 0.00002f);
  for (const auto &key : test_keys) {
    bf_original.add(key);
  }
  bf_original.build();

  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(bf_original);
  }

  auto bf_loaded = BinaryFuseFilter::make(1);
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*bf_loaded);
  }

  for (const auto &key : test_keys) {
    ASSERT_TRUE(bf_loaded->contains(key))
        << "Loaded 16-bit filter should contain: " << key;
  }
  ASSERT_EQ(bf_loaded->size(), bf_original.size());
  ASSERT_EQ(bf_loaded->numElements(), bf_original.numElements());

  std::remove(test_file.c_str());
}

// --- Edge case ---

TEST(BinaryFuseFilterTest, MinimumElementCount) {
  // 15 keys should build OK
  auto bf = BinaryFuseFilter::make(100);
  auto keys = getTestKeys(15);
  for (const auto &key : keys) {
    bf->add(key);
  }
  ASSERT_NO_THROW(bf->build());

  for (const auto &key : keys) {
    ASSERT_TRUE(bf->contains(key));
  }

  // 5 keys should throw
  auto bf_small = BinaryFuseFilter::make(100);
  auto small_keys = getTestKeys(5);
  for (const auto &key : small_keys) {
    bf_small->add(key);
  }
  ASSERT_THROW(bf_small->build(), std::invalid_argument);
}

} // namespace caramel::tests
