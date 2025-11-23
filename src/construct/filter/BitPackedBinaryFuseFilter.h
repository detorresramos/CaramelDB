#pragma once

#include "BitPackedArray.h"
#include <src/construct/SpookyHash.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace caramel {

// Hasher for Binary Fuse filter with random seed generation and Murmur64 mixing
// Matches the SimpleMixSplit behavior from the library
class BitPackedBinaryFuseHasher {
public:
  uint64_t seed;

  BitPackedBinaryFuseHasher() {
    std::random_device random;
    seed = random();
    seed <<= 32;
    seed |= random();
  }

  inline uint64_t operator()(uint64_t key) const {
    return murmur64(key + seed);
  }

private:
  static inline uint64_t murmur64(uint64_t h) {
    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    h *= UINT64_C(0xc4ceb9fe1a85ec53);
    h ^= h >> 33;
    return h;
  }
};

// Status codes for Binary Fuse filter operations
enum class BinaryFuseStatus {
  Ok = 0,
  NotFound = 1,
  NotEnoughSpace = 2,
};

// Helper functions from the library
inline static size_t calculateSegmentLength(size_t arity, size_t size) {
  size_t segmentLength;
  if (arity == 3) {
    segmentLength = 1L << (int)floor(log(size) / log(3.33) + 2.25);
  } else if (arity == 4) {
    segmentLength = 1L << (int)floor(log(size) / log(2.91) - 0.5);
  } else {
    segmentLength = 65536;  // not supported
  }
  return segmentLength;
}

inline static double calculateSizeFactor(size_t arity, size_t size) {
  if (size <= 2) { size = 2; }
  double sizeFactor;
  if (arity == 3) {
    sizeFactor = fmax(1.125, 0.875 + 0.25 * log(1000000) / log(size));
  } else if (arity == 4) {
    sizeFactor = fmax(1.075, 0.77 + 0.305 * log(600000) / log(size));
  } else {
    sizeFactor = 2.0;  // not supported
  }
  return sizeFactor;
}

/**
 * Binary Fuse filter with bit-packed fingerprints (1-32 bits).
 * Based on "Binary Fuse Filters: Fast and Smaller Than Xor Filters"
 * by Graf & Lemire (2021).
 *
 * This implementation uses 4-wise hashing and bit-packing to support
 * arbitrary fingerprint widths, enabling fine-grained control over
 * the space/FPR trade-off.
 */
class BitPackedBinaryFuseFilter {
public:
  /**
   * Create a Binary Fuse filter.
   * @param size Number of keys to store
   * @param bits_per_fingerprint Fingerprint width in bits (1-32)
   */
  BitPackedBinaryFuseFilter(size_t size, int bits_per_fingerprint)
      : _size(size), _bits_per_fingerprint(bits_per_fingerprint) {

    if (size == 0) {
      throw std::invalid_argument("BitPackedBinaryFuseFilter: size must be > 0");
    }

    // Binary Fuse filter uses 1.075x space overhead for 4-wise
    _segment_length = calculateSegmentLength(_arity, size);

    // Hardcoded 18-bit limit to segment length (from library)
    if (_segment_length > (1 << 18)) {
      _segment_length = (1 << 18);
    }

    double size_factor = calculateSizeFactor(_arity, size);
    size_t capacity = size * size_factor;
    size_t segment_count = (capacity + _segment_length - 1) / _segment_length - (_arity - 1);

    _array_length = (segment_count + _arity - 1) * _segment_length;
    _segment_length_mask = _segment_length - 1;
    _segment_count = (_array_length + _segment_length - 1) / _segment_length;
    _segment_count = _segment_count <= _arity - 1 ? 1 : _segment_count - (_arity - 1);
    _array_length = (_segment_count + _arity - 1) * _segment_length;
    _segment_count_length = _segment_count * _segment_length;

    // Create bit-packed storage
    _fingerprints = std::make_unique<BitPackedArray>(_array_length, bits_per_fingerprint);

    _hasher = std::make_unique<BitPackedBinaryFuseHasher>();
    _hash_index = 0;
  }

  /**
   * Build the filter from a set of keys.
   * @param keys Array of 64-bit hashed keys
   * @param start Start index in keys array
   * @param end End index in keys array
   * @return Ok on success, NotEnoughSpace if construction fails
   */
  BinaryFuseStatus AddAll(const uint64_t* keys, size_t start, size_t end) {
    size_t n = end - start;
    if (n != _size) {
      return BinaryFuseStatus::NotEnoughSpace;
    }

    // Allocate temporary arrays for peeling algorithm
    std::vector<uint64_t> reverse_order(_size + 1);
    std::vector<uint8_t> reverse_h(_size);
    size_t reverse_order_pos;

    std::vector<uint8_t> t2count(_array_length);
    std::vector<uint64_t> t2hash(_array_length);
    std::vector<size_t> alone(_array_length);

    _hash_index = 0;

    // Helper arrays for 4-wise hashing
    size_t h0123[7];
    size_t hi0123[7] = {0, 1, 2, 3, 0, 1, 2};

    // Try construction with different hash seeds (retry on failure)
    while (true) {
      std::fill(t2count.begin(), t2count.end(), 0);
      std::fill(t2hash.begin(), t2hash.end(), 0);
      std::fill(reverse_order.begin(), reverse_order.end(), 0);
      reverse_order[_size] = 1;

      // Counting sort (block-based distribution)
      int block_bits = 1;
      while ((size_t(1) << block_bits) < _segment_count) {
        block_bits++;
      }
      size_t block = size_t(1) << block_bits;
      std::vector<size_t> start_pos(block);
      for (uint32_t i = 0; i < (uint32_t(1) << block_bits); i++) {
        start_pos[i] = i * _size / block;
      }

      for (size_t i = start; i < end; i++) {
        uint64_t k = keys[i];
        uint64_t hash = (*_hasher)(k);
        size_t segment_index = hash >> (64 - block_bits);

        // Linear probing to find empty slot
        while (reverse_order[start_pos[segment_index]] != 0) {
          segment_index++;
          segment_index &= (size_t(1) << block_bits) - 1;
        }
        reverse_order[start_pos[segment_index]] = hash;
        start_pos[segment_index]++;
      }

      // Phase 1: Build count table
      uint8_t count_mask = 0;
      for (size_t i = 0; i < _size; i++) {
        uint64_t hash = reverse_order[i];
        for (int hi = 0; hi < 4; hi++) {
          int index = getHashFromHash(hash, hi);
          t2count[index] += 4;
          t2count[index] ^= hi;
          t2hash[index] ^= hash;
          count_mask |= t2count[index];
        }
      }

      if (count_mask >= 0x80) {
        // Counter overflow - this shouldn't happen with good hashing
        return BinaryFuseStatus::NotEnoughSpace;
      }

      // Phase 2: Find cells with exactly one key
      reverse_order_pos = 0;
      size_t alone_pos = 0;
      for (size_t i = 0; i < _array_length; i++) {
        if ((t2count[i] >> 2) == 1) {
          alone[alone_pos++] = i;
        }
      }

      // Phase 3: Peel (process keys in reverse order)
      while (alone_pos > 0) {
        alone_pos--;
        size_t index = alone[alone_pos];

        if ((t2count[index] >> 2) == 1) {
          uint64_t hash = t2hash[index];
          int found = t2count[index] & 3;

          reverse_h[reverse_order_pos] = found;
          reverse_order[reverse_order_pos] = hash;

          // Calculate all 4 hash locations
          h0123[1] = getHashFromHash(hash, 1);
          h0123[2] = getHashFromHash(hash, 2);
          h0123[3] = getHashFromHash(hash, 3);
          h0123[4] = getHashFromHash(hash, 0);
          h0123[5] = h0123[1];
          h0123[6] = h0123[2];

          // Update counts for the other 3 locations
          // IMPORTANT: Always assign, conditionally increment (matches library behavior)
          size_t index3 = h0123[found + 1];
          alone[alone_pos] = index3;
          alone_pos += ((t2count[index3] >> 2) == 2 ? 1 : 0);
          t2count[index3] -= 4;
          t2count[index3] ^= hi0123[found + 1];
          t2hash[index3] ^= hash;

          index3 = h0123[found + 2];
          alone[alone_pos] = index3;
          alone_pos += ((t2count[index3] >> 2) == 2 ? 1 : 0);
          t2count[index3] -= 4;
          t2count[index3] ^= hi0123[found + 2];
          t2hash[index3] ^= hash;

          index3 = h0123[found + 3];
          alone[alone_pos] = index3;
          alone_pos += ((t2count[index3] >> 2) == 2 ? 1 : 0);
          t2count[index3] -= 4;
          t2count[index3] ^= hi0123[found + 3];
          t2hash[index3] ^= hash;

          reverse_order_pos++;
        }
      }

      // Check if all keys were processed
      if (reverse_order_pos == _size) {
        break;
      }

      // Construction failed, retry with different hash seed
      _hash_index++;
      if (_hash_index > 10) {
        return BinaryFuseStatus::NotEnoughSpace;
      }
      delete _hasher.release();
      _hasher = std::make_unique<BitPackedBinaryFuseHasher>();
    }

    // Phase 4: Assign fingerprints in reverse order
    for (int i = static_cast<int>(reverse_order_pos) - 1; i >= 0; i--) {
      uint64_t hash = reverse_order[i];
      int found = reverse_h[i];

      uint64_t fp = fingerprint(hash);

      // Calculate all 4 hash locations
      h0123[0] = getHashFromHash(hash, 0);
      h0123[1] = getHashFromHash(hash, 1);
      h0123[2] = getHashFromHash(hash, 2);
      h0123[3] = getHashFromHash(hash, 3);
      h0123[4] = h0123[0];
      h0123[5] = h0123[1];
      h0123[6] = h0123[2];

      // XOR with the other three locations
      fp ^= _fingerprints->get(h0123[found + 1]);
      fp ^= _fingerprints->get(h0123[found + 2]);
      fp ^= _fingerprints->get(h0123[found + 3]);

      // Store at the "alone" location
      _fingerprints->set(h0123[found], fp);
    }

    return BinaryFuseStatus::Ok;
  }

  /**
   * Check if a key is in the filter.
   * @param key 64-bit hashed key
   * @return Ok if likely in set, NotFound if definitely not in set
   */
  BinaryFuseStatus Contain(uint64_t key) const {
    uint64_t hash = (*_hasher)(key);
    uint64_t fp = fingerprint(hash);

    // Get fingerprints from all 4 hash locations and XOR them
    for (int hi = 0; hi < 4; hi++) {
      size_t h = getHashFromHash(hash, hi);
      fp ^= _fingerprints->get(h);
    }

    return (fp == 0) ? BinaryFuseStatus::Ok : BinaryFuseStatus::NotFound;
  }

  /**
   * Get size in bytes.
   */
  size_t SizeInBytes() const {
    return _fingerprints->sizeInBytes();
  }

  /**
   * Get number of elements.
   */
  size_t Size() const { return _size; }

  // Public members for serialization (match library interface)
  size_t size;
  size_t arrayLength;
  size_t segmentCount;
  size_t segmentCountLength;
  size_t segmentLength;
  size_t segmentLengthMask;
  std::unique_ptr<BitPackedArray> fingerprints;
  std::unique_ptr<BitPackedBinaryFuseHasher> hasher;
  size_t hashIndex;

  // Constructor for deserialization
  BitPackedBinaryFuseFilter()
      : _size(0), _bits_per_fingerprint(0), _array_length(0),
        _segment_length(0), _segment_count(0), _segment_count_length(0),
        _segment_length_mask(0), _hash_index(0) {}

private:
  /**
   * Compute fingerprint from hash.
   * Reduces 64-bit hash to n-bit fingerprint.
   */
  inline uint64_t fingerprint(uint64_t hash) const {
    uint64_t fp = hash ^ (hash >> 32);
    return fp & ((1ULL << _bits_per_fingerprint) - 1);
  }

  /**
   * Compute hash location from hash and index.
   * Uses 128-bit multiplication for segment selection (4-wise Binary Fuse).
   */
  inline size_t getHashFromHash(uint64_t hash, int index) const {
    // Use 128-bit multiplication to select segment
    __uint128_t x = (__uint128_t)hash * (__uint128_t)_segment_count_length;
    uint64_t h = (uint64_t)(x >> 64);
    h += index * _segment_length;

    // Mix with rotated hash bits for within-segment position
    uint64_t hh = hash;
    if (index > 0) {
      h ^= (hh >> ((index - 1) * 16)) & _segment_length_mask;
    }

    return h;
  }

  static constexpr size_t _arity = 4;  // Binary Fuse uses 4-wise hashing

  size_t _size;
  int _bits_per_fingerprint;
  size_t _array_length;
  size_t _segment_length;
  size_t _segment_count;
  size_t _segment_count_length;
  size_t _segment_length_mask;
  size_t _hash_index;

  std::unique_ptr<BitPackedArray> _fingerprints;
  std::unique_ptr<BitPackedBinaryFuseHasher> _hasher;
};

} // namespace caramel
