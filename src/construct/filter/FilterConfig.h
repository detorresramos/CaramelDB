#pragma once

#include <memory>
#include <stdexcept>

namespace caramel {

struct PreFilterConfig {
  virtual ~PreFilterConfig() = default;
};

using PreFilterConfigPtr = std::shared_ptr<PreFilterConfig>;

struct BloomPreFilterConfig : public PreFilterConfig {
  BloomPreFilterConfig(size_t size, size_t num_hashes)
      : size(size), num_hashes(num_hashes) {
    if (size == 0) {
      throw std::invalid_argument("Bloom filter size must be > 0");
    }
    if (num_hashes == 0) {
      throw std::invalid_argument("Bloom filter num_hashes must be > 0");
    }
  }

  size_t size;       // Total bitarray size in bits
  size_t num_hashes; // Number of hash functions
};

struct XORPreFilterConfig : public PreFilterConfig {
  explicit XORPreFilterConfig(int fingerprint_bits)
      : fingerprint_bits(fingerprint_bits) {
    if (fingerprint_bits < 1 || fingerprint_bits > 32) {
      throw std::invalid_argument(
          "XOR filter fingerprint_bits must be between 1 and 32");
    }
  }

  int fingerprint_bits;
};

struct BinaryFusePreFilterConfig : public PreFilterConfig {
  explicit BinaryFusePreFilterConfig(int fingerprint_bits)
      : fingerprint_bits(fingerprint_bits) {
    if (fingerprint_bits < 1 || fingerprint_bits > 32) {
      throw std::invalid_argument(
          "Binary fuse filter fingerprint_bits must be between 1 and 32");
    }
  }

  int fingerprint_bits;
};

} // namespace caramel
