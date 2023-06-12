#include <gtest/gtest.h>
#include <memory>
#include <src/CSF.h>

namespace caramel::tests {

TEST(CSFTest, SomeExampleNameHere) {
  auto x = CSF();

  bool result = x.helloWorld();

  ASSERT_EQ(result, true);
}

} // namespace caramel::tests