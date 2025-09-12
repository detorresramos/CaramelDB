#pragma once

#include <memory>
#include <optional>

namespace caramel {

struct PreFilterConfig {
  virtual ~PreFilterConfig() = default;
};

using PreFilterConfigPtr = std::shared_ptr<PreFilterConfig>;

struct BloomPreFilterConfig : public PreFilterConfig {
  BloomPreFilterConfig(std::optional<float> error_rate = std::nullopt) 
      : error_rate(error_rate) {}
  
  std::optional<float> error_rate;
};

struct XORPreFilterConfig : public PreFilterConfig {
  XORPreFilterConfig() = default;
};

} // namespace caramel