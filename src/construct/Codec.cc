#include "Codec.h"
#include "ConstructUtils.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace caramel {

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

} // namespace caramel