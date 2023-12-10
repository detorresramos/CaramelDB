#pragma once

#include <src/BitArray.h>
#include <unordered_map>
#include <vector>

namespace caramel {

// bool operator>(const std::array<char, 10> &lhs,
//                const std::array<char, 10> &rhs) {
//   return std::lexicographical_compare(rhs.begin(), rhs.end(), lhs.begin(),
//                                       lhs.end());
// }

template <typename T>
double
minRedundancyCodewordLengths(std::vector<uint32_t> &A,
                             const std::unordered_map<T, uint32_t> &frequencies,
                             const std::vector<T> &symbols) {
  int n = A.size();
  int root;      /* next root node to be used */
  int leaf;      /* next leaf to be used */
  int next;      /* next value to be assigned */
  int avbl;      /* number of available nodes */
  int used;      /* number of internal nodes */
  uint32_t dpth; /* current depth of leaves */

  if (n <= 1) {
    A[0] = 1;
    return 1;
  }

  double overall_length = 0;

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
      overall_length += dpth * frequencies.at(symbols[n - next - 1]);
      A[next--] = dpth;
      avbl--;
    }
    avbl = 2 * used;
    dpth++;
    used = 0;
  }

  return overall_length;
}

template <typename T> using CodeDict = std::unordered_map<T, BitArrayPtr>;

template <typename T>
std::tuple<CodeDict<T>, std::vector<uint32_t>, std::vector<T>>
cannonicalHuffman(const std::vector<T> &symbols,
                  uint32_t length_limit = std::numeric_limits<uint32_t>::max(),
                  double entropy_threshold = 1) {
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

  double overall_length =
      minRedundancyCodewordLengths(codeword_lengths, frequencies, symbols);

  // We reverse because code assignment is done in non-decreasing order of bit
  // length instead of frequency
  std::reverse(symbol_frequency_pairs.begin(), symbol_frequency_pairs.end());
  std::reverse(codeword_lengths.begin(), codeword_lengths.end());

  /* We now progress through the symbols, from more frequent
   * to less frequent, computing for each prefix of symbols
   * the length of the decoding table and the cumulative
   * length of such symbols. If we pass the table length limit
   * or the empirical entropy threshold be break the loop. */
  long accumulatedOverallLength = 0;
  uint32_t currentLength = codeword_lengths[0];
  uint32_t d = 1;
  for (uint32_t cutpoint = 0; cutpoint < frequencies.size(); cutpoint++) {
    if (currentLength != codeword_lengths[cutpoint]) {
      if (++d >= length_limit)
        break;
      if (accumulatedOverallLength / overall_length > entropy_threshold)
        break;
      currentLength = codeword_lengths[cutpoint];
    }
    accumulatedOverallLength +=
        codeword_lengths[cutpoint] * frequencies.at(symbols[cutpoint]);
  }

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

} // namespace caramel