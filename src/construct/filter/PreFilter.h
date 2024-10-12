#pragma once

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/polymorphic.hpp>
#include <string>
#include <vector>

namespace caramel {

template <typename T> class PreFilter {
public:
  virtual void apply(std::vector<std::string> &keys, std::vector<T> &values,
                     float delta, bool verbose) = 0;

  virtual std::optional<T> contains(const std::string &key) = 0;

  virtual ~PreFilter() = default;

private:
  friend class cereal::access;
  template <typename Archive> void serialize(Archive &archive) {
    (void)archive;
  }
};

template <typename T> using PreFilterPtr = std::shared_ptr<PreFilter<T>>;

} // namespace caramel
