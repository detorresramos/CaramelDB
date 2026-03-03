#pragma once

#include "Codec.h"
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>
#include <vector>

namespace caramel {

template <typename T> struct CsfCodebook {
  std::vector<uint32_t> code_length_counts;
  std::vector<T> ordered_symbols;
  uint32_t max_codelength = 0;
  HuffmanLookupTable<T> lookup_table;

  CsfCodebook() = default;

  static CsfCodebook fromHuffman(const HuffmanOutput<T> &huffman) {
    CsfCodebook cb;
    cb.code_length_counts = huffman.code_length_counts;
    cb.ordered_symbols = huffman.ordered_symbols;
    cb.max_codelength = huffman.max_codelength;
    cb.buildLookupTable();
    return cb;
  }

  void buildLookupTable() {
    if (!code_length_counts.empty() && max_codelength > 0) {
      lookup_table = HuffmanLookupTable<T>(code_length_counts, ordered_symbols,
                                           max_codelength);
    }
  }

private:
  friend class cereal::access;

  template <class Archive> void save(Archive &archive) const {
    archive(code_length_counts, ordered_symbols, max_codelength);
  }

  template <class Archive> void load(Archive &archive) {
    archive(code_length_counts, ordered_symbols, max_codelength);
    buildLookupTable();
  }
};

} // namespace caramel
