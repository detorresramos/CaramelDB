#pragma once

#include <algorithm>
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>
#include <src/BitArray.h>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace caramel {

// A canonical Huffman codeword: `length` bits, most significant bit first.
struct Code {
  uint64_t bits = 0;
  uint32_t length = 0;

  uint32_t numBits() const { return length; }
  bool operator[](uint32_t i) const { return (bits >> (length - 1 - i)) & 1; }
};

template <typename T> using CodeDict = std::unordered_map<T, Code>;

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
template <typename T, typename Bits>
T canonicalDecode(const Bits &bitarray,
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

template <typename T> struct CsfCodebook {
  std::vector<uint32_t> code_length_counts;
  std::vector<T> ordered_symbols;
  uint32_t max_codelength = 0;
  // Construction-only value -> code map. Not serialized: it is a pure function
  // of (code_length_counts, ordered_symbols), rebuilt by buildCodedict().
  CodeDict<T> codedict;

  CsfCodebook() = default;

  void buildCodedict() {
    codedict.clear();
    codedict.reserve(ordered_symbols.size());
    uint64_t code = 0;
    size_t i = 0;
    for (uint32_t len = 1; len < code_length_counts.size(); len++) {
      for (uint32_t k = 0; k < code_length_counts[len]; k++, i++) {
        codedict.emplace(ordered_symbols[i], Code{code, len});
        code++;
      }
      code <<= 1;
    }
  }

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

  std::vector<uint32_t> code_length_counts(codeword_lengths.back() + 1, 0);
  for (uint64_t len : codeword_lengths) {
    code_length_counts[len]++;
  }

  std::vector<T> ordered_symbols;
  for (auto [symbol, freq] : symbol_frequency_pairs) {
    ordered_symbols.push_back(symbol);
  }

  CsfCodebook<T> cb;
  cb.code_length_counts = std::move(code_length_counts);
  cb.ordered_symbols = std::move(ordered_symbols);
  cb.max_codelength = cb.code_length_counts.size() - 1;
  cb.buildCodedict();
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
