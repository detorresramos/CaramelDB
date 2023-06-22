#include <gtest/gtest.h>
#include <memory>
#include <src/BitArray.h>

namespace caramel::tests {

TEST(BitArrayTest, TestSingleBitModifications) {
  uint32_t num_bits = 18;
  BitArray bitarray = BitArray(num_bits);
  ASSERT_FALSE(bitarray.any());

  bitarray.setBit(8);
  ASSERT_TRUE(bitarray[8]);
  ASSERT_TRUE(bitarray.any());

  bitarray.clearBit(8);
  ASSERT_FALSE(bitarray.any());
  ASSERT_FALSE(bitarray[8]);
}

TEST(BitArrayTest, TestOutOfBoundsBitAccess) {
  uint32_t num_bits = 18;
  BitArray bitarray = BitArray(num_bits);

  ASSERT_ANY_THROW(bitarray.setBit(18));
  ASSERT_ANY_THROW(bitarray.clearBit(18));
}

TEST(BitArrayTest, TestFind) {
  uint32_t num_bits = 18;
  BitArray bitarray = BitArray(num_bits);

  for (uint32_t i = 0; i < num_bits; i++) {
    bitarray.setBit(i);
    ASSERT_EQ(bitarray.find(), i);
    bitarray.clearAll();
  }
}

TEST(BitArrayTest, TestXorEquals) {
  uint32_t num_bits = 18;
  BitArray bitarray1 = BitArray(num_bits);
  bitarray1.setBit(3);

  BitArray bitarray2 = BitArray(num_bits);
  bitarray2.setBit(3);
  bitarray2.setBit(4);

  bitarray1 ^= bitarray2;

  ASSERT_FALSE(bitarray1[3]);
  ASSERT_TRUE(bitarray1[4]);
}

TEST(BitArrayTest, TestAndEquals) {
  uint32_t num_bits = 18;
  BitArray bitarray1 = BitArray(num_bits);
  bitarray1.setBit(3);

  BitArray bitarray2 = BitArray(num_bits);
  bitarray2.setBit(3);
  bitarray2.setBit(4);

  bitarray1 &= bitarray2;

  ASSERT_TRUE(bitarray1[3]);
  ASSERT_FALSE(bitarray1[4]);
}

TEST(BitArrayTest, TestScalarProduct) {
  uint32_t num_bits = 7;
  BitArrayPtr bitarray1 = BitArray::make(num_bits);
  bitarray1->setBit(3);

  BitArrayPtr bitarray2 = BitArray::make(num_bits);
  bitarray2->setBit(3);
  bitarray2->setBit(4);

  uint32_t product = BitArray::scalarProduct(bitarray1, bitarray2);
  ASSERT_EQ(product, 1);

  bitarray2->setBit(5);
  product = BitArray::scalarProduct(bitarray1, bitarray2);
  ASSERT_EQ(product, 1);

  bitarray1->setBit(5);
  product = BitArray::scalarProduct(bitarray1, bitarray2);
  ASSERT_EQ(product, 0);
}

TEST(BitArrayTest, TestToString) {
  uint32_t num_bits = 7;
  BitArrayPtr bitarray = BitArray::make(num_bits);
  bitarray->setBit(3);

  ASSERT_EQ(bitarray->str(), "0001000");
}

} // namespace caramel::tests