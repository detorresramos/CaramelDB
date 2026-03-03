#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <memory>
#include <src/construct/Construct.h>
#include <src/construct/Csf.h>
#include <src/construct/filter/FilterTypes.h>

namespace caramel::tests {

TEST(ConstructCsfTest, QueriesDecodeCorrectly) {
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  std::vector<uint32_t> values = {1, 2, 3};
  CsfPtr<uint32_t> csf = constructCsf<uint32_t>(keys, values);

  for (uint32_t i = 0; i < keys.size(); i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
}

TEST(ConstructCsfTest, LargeScaleUint32) {
  size_t n = 1000;
  std::vector<std::string> keys;
  std::vector<uint32_t> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(static_cast<uint32_t>(i));
  }

  CsfPtr<uint32_t> csf = constructCsf<uint32_t>(keys, values, nullptr, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
}

TEST(ConstructCsfTest, Uint64Values) {
  size_t n = 200;
  std::vector<std::string> keys;
  std::vector<uint64_t> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(static_cast<uint64_t>(i) * 1000000000ULL);
  }

  CsfPtr<uint64_t> csf = constructCsf<uint64_t>(keys, values, nullptr, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
}

TEST(ConstructCsfTest, StringValues) {
  size_t n = 200;
  std::vector<std::string> keys;
  std::vector<std::string> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back("value_" + std::to_string(i));
  }

  CsfPtr<std::string> csf =
      constructCsf<std::string>(keys, values, nullptr, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
}

TEST(ConstructCsfTest, Char10Values) {
  size_t n = 200;
  std::vector<std::string> keys;
  std::vector<Char10> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    Char10 val{};
    std::string s = "v" + std::to_string(i);
    std::memcpy(val.data(), s.data(), std::min(s.size(), size_t(10)));
    values.push_back(val);
  }

  CsfPtr<Char10> csf = constructCsf<Char10>(keys, values, nullptr, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
}

TEST(ConstructCsfTest, Char12Values) {
  size_t n = 200;
  std::vector<std::string> keys;
  std::vector<Char12> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    Char12 val{};
    std::string s = "v" + std::to_string(i);
    std::memcpy(val.data(), s.data(), std::min(s.size(), size_t(12)));
    values.push_back(val);
  }

  CsfPtr<Char12> csf = constructCsf<Char12>(keys, values, nullptr, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
}

TEST(ConstructCsfTest, SaveAndLoadUint32) {
  std::ostringstream filename;
  filename << "/tmp/csf_test_uint32_" << getpid() << ".bin";
  const std::string test_file = filename.str();

  size_t n = 200;
  std::vector<std::string> keys;
  std::vector<uint32_t> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(static_cast<uint32_t>(i));
  }

  CsfPtr<uint32_t> csf = constructCsf<uint32_t>(keys, values, nullptr, false);
  csf->save(test_file);

  CsfPtr<uint32_t> loaded = Csf<uint32_t>::load(test_file);
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(loaded->query(keys[i]), values[i]);
  }

  std::remove(test_file.c_str());
}

TEST(ConstructCsfTest, SaveAndLoadString) {
  std::ostringstream filename;
  filename << "/tmp/csf_test_string_" << getpid() << ".bin";
  const std::string test_file = filename.str();

  size_t n = 200;
  std::vector<std::string> keys;
  std::vector<std::string> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back("value_" + std::to_string(i));
  }

  CsfPtr<std::string> csf =
      constructCsf<std::string>(keys, values, nullptr, false);
  csf->save(test_file);

  CsfPtr<std::string> loaded = Csf<std::string>::load(test_file);
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(loaded->query(keys[i]), values[i]);
  }

  std::remove(test_file.c_str());
}

TEST(ConstructCsfTest, SingleKey) {
  std::vector<std::string> keys = {"only_key"};
  std::vector<uint32_t> values = {42};
  CsfPtr<uint32_t> csf = constructCsf<uint32_t>(keys, values, nullptr, false);
  ASSERT_EQ(csf->query("only_key"), 42);
}

TEST(ConstructCsfTest, AllSameValues) {
  size_t n = 200;
  std::vector<std::string> keys;
  std::vector<uint32_t> values;
  keys.reserve(n);
  values.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(7);
  }

  CsfPtr<uint32_t> csf = constructCsf<uint32_t>(keys, values, nullptr, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), 7);
  }
}

} // namespace caramel::tests
