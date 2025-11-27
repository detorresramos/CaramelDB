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

enum class BinaryFuseStatus {
  Ok = 0,
  NotFound = 1,
  NotEnoughSpace = 2,
};

inline static size_t calculateSegmentLength(size_t arity, size_t size) {
  size_t segmentLength;
  if (arity == 3) {
    segmentLength = 1L << (int)floor(log(size) / log(3.33) + 2.25);
  } else if (arity == 4) {
    segmentLength = 1L << (int)floor(log(size) / log(2.91) - 0.5);
  } else {
    segmentLength = 65536;
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
    sizeFactor = 2.0;
  }
  return sizeFactor;
}

/**
 * Binary Fuse filter with bit-packed fingerprints (1-32 bits).
 */
class BitPackedBinaryFuseFilter {
public:
  // Public members for serialization
  size_t size;
  size_t arrayLength;
  size_t segmentCount;
  size_t segmentCountLength;
  size_t segmentLength;
  size_t segmentLengthMask;
  std::unique_ptr<BitPackedArray> fingerprints;
  std::unique_ptr<BitPackedBinaryFuseHasher> hasher;
  size_t hashIndex;

  BitPackedBinaryFuseFilter(size_t filter_size, int bits_per_fingerprint)
      : size(filter_size), bitsPerFingerprint(bits_per_fingerprint), hashIndex(0) {

    if (filter_size == 0) {
      throw std::invalid_argument("BitPackedBinaryFuseFilter: size must be > 0");
    }

    segmentLength = calculateSegmentLength(arity, filter_size);
    if (segmentLength > (1 << 18)) {
      segmentLength = (1 << 18);
    }

    double size_factor = calculateSizeFactor(arity, filter_size);
    size_t capacity = filter_size * size_factor;
    size_t seg_count = (capacity + segmentLength - 1) / segmentLength - (arity - 1);

    arrayLength = (seg_count + arity - 1) * segmentLength;
    segmentLengthMask = segmentLength - 1;
    segmentCount = (arrayLength + segmentLength - 1) / segmentLength;
    segmentCount = segmentCount <= arity - 1 ? 1 : segmentCount - (arity - 1);
    arrayLength = (segmentCount + arity - 1) * segmentLength;
    segmentCountLength = segmentCount * segmentLength;

    fingerprints = std::make_unique<BitPackedArray>(arrayLength, bits_per_fingerprint);
    hasher = std::make_unique<BitPackedBinaryFuseHasher>();
  }

  BitPackedBinaryFuseFilter()
      : size(0), arrayLength(0), segmentCount(0), segmentCountLength(0),
        segmentLength(0), segmentLengthMask(0), hashIndex(0), bitsPerFingerprint(0) {}

  BinaryFuseStatus AddAll(const uint64_t* keys, size_t start, size_t end) {
    size_t n = end - start;
    if (n != size) {
      return BinaryFuseStatus::NotEnoughSpace;
    }

    std::vector<uint64_t> reverse_order(size + 1);
    std::vector<uint8_t> reverse_h(size);
    size_t reverse_order_pos;

    std::vector<uint8_t> t2count(arrayLength);
    std::vector<uint64_t> t2hash(arrayLength);
    std::vector<size_t> alone(arrayLength);

    hashIndex = 0;

    size_t h0123[7];
    size_t hi0123[7] = {0, 1, 2, 3, 0, 1, 2};

    while (true) {
      std::fill(t2count.begin(), t2count.end(), 0);
      std::fill(t2hash.begin(), t2hash.end(), 0);
      std::fill(reverse_order.begin(), reverse_order.end(), 0);
      reverse_order[size] = 1;

      int block_bits = 1;
      while ((size_t(1) << block_bits) < segmentCount) {
        block_bits++;
      }
      size_t block = size_t(1) << block_bits;
      std::vector<size_t> start_pos(block);
      for (uint32_t i = 0; i < (uint32_t(1) << block_bits); i++) {
        start_pos[i] = i * size / block;
      }

      for (size_t i = start; i < end; i++) {
        uint64_t k = keys[i];
        uint64_t hash = (*hasher)(k);
        size_t segment_index = hash >> (64 - block_bits);

        while (reverse_order[start_pos[segment_index]] != 0) {
          segment_index++;
          segment_index &= (size_t(1) << block_bits) - 1;
        }
        reverse_order[start_pos[segment_index]] = hash;
        start_pos[segment_index]++;
      }

      uint8_t count_mask = 0;
      for (size_t i = 0; i < size; i++) {
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
        return BinaryFuseStatus::NotEnoughSpace;
      }

      reverse_order_pos = 0;
      size_t alone_pos = 0;
      for (size_t i = 0; i < arrayLength; i++) {
        if ((t2count[i] >> 2) == 1) {
          alone[alone_pos++] = i;
        }
      }

      while (alone_pos > 0) {
        alone_pos--;
        size_t index = alone[alone_pos];

        if ((t2count[index] >> 2) == 1) {
          uint64_t hash = t2hash[index];
          int found = t2count[index] & 3;

          reverse_h[reverse_order_pos] = found;
          reverse_order[reverse_order_pos] = hash;

          h0123[1] = getHashFromHash(hash, 1);
          h0123[2] = getHashFromHash(hash, 2);
          h0123[3] = getHashFromHash(hash, 3);
          h0123[4] = getHashFromHash(hash, 0);
          h0123[5] = h0123[1];
          h0123[6] = h0123[2];

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

      if (reverse_order_pos == size) {
        break;
      }

      hashIndex++;
      if (hashIndex > 10) {
        return BinaryFuseStatus::NotEnoughSpace;
      }
      hasher = std::make_unique<BitPackedBinaryFuseHasher>();
    }

    for (int i = static_cast<int>(reverse_order_pos) - 1; i >= 0; i--) {
      uint64_t hash = reverse_order[i];
      int found = reverse_h[i];

      uint64_t fp = fingerprint(hash);

      h0123[0] = getHashFromHash(hash, 0);
      h0123[1] = getHashFromHash(hash, 1);
      h0123[2] = getHashFromHash(hash, 2);
      h0123[3] = getHashFromHash(hash, 3);
      h0123[4] = h0123[0];
      h0123[5] = h0123[1];
      h0123[6] = h0123[2];

      fp ^= fingerprints->get(h0123[found + 1]);
      fp ^= fingerprints->get(h0123[found + 2]);
      fp ^= fingerprints->get(h0123[found + 3]);

      fingerprints->set(h0123[found], fp);
    }

    return BinaryFuseStatus::Ok;
  }

  BinaryFuseStatus Contain(uint64_t key) const {
    uint64_t hash = (*hasher)(key);
    uint64_t fp = fingerprint(hash);

    for (int hi = 0; hi < 4; hi++) {
      size_t h = getHashFromHash(hash, hi);
      fp ^= fingerprints->get(h);
    }

    return (fp == 0) ? BinaryFuseStatus::Ok : BinaryFuseStatus::NotFound;
  }

  size_t SizeInBytes() const {
    return fingerprints->sizeInBytes();
  }

  size_t Size() const { return size; }

private:
  static constexpr size_t arity = 4;
  int bitsPerFingerprint;

  inline uint64_t fingerprint(uint64_t hash) const {
    uint64_t fp = hash ^ (hash >> 32);
    return fp & ((1ULL << bitsPerFingerprint) - 1);
  }

  inline size_t getHashFromHash(uint64_t hash, int index) const {
    __uint128_t x = (__uint128_t)hash * (__uint128_t)segmentCountLength;
    uint64_t h = (uint64_t)(x >> 64);
    h += index * segmentLength;

    uint64_t hh = hash;
    if (index > 0) {
      h ^= (hh >> ((index - 1) * 16)) & segmentLengthMask;
    }

    return h;
  }
};

} // namespace caramel
