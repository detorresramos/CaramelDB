#include <gtest/gtest.h>
#include <fstream>
#include <cereal/archives/binary.hpp>
#include <src/construct/filter/XorFilter.h>

namespace caramel::tests {

TEST(XorFilterTest, SimpleAddAndCheck) {
  auto xf = XorFilter::make(100);

  std::vector<std::string> inserted_keys = {
      "apple",  "banana", "cherry",   "date",   "elderberry",
      "fig",    "grape",  "honeydew", "kiwi",   "lemon",
      "mango",  "nectarine", "orange", "papaya", "quince"};

  for (const auto &key : inserted_keys) {
    xf->add(key);
  }

  // Build the filter after adding all keys
  xf->build();

  for (const auto &key : inserted_keys) {
    ASSERT_TRUE(xf->contains(key))
        << "Key '" << key << "' must be in the filter (no false negatives)";
  }

  std::vector<std::string> not_inserted_keys = {
      "strawberry", "watermelon", "pear",   "plum",         "guava",
      "lychee",     "dragonfruit", "passionfruit", "starfruit", "durian"};

  int false_positives = 0;
  for (const auto &key : not_inserted_keys) {
    if (xf->contains(key)) {
      false_positives++;
    }
  }

  // Xor filters have ~0.39% FPR, so with 10 queries we expect 0-1 false positives
  ASSERT_LE(false_positives, 2)
      << "Too many false positives for xor filter with "
      << not_inserted_keys.size() << " queries";
}

TEST(XorFilterTest, SaveAndLoad) {
  std::string test_file = "/tmp/xor_filter_test.bin";

  // Create and populate a xor filter
  auto xf_original = XorFilter::make(100);

  std::vector<std::string> test_keys = {
      "apple", "banana", "cherry", "date", "elderberry",
      "fig", "grape", "honeydew", "kiwi", "lemon"};

  for (const auto &key : test_keys) {
    xf_original->add(key);
  }
  xf_original->build();

  // Save to file
  {
    std::ofstream ofs(test_file, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(ofs);
    oarchive(*xf_original);
  }

  // Load from file
  auto xf_loaded = XorFilter::make(1); // Temporary, will be overwritten by deserialize
  {
    std::ifstream ifs(test_file, std::ios::binary);
    cereal::BinaryInputArchive iarchive(ifs);
    iarchive(*xf_loaded);
  }

  // Verify loaded filter works correctly
  for (const auto &key : test_keys) {
    ASSERT_TRUE(xf_loaded->contains(key))
        << "Loaded filter should contain key: " << key;
  }

  // Verify size matches
  ASSERT_EQ(xf_loaded->size(), xf_original->size());
  ASSERT_EQ(xf_loaded->numElements(), xf_original->numElements());

  // Clean up
  std::remove(test_file.c_str());
}

} // namespace caramel::tests
