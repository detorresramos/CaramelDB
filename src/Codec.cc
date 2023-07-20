#include "Codec.h"

namespace caramel {

/*
  A: List of symbol frequencies in non-decreasing order.
  returns: The expected lengths of codewords in huffman encoding

  Algorithm described in: http://hjemmesider.diku.dk/~jyrki/Paper/WADS95.pdf
  reference sources:
    - https://github.com/madler/brotli/blob/master/huff.c
    - https://people.eng.unimelb.edu.au/ammoffat/inplace.c
*/
void minRedundancyCodewordLengths(std::vector<uint32_t> A) {
  uint32_t n = A.size();

  uint32_t root; /* next root node to be used */
  uint32_t leaf; /* next leaf to be used */
  uint32_t next; /* next value to be assigned */
  uint32_t avbl; /* number of available nodes */
  uint32_t used; /* number of internal nodes */
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
  for (next = n - 3; next >= 0; next--)
    A[next] = A[A[next]] + 1;

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

std::tuple<CodeDict, std::vector<uint32_t>, std::vector<uint32_t>>
cannonicalHuffman(const std::vector<uint32_t> &symbols) {
  std::unordered_map<uint32_t, uint32_t> frequencies;
  for (uint32_t symbol : symbols) {
    ++frequencies[symbol];
  }

  // TODO(david) unwrap this into a two separate vectors since we end up copying
  std::vector<std::pair<uint32_t, uint32_t>> symbol_frequency_pairs(
      frequencies.begin(), frequencies.end());
  // Sort the pairs by frequency first, then by symbol.
  // This is required for the decoder to reconstruct the codes
  std::sort(symbol_frequency_pairs.begin(), symbol_frequency_pairs.end(),
            [](const auto &a, const auto &b) {
              return a.second != b.second ? a.second < b.second
                                          : a.first < b.first;
            });

  std::vector<uint32_t> codeword_lengths;
  for (auto [symbol, freq] : symbol_frequency_pairs) {
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
  CodeDict codedict;
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

  std::vector<uint32_t> ordered_symbols;
  for (auto [symbol, freq] : symbol_frequency_pairs) {
    codeword_lengths.push_back(symbol);
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
uint32_t cannonicalDecode(const BitArrayPtr &bitarray,
                          const std::vector<uint32_t> &code_length_counts,
                          const std::vector<uint32_t> &symbols) {
  uint32_t code = 0;
  uint32_t first = 0;
  uint32_t index = 0;
  for (uint32_t i = 1; i < code_length_counts.size(); i++) {

    uint32_t next_bit = (*bitarray)[i - 1];
    code = code | next_bit;
    uint32_t count = code_length_counts[i];
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