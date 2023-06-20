#include <gtest/gtest.h>
#include <memory>
#include <src/BitArray.h>

namespace caramel::tests {

TEST(BitArrayTest, TestFind) {
  BitArray bitarray = BitArray(5);

  bitarray.setBit(1);
  ASSERT_EQ(bitarray.find(), 1);
}

} // namespace caramel::tests