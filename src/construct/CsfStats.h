#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace caramel {

struct FilterStats {
  std::string type; // "bloom", "xor", "binary_fuse"
  size_t size_bytes;
  size_t num_elements;
  std::optional<size_t> num_hashes;    // Bloom only
  std::optional<size_t> size_bits;     // Bloom only
  std::optional<int> fingerprint_bits; // XOR/BinaryFuse only
};

struct HuffmanStats {
  size_t num_unique_symbols;
  uint32_t max_code_length;
  double avg_bits_per_symbol;
  std::vector<uint32_t> code_length_distribution;
};

struct BucketStats {
  size_t num_buckets;
  size_t total_solution_bits;
  double avg_solution_bits;
  size_t min_solution_bits;
  size_t max_solution_bits;
};

struct CsfStats {
  size_t in_memory_bytes;
  BucketStats bucket_stats;
  HuffmanStats huffman_stats;
  std::optional<FilterStats> filter_stats;
  double solution_bytes;
  double filter_bytes;
  double metadata_bytes;
};

} // namespace caramel
