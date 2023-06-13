#include <gtest/gtest.h>
#include <memory>
#include <src/Dummy.h>

namespace caramel::tests {

TEST(DummyTest, SomeExampleNameHere) {
  auto x = Dummy();

  bool result = x.helloWorld();

  ASSERT_EQ(result, true);
}

} // namespace caramel::tests