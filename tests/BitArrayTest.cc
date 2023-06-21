#include <gtest/gtest.h>
#include <memory>
#include <src/BitArray.h>

namespace caramel::tests {

TEST(BitArrayTest, TestFind) {
  uint32_t num_bits = 18;
  BitArray bitarray = BitArray(num_bits);

  for (uint32_t i = 0; i < num_bits; i++) {
    bitarray.setBit(i);
    ASSERT_EQ(bitarray.find(), i);
    bitarray.clearAll();
  }
}

} // namespace caramel::tests