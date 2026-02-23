#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace caramel {

class UnorderedMapBaseline {
public:
  UnorderedMapBaseline(const std::vector<std::string> &keys,
                       const std::vector<uint32_t> &values) {
    _map.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
      _map[keys[i]] = values[i];
    }
  }

  uint32_t query(const std::string &key) const { return _map.at(key); }

  size_t size() const { return _map.size(); }

private:
  std::unordered_map<std::string, uint32_t> _map;
};

} // namespace caramel
