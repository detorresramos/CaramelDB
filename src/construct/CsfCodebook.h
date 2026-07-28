#pragma once

#include <algorithm>
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>
#include <src/BitArray.h>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T> using CodeDict = std::unordered_map<T, BitArray>;

/*
  A: List of symbol frequencies in non-decreasing order.
  returns: The expected lengths of codewords in huffman encoding

  Algorithm described in: http://hjemmesider.diku.dk/~jyrki/Paper/WADS95.pdf
  reference sources:
    - https://github.com/madler/brotli/blob/master/huff.c
    - https://people.eng.unimelb.edu.au/ammoffat/inplace.c
*/
inline void minRedundancyCodewordLengths(std::vector<uint64_t> &A) {
  int n = A.size();
  int root;      /* next root node to be used */
  int leaf;      /* next leaf to be used */
  int next;      /* next value to be assigned */
  int avbl;      /* number of available nodes */
  int used;      /* number of internal nodes */
  uint64_t dpth; /* current depth of leaves */

  /* check for pathological cases */
  if (n == 0) {
    return;
  }
  if (n == 1) {
    A[0] = 1;
    return;
  }

  /* first pass, left to right, setting parent pointers */
  A[0] += A[1];
  root = 0;
  leaf = 2;
  for (next = 1; next < n - 1; next++) {
    /* select first item for a pairing */
    if (leaf >= n || A[root] < A[leaf]) {
      A[next] = A[root];
      A[root++] = next;
    } else
      A[next] = A[leaf++];

    /* add on the second item */
    if (leaf >= n || (root < next && A[root] < A[leaf])) {
      A[next] += A[root];
      A[root++] = next;
    } else
      A[next] += A[leaf++];
  }

  /* second pass, right to left, setting internal depths */
  A[n - 2] = 0;
  for (next = n - 3; next >= 0; next--) {
    A[next] = A[A[next]] + 1;
  }
  /* third pass, right to left, setting leaf depths */
  avbl = 1;
  used = 0;
  dpth = 0;
  root = n - 2;
  next = n - 1;
  while (avbl > 0) {
    while (root >= 0 && A[root] == dpth) {
      used++;
      root--;
    }
    while (avbl > used) {
      A[next--] = dpth;
      avbl--;
    }
    avbl = 2 * used;
    dpth++;
    used = 0;
  }
}

/*
  Find the first decodable segment in a given bitarray and return the
  associated symbol.

  Source: https://github.com/madler/zlib/blob/master/contrib/puff/puff.c#L235
*/
template <typename T>
T canonicalDecode(const BitArray &bitarray,
                   const std::vector<uint32_t> &code_length_counts,
                   const std::vector<T> &symbols) {
  int code = 0;
  int first = 0;
  int index = 0;
  for (uint32_t i = 1; i < code_length_counts.size(); i++) {
    uint32_t next_bit = bitarray[i - 1];
    code = code | next_bit;
    int count = code_length_counts[i];
    if (code - count < first) {
      return symbols[index + (code - first)];
    }
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  throw std::invalid_argument("Invalid Code Passed");
}

template <typename T>
inline T canonicalDecodeFromNumber(
    uint64_t encoded_value, const std::vector<uint32_t> &code_length_counts,
    const std::vector<T> &symbols, uint32_t max_codelength) {
  int code = 0;
  int first = 0;
  int index = 0;
  for (uint32_t i = 1; i < code_length_counts.size(); i++) {
    uint32_t next_bit = (encoded_value >> (max_codelength - i)) & 1ULL;
    code = code | next_bit;
    int count = code_length_counts[i];
    if (code - count < first) {
      return symbols[index + (code - first)];
    }
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  throw std::invalid_argument("Invalid Code Passed");
}

// Branchless replacement for canonicalDecodeFromNumber's bit-at-a-time loop.
//
// For a canonical code, a value's codeword length is the smallest l with
// left_aligned_limit[l] > value, and left_aligned_limit is non-decreasing, so
// the length is just a count of the limits the value clears. Two small tables
// (max_codelength+1 entries each, so they stay resident) replace ~max_codelength
// iterations of data-dependent branching.
struct CanonicalDecodeTables {
  // limit[l] = (first_code[l] + count[l]) << (max_codelength - l)
  std::vector<uint64_t> limit;
  // symbol_index = index_bias[l] + (value >> (max_codelength - l))
  std::vector<int64_t> index_bias;
  uint32_t max_codelength = 0;
  uint32_t last_symbol = 0;

  void build(const std::vector<uint32_t> &code_length_counts,
             size_t num_symbols, uint32_t max_cl) {
    max_codelength = max_cl;
    last_symbol = num_symbols ? static_cast<uint32_t>(num_symbols - 1) : 0;
    limit.assign(max_cl + 1, 0);
    index_bias.assign(max_cl + 1, 0);
    uint64_t first = 0;
    int64_t index = 0;
    for (uint32_t l = 1; l <= max_cl && l < code_length_counts.size(); l++) {
      uint64_t count = code_length_counts[l];
      limit[l] = (first + count) << (max_cl - l);
      index_bias[l] = index - static_cast<int64_t>(first);
      index += static_cast<int64_t>(count);
      first = (first + count) << 1;
    }
  }
};

template <typename T>
inline T __attribute__((always_inline))
canonicalDecodeBranchless(uint64_t encoded_value,
                          const CanonicalDecodeTables &tables,
                          const std::vector<T> &symbols) {
  uint32_t length = 1;
  for (uint32_t l = 1; l < tables.max_codelength; l++) {
    length += (encoded_value >= tables.limit[l]);
  }
  int64_t idx = tables.index_bias[length] +
                static_cast<int64_t>(encoded_value >>
                                     (tables.max_codelength - length));
  // Keys not in the CSF decode to an arbitrary codeword, which can land past
  // the symbol table; clamp rather than read out of bounds.
  if (idx < 0 || idx > tables.last_symbol) {
    idx = tables.last_symbol;
  }
  return symbols[idx];
}

template <typename T> struct CsfCodebook {
  std::vector<uint32_t> code_length_counts;
  std::vector<T> ordered_symbols;
  uint32_t max_codelength = 0;
  CodeDict<T> codedict;
  // Query-time only, rebuilt on load rather than serialized.
  CanonicalDecodeTables decode_tables;

  void buildDecodeTables() {
    decode_tables.build(code_length_counts, ordered_symbols.size(),
                        max_codelength);
  }

  CsfCodebook() = default;

private:
  friend class cereal::access;

  template <class Archive> void serialize(Archive &archive) {
    archive(code_length_counts, ordered_symbols, max_codelength);
  }
};

template <typename T>
CsfCodebook<T>
canonicalHuffmanFromFrequencies(const std::unordered_map<T, uint64_t> &frequencies) {
  // TODO(david) unwrap this into a two separate vectors since we end up copying
  std::vector<std::pair<T, uint64_t>> symbol_frequency_pairs(
      frequencies.begin(), frequencies.end());
  // Sort the pairs by frequency first, then by symbol.
  // This is required for the decoder to reconstruct the codes
  std::sort(symbol_frequency_pairs.begin(), symbol_frequency_pairs.end(),
            [](const auto &a, const auto &b) {
              return a.second != b.second ? a.second < b.second
                                          : a.first < b.first;
            });

  std::vector<uint64_t> codeword_lengths;
  for (auto [_, freq] : symbol_frequency_pairs) {
    codeword_lengths.push_back(freq);
  }
  minRedundancyCodewordLengths(codeword_lengths);

  // We reverse because code assignment is done in non-decreasing order of bit
  // length instead of frequency
  std::reverse(symbol_frequency_pairs.begin(), symbol_frequency_pairs.end());
  std::reverse(codeword_lengths.begin(), codeword_lengths.end());

  // TODO(any) add length limiting

  uint32_t code = 0;
  CodeDict<T> codedict;
  std::vector<uint32_t> code_length_counts(codeword_lengths.back() + 1, 0);
  for (uint32_t i = 0; i < symbol_frequency_pairs.size(); i++) {
    auto &[symbol, _] = symbol_frequency_pairs[i];
    uint32_t current_length = static_cast<uint32_t>(codeword_lengths[i]);
    codedict.emplace(symbol, BitArray::fromNumber(code, current_length));
    code_length_counts[current_length]++;
    if (i + 1 < codeword_lengths.size()) {
      code++;
      code <<= codeword_lengths[i + 1] - current_length;
    }
  }

  std::vector<T> ordered_symbols;
  for (auto [symbol, freq] : symbol_frequency_pairs) {
    ordered_symbols.push_back(symbol);
  }

  CsfCodebook<T> cb;
  cb.code_length_counts = std::move(code_length_counts);
  cb.ordered_symbols = std::move(ordered_symbols);
  cb.max_codelength = cb.code_length_counts.size() - 1;
  cb.codedict = std::move(codedict);
  return cb;
}

template <typename T>
CsfCodebook<T> canonicalHuffman(const std::vector<T> &symbols) {
  std::unordered_map<T, uint64_t> frequencies;
  for (const auto &symbol : symbols) {
    ++frequencies[symbol];
  }
  return canonicalHuffmanFromFrequencies<T>(frequencies);
}

} // namespace caramel
