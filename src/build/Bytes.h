#pragma once

#include <cereal/archives/binary.hpp>
#include <sstream>
#include <string>

namespace caramel::build {

// Step inputs and outputs travel between processes as opaque byte strings.
template <typename Obj> std::string toBytes(const Obj &obj) {
  std::ostringstream stream(std::ios::binary);
  {
    cereal::BinaryOutputArchive archive(stream);
    archive(obj);
  }
  return stream.str();
}

template <typename Obj> Obj fromBytes(const std::string &bytes) {
  std::istringstream stream(bytes, std::ios::binary);
  cereal::BinaryInputArchive archive(stream);
  Obj obj;
  archive(obj);
  return obj;
}

} // namespace caramel::build
