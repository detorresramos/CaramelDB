#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <cereal/archives/binary.hpp>
#include <src/construct/filter/BinaryFuseFilter.h>

namespace caramel::tests {

TEST(BinaryFuseFilterTest, SimpleAddAndCheck) {
  auto bf = BinaryFuseFilter::make(100);

  std::vector<std::string> inserted_keys = {
      "apple",  "banana", "cherry",   "date",   "elderberry",
      "fig",    "grape",  "honeydew", "kiwi",   "lemon",
      "mango",  "nectarine", "orange", "papaya", "quince"};

  for (const auto &key : inserted_keys) {
    bf->add(key);
  }

  // Build the filter after adding all keys
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

  // Binary fuse filters have ~0.39% FPR, so with 10 queries we expect 0-1 false positives
  ASSERT_LE(false_positives, 2)
      << "Too many false positives for binary fuse filter with "
      << not_inserted_keys.size() << " queries";
}

TEST(BinaryFuseFilterTest, SaveAndLoad) {
  // Use unique temp file to avoid parallel test conflicts
  std::ostringstream filename;
  filename << "/tmp/binary_fuse_filter_test_" << getpid() << ".bin";
  std::string test_file = filename.str();

  // Create and populate a binary fuse filter
  auto bf_original = BinaryFuseFilter::make(100);

  // Use same 15 keys as SimpleAddAndCheck to see if key count matters
  std::vector<std::string> test_keys = {
      "apple",  "banana", "cherry",   "date",   "elderberry",
      "fig",    "grape",  "honeydew", "kiwi",   "lemon",
      "mango",  "nectarine", "orange", "papaya", "quince"};

  for (const auto &key : test_keys) {
    bf_original->add(key);
  }

  bf_original->build();

  // Save to file
  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(*bf_original);
  }

  // Load from file
  auto bf_loaded = BinaryFuseFilter::make(1);
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

  // Verify size matches
  ASSERT_EQ(bf_loaded->size(), bf_original->size());
  ASSERT_EQ(bf_loaded->numElements(), bf_original->numElements());

  // Clean up
  std::remove(test_file.c_str());
}

} // namespace caramel::tests
