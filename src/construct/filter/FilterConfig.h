#pragma once

#include <memory>
#include <optional>

namespace caramel {

struct PreFilterConfig {
  virtual ~PreFilterConfig() = default;
};

using PreFilterConfigPtr = std::shared_ptr<PreFilterConfig>;

struct BloomPreFilterConfig : public PreFilterConfig {
  BloomPreFilterConfig(std::optional<float> error_rate = std::nullopt,
                       std::optional<size_t> k = std::nullopt)
      : error_rate(error_rate), k(k) {}

  std::optional<float> error_rate;
  std::optional<size_t> k;
};

struct XORPreFilterConfig : public PreFilterConfig {
  XORPreFilterConfig() = default;
};

} // namespace caramel