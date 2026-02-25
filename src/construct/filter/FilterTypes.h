#pragma once

#include <array>
#include <cereal/types/polymorphic.hpp>

namespace caramel {

using Char10 = std::array<char, 10>;
using Char12 = std::array<char, 12>;

} // namespace caramel

#define CARAMEL_REGISTER_PREFILTER(FilterClass)                                \
  CEREAL_REGISTER_TYPE(caramel::FilterClass<uint32_t>)                         \
  CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint32_t>,           \
                                       caramel::FilterClass<uint32_t>)         \
  CEREAL_REGISTER_TYPE(caramel::FilterClass<uint64_t>)                         \
  CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint64_t>,           \
                                       caramel::FilterClass<uint64_t>)         \
  CEREAL_REGISTER_TYPE(caramel::FilterClass<caramel::Char10>)                  \
  CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<caramel::Char10>,    \
                                       caramel::FilterClass<caramel::Char10>)  \
  CEREAL_REGISTER_TYPE(caramel::FilterClass<std::string>)                      \
  CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<std::string>,        \
                                       caramel::FilterClass<std::string>)      \
  CEREAL_REGISTER_TYPE(caramel::FilterClass<caramel::Char12>)                  \
  CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<caramel::Char12>,    \
                                       caramel::FilterClass<caramel::Char12>)
