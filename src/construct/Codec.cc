#include "Codec.h"
#include "ConstructUtils.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace caramel {

bool operator>(const std::array<char, 10> &lhs,
               const std::array<char, 10> &rhs) {
  return std::lexicographical_compare(rhs.begin(), rhs.end(), lhs.begin(),
                                      lhs.end());
}

/*
  A: List of symbol frequencies in non-decreasing order.
  returns: The expected lengths of codewords in huffman encoding

  Algorithm described in: http://hjemmesider.diku.dk/~jyrki/Paper/WADS95.pdf
  reference sources:
    - https://github.com/madler/brotli/blob/master/huff.c
    - https://people.eng.unimelb.edu.au/ammoffat/inplace.c
*/
void minRedundancyCodewordLengths(std::vector<uint32_t> &A) {
  int n = A.size();
  int root;      /* next root node to be used */
  int leaf;      /* next leaf to be used */
  int next;      /* next value to be assigned */
  int avbl;      /* number of available nodes */
  int used;      /* number of internal nodes */
  uint32_t dpth; /* current depth of leaves */

  /* check for pathological cases */
  if (n == 0) {
    return;
  }
  if (n == 1) {
    A[0] = 0;
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

template <typename T>
std::tuple<CodeDict<T>, std::vector<uint32_t>, std::vector<T>>
cannonicalHuffman(const std::vector<T> &symbols) {
  std::unordered_map<T, uint32_t> frequencies;
  for (const auto &symbol : symbols) {
    ++frequencies[symbol];
  }

  // TODO(david) unwrap this into a two separate vectors since we end up copying
  std::vector<std::pair<T, uint32_t>> symbol_frequency_pairs(
      frequencies.begin(), frequencies.end());
  // Sort the pairs by frequency first, then by symbol.
  // This is required for the decoder to reconstruct the codes
  std::sort(symbol_frequency_pairs.begin(), symbol_frequency_pairs.end(),
            [](const auto &a, const auto &b) {
              return a.second != b.second ? a.second < b.second
                                          : a.first < b.first;
            });

  std::vector<uint32_t> codeword_lengths;
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
  // maps the symbol to a bitarray representing its code
  CodeDict<T> codedict;
  // Initialize code_length_counts with size = max code length + 1
  // accessing element i gives the number of codes of length i in the codedict
  std::vector<uint32_t> code_length_counts(codeword_lengths.back() + 1, 0);
  for (uint32_t i = 0; i < symbol_frequency_pairs.size(); i++) {
    auto [symbol, _] = symbol_frequency_pairs[i];
    uint32_t current_length = codeword_lengths[i];
    codedict[symbol] = BitArray::fromNumber(code, current_length);
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

  return {codedict, code_length_counts, ordered_symbols};
}

template std::tuple<CodeDict<uint32_t>, std::vector<uint32_t>,
                    std::vector<uint32_t>>
cannonicalHuffman(const std::vector<uint32_t> &);

template std::tuple<CodeDict<std::array<char, 10>>, std::vector<uint32_t>,
                    std::vector<std::array<char, 10>>>
cannonicalHuffman(const std::vector<std::array<char, 10>> &);

/*
  Find the first decodable segment in a given bitarray and return the
  associated symbol.

  Inputs:
      bitarray: A bitarray to decode
      code_length_counts: A list where the value at index i represents the
          number of symbols of code length i
      symbols: A list of symbols in canonical order

  Note: code_length_counts and symbols are returned from canonical_huffman

  Source: https://github.com/madler/zlib/blob/master/contrib/puff/puff.c#L235
*/
template <typename T>
T cannonicalDecode(const BitArrayPtr &bitarray,
                   const std::vector<uint32_t> &code_length_counts,
                   const std::vector<T> &symbols) {
  // instead of storing the symbols, if we have variable length stuff, store a
  // vector of pointers
  int code = 0;
  int first = 0;
  int index = 0;
  for (uint32_t i = 1; i < code_length_counts.size(); i++) {
    uint32_t next_bit = (*bitarray)[i - 1];
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

template uint32_t cannonicalDecode<uint32_t>(const BitArrayPtr &,
                                             const std::vector<uint32_t> &,
                                             const std::vector<uint32_t> &);

template std::array<char, 10> cannonicalDecode<std::array<char, 10>>(
    const BitArrayPtr &, const std::vector<uint32_t> &,
    const std::vector<std::array<char, 10>> &);

} // namespace caramel