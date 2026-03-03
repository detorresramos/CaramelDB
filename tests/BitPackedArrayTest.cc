#include <gtest/gtest.h>
#include <src/construct/filter/BitPackedArray.h>

namespace caramel::tests {

TEST(BitPackedArrayTest, SetGetVariousBitWidths) {
  std::vector<int> widths = {1, 2, 4, 7, 8, 16, 32};
  size_t num_elements = 100;

  for (int width : widths) {
    BitPackedArray arr(num_elements, width);
    uint64_t max_val = (1ULL << width) - 1;

    for (size_t i = 0; i < num_elements; i++) {
      arr.set(i, i % (max_val + 1));
    }

    for (size_t i = 0; i < num_elements; i++) {
      ASSERT_EQ(arr.get(i), i % (max_val + 1))
          << "Mismatch at index " << i << " with width " << width;
    }
  }
}

TEST(BitPackedArrayTest, CrossWordBoundary) {
  // 7-bit array: index 9 starts at bit 63, index 18 starts at bit 126
  BitPackedArray arr(20, 7);

  // Set values around word boundaries
  for (size_t i = 0; i < 20; i++) {
    arr.set(i, i % 128);
  }

  // Index 9: bit_pos = 63, spans words 0 and 1
  ASSERT_EQ(arr.get(9), 9);
  // Index 18: bit_pos = 126, spans words 1 and 2
  ASSERT_EQ(arr.get(18), 18);

  // Verify neighbors aren't corrupted
  ASSERT_EQ(arr.get(8), 8);
  ASSERT_EQ(arr.get(10), 10);
  ASSERT_EQ(arr.get(17), 17);
  ASSERT_EQ(arr.get(19), 19);
}

TEST(BitPackedArrayTest, XorAt) {
  BitPackedArray arr(20, 7);
  arr.set(5, 0b1010101);
  arr.xorAt(5, 0b1100110);
  ASSERT_EQ(arr.get(5), 0b0110011);

  // XorAt on a cross-word-boundary index
  arr.set(9, 0b1111000);
  arr.xorAt(9, 0b0001111);
  ASSERT_EQ(arr.get(9), 0b1110111);
}

TEST(BitPackedArrayTest, Metadata) {
  BitPackedArray arr(20, 7);
  ASSERT_EQ(arr.size(), 20);
  ASSERT_EQ(arr.bitsPerElement(), 7);

  // 20 * 7 = 140 bits -> ceil(140/64) = 3 words
  ASSERT_EQ(arr.numWords(), 3);
  ASSERT_EQ(arr.sizeInBytes(), 3 * sizeof(uint64_t));
  ASSERT_NE(arr.data(), nullptr);
}

TEST(BitPackedArrayTest, SingleElement) {
  BitPackedArray arr(1, 8);
  arr.set(0, 42);
  ASSERT_EQ(arr.get(0), 42);
}

TEST(BitPackedArrayTest, MaxValueForBitWidth) {
  std::vector<int> widths = {1, 2, 4, 7, 8, 16, 32};

  for (int width : widths) {
    BitPackedArray arr(1, width);
    uint64_t max_val = (1ULL << width) - 1;
    arr.set(0, max_val);
    ASSERT_EQ(arr.get(0), max_val) << "Failed for width " << width;
  }
}

TEST(BitPackedArrayTest, SetMasksOverflow) {
  BitPackedArray arr(1, 4);
  // 0b11111 = 31, but 4-bit mask is 0b1111 = 15, so set() masks to 15
  arr.set(0, 0b11111);
  ASSERT_EQ(arr.get(0), 0b1111);
}

TEST(BitPackedArrayTest, InvalidBitsPerElement) {
  ASSERT_THROW(BitPackedArray(10, 0), std::invalid_argument);
  ASSERT_THROW(BitPackedArray(10, 33), std::invalid_argument);
}

} // namespace caramel::tests
