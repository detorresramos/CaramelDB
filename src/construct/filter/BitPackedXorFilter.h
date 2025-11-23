#pragma once

#include "BitPackedArray.h"
#include <src/construct/SpookyHash.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace caramel {

// Custom hasher that just returns the key (we pre-hash strings)
class BitPackedXorHasher {
public:
  uint64_t seed;
  BitPackedXorHasher() : seed(0) {}
  inline uint64_t operator()(uint64_t key) const { return key; }
};

// Status codes for XOR filter operations
enum class XorStatus {
  Ok = 0,
  NotFound = 1,
  NotEnoughSpace = 2,
};

/**
 * XOR filter with bit-packed fingerprints (1-32 bits).
 * Based on the algorithm from "Xor Filters: Faster and Smaller Than Bloom and Cuckoo Filters"
 * by Graf & Lemire (2020).
 *
 * This implementation uses bit-packing to support arbitrary fingerprint widths,
 * enabling fine-grained control over the space/FPR trade-off.
 */
class BitPackedXorFilter {
public:
  // Public members (match fastfilter_cpp interface for serialization)
  size_t size;
  size_t arrayLength;
  size_t blockLength;
  std::unique_ptr<BitPackedArray> fingerprints;
  std::unique_ptr<BitPackedXorHasher> hasher;
  size_t hashIndex;

  /**
   * Create an XOR filter.
   * @param size Number of keys to store
   * @param bits_per_fingerprint Fingerprint width in bits (1-32)
   */
  BitPackedXorFilter(size_t filter_size, int bits_per_fingerprint)
      : size(filter_size), arrayLength(0), blockLength(0), hashIndex(0),
        bitsPerFingerprint(bits_per_fingerprint) {

    if (filter_size == 0) {
      throw std::invalid_argument("BitPackedXorFilter: size must be > 0");
    }

    // XOR filter uses 1.23x space overhead
    arrayLength = 32 + static_cast<size_t>(1.23 * filter_size);
    blockLength = arrayLength / 3;

    // Create bit-packed storage
    fingerprints = std::make_unique<BitPackedArray>(arrayLength, bits_per_fingerprint);

    hasher = std::make_unique<BitPackedXorHasher>();
    hashIndex = 0;
  }

  // Constructor for deserialization
  BitPackedXorFilter() : size(0), arrayLength(0), blockLength(0), hashIndex(0),
                         bitsPerFingerprint(0) {}

  /**
   * Build the filter from a set of keys.
   * @param keys Array of 64-bit hashed keys
   * @param start Start index in keys array
   * @param end End index in keys array
   * @return Ok on success, NotEnoughSpace if construction fails
   */
  XorStatus AddAll(const uint64_t* keys, size_t start, size_t end) {
    size_t n = end - start;
    if (n != size) {
      return XorStatus::NotEnoughSpace;
    }

    std::vector<uint64_t> reverse_order;
    reverse_order.reserve(n);
    std::vector<int> reverse_h;
    reverse_h.reserve(n);

    // Try construction with different hash seeds (retry on failure)
    for (int attempt = 0; attempt < 10; attempt++) {
      hashIndex = attempt;
      reverse_order.clear();
      reverse_h.clear();

      // Count how many keys map to each cell
      std::vector<uint64_t> t2count(arrayLength, 0);
      std::vector<uint64_t> t2hash(arrayLength, 0);

      // Phase 1: Build count table
      for (size_t i = start; i < end; i++) {
        uint64_t hash = keys[i];
        for (int hi = 0; hi < 3; hi++) {
          size_t h = getHashFromHash(hash, hi);
          t2count[h]++;
          t2hash[h] ^= hash;
        }
      }

      // Phase 2: Peel (find keys that can be uniquely placed)
      std::vector<size_t> alone;
      alone.reserve(n);

      // Find cells with exactly one key
      for (size_t i = 0; i < arrayLength; i++) {
        if (t2count[i] == 1) {
          alone.push_back(i);
        }
      }

      // Process keys in reverse order
      int found = -1;
      while (!alone.empty()) {
        size_t i = alone.back();
        alone.pop_back();

        if (t2count[i] == 0) {
          continue;
        }

        uint64_t hash = t2hash[i];

        // Find which of the 3 hash locations is "alone"
        for (int hi = 0; hi < 3; hi++) {
          size_t h = getHashFromHash(hash, hi);
          t2count[h]--;

          if (h == i) {
            found = hi;
          } else {
            t2hash[h] ^= hash;
            if (t2count[h] == 1) {
              alone.push_back(h);
            }
          }
        }

        reverse_order.push_back(hash);
        reverse_h.push_back(found);
      }

      // Check if all keys were processed
      if (reverse_order.size() != n) {
        // Construction failed, retry with different hash seed
        continue;
      }

      // Phase 3: Assign fingerprints in reverse order
      for (int i = static_cast<int>(reverse_order.size()) - 1; i >= 0; i--) {
        uint64_t hash = reverse_order[i];
        int found_hi = reverse_h[i];

        uint64_t fp = fingerprint(hash);

        // XOR with the other two locations
        for (int hi = 0; hi < 3; hi++) {
          size_t h = getHashFromHash(hash, hi);
          if (found_hi != hi) {
            fp ^= fingerprints->get(h);
          }
        }

        // Store at the "alone" location
        size_t h = getHashFromHash(hash, found_hi);
        fingerprints->set(h, fp);
      }

      return XorStatus::Ok;  // Success!
    }

    return XorStatus::NotEnoughSpace;  // Failed after all attempts
  }

  /**
   * Check if a key is in the filter.
   * @param key 64-bit hashed key
   * @return Ok if likely in set, NotFound if definitely not in set
   */
  XorStatus Contain(uint64_t key) const {
    uint64_t hash = (*hasher)(key);
    uint64_t fp = fingerprint(hash);

    // Get fingerprints from all 3 hash locations
    uint64_t h0 = getHashFromHash(hash, 0);
    uint64_t h1 = getHashFromHash(hash, 1);
    uint64_t h2 = getHashFromHash(hash, 2);

    // XOR all three fingerprints
    fp ^= fingerprints->get(h0);
    fp ^= fingerprints->get(h1);
    fp ^= fingerprints->get(h2);

    return (fp == 0) ? XorStatus::Ok : XorStatus::NotFound;
  }

  /**
   * Get size in bytes.
   */
  size_t SizeInBytes() const {
    return fingerprints->sizeInBytes();
  }

  /**
   * Get number of elements.
   */
  size_t Size() const { return size; }

private:
  int bitsPerFingerprint;  // Fingerprint width in bits

  /**
   * Compute fingerprint from hash.
   * Reduces 64-bit hash to n-bit fingerprint.
   */
  inline uint64_t fingerprint(uint64_t hash) const {
    uint64_t fp = hash ^ (hash >> 32);
    return fp & ((1ULL << bitsPerFingerprint) - 1);
  }

  /**
   * Compute hash location from hash and index.
   * Uses different mixing for each of the 3 hash functions.
   */
  inline size_t getHashFromHash(uint64_t hash, int index) const {
    // Rotate hash by different amounts for each hash function
    uint64_t r = rotl64(hash, index * 21 + static_cast<int>(hashIndex) * 7);
    // Reduce to block range
    uint32_t reduced = reduce(static_cast<uint32_t>(r), static_cast<uint32_t>(blockLength));
    return static_cast<size_t>(reduced) + index * blockLength;
  }

  /**
   * Rotate 64-bit value left.
   */
  inline static uint64_t rotl64(uint64_t n, unsigned int c) {
    const unsigned int mask = 63;
    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
  }

  /**
   * Fast modulo reduction using multiplication.
   * Computes (hash * n) / 2^32, which approximates hash % n.
   */
  inline static uint32_t reduce(uint32_t hash, uint32_t n) {
    return static_cast<uint32_t>((static_cast<uint64_t>(hash) * n) >> 32);
  }
};

} // namespace caramel
