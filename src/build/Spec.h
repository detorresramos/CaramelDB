#pragma once

#include <cereal/access.hpp>
#include <cstdint>
#include <memory>
#include <src/construct/Ribbon.h>
#include <src/construct/filter/FilterConfig.h>
#include <stdexcept>

namespace caramel::build {

// A serializable prefilter choice (PreFilterConfig is a polymorphic pointer).
struct FilterType {
  enum Kind : uint8_t { None, Bloom, Xor, BinaryFuse, Auto };
  Kind kind = None;
  uint32_t param_a = 0; // bits_per_element | fingerprint_bits
  uint32_t param_b = 0; // num_hashes (Bloom)

  static FilterType from(const PreFilterConfigPtr &config) {
    if (!config) {
      return {};
    }
    if (auto c = std::dynamic_pointer_cast<BloomPreFilterConfig>(config)) {
      return {Bloom, static_cast<uint32_t>(c->bits_per_element),
              static_cast<uint32_t>(c->num_hashes)};
    }
    if (auto c = std::dynamic_pointer_cast<XORPreFilterConfig>(config)) {
      return {Xor, static_cast<uint32_t>(c->fingerprint_bits), 0};
    }
    if (auto c = std::dynamic_pointer_cast<BinaryFusePreFilterConfig>(config)) {
      return {BinaryFuse, static_cast<uint32_t>(c->fingerprint_bits), 0};
    }
    if (std::dynamic_pointer_cast<AutoPreFilterConfig>(config)) {
      return {Auto, 0, 0};
    }
    throw std::invalid_argument("Unsupported filter configuration type");
  }

  // The concrete config for a non-Auto choice.
  PreFilterConfigPtr config() const {
    switch (kind) {
    case Bloom:
      return std::make_shared<BloomPreFilterConfig>(param_a, param_b);
    case Xor:
      return std::make_shared<XORPreFilterConfig>(param_a);
    case BinaryFuse:
      return std::make_shared<BinaryFusePreFilterConfig>(param_a);
    default:
      return nullptr;
    }
  }

  template <class Archive> void serialize(Archive &archive) {
    archive(kind, param_a, param_b);
  }
};

enum class Encoding : uint8_t {
  // One Huffman code per (row, column): Csf, MultisetCsf, RaggedMultisetCsf.
  Columns,
  // One code stream per row: the row's codes followed by STOP. PackedCsf.
  Packed,
};

// Huffman codebooks of a multi-column build. Auto picks whichever of one
// codebook per column or one shared by all columns makes the smaller index:
// per-column codes are shorter, but every column pays for its own symbol
// table, which dominates when columns have many distinct values and few rows.
enum class CodebookMode : uint8_t { PerColumn, Shared, Auto };

// Everything that shapes a build, identical on every worker. Each flag maps
// to one isolated piece of the pipeline:
//   filter          chooseFilters / findExceptions / buildFilters
//   shared_filter   chooseFilters (which columns share a filter)
//   codebook        makePlan (one codebook per column, one shared, or auto)
//   encoding        countValues / solve (what a row's equations are)
struct BuildSpec {
  FilterType filter;
  bool shared_filter = false;
  CodebookMode codebook = CodebookMode::Auto;
  Encoding encoding = Encoding::Columns;
  bool verbose = false;
  // Initial solution bits per equation of every cell. Lower values make more
  // cells fail and grow; tests use that to exercise growth.
  double ribbon_overhead = RIBBON_OVERHEAD;

  template <class Archive> void serialize(Archive &archive) {
    archive(filter, shared_filter, codebook, encoding, verbose,
            ribbon_overhead);
  }
};

} // namespace caramel::build
