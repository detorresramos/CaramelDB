#pragma once

#include <memory>

namespace caramel {

struct PreFilterConfig {
  virtual ~PreFilterConfig() = default;
};

using PreFilterConfigPtr = std::shared_ptr<PreFilterConfig>;

struct BloomPreFilterConfig : public PreFilterConfig {
  BloomPreFilterConfig() = default;
};

struct XORPreFilterConfig : public PreFilterConfig {
  XORPreFilterConfig() = default;
};

} // namespace caramel