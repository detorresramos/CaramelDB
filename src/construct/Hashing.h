#pragma once

#include "SpookyHash.h"
#include <cstdint>
#include <cstring>
#include <string>

namespace caramel {

namespace detail {

inline uint64_t mum(uint64_t a, uint64_t b) {
  const __uint128_t r = static_cast<__uint128_t>(a) * b;
  return static_cast<uint64_t>(r) ^ static_cast<uint64_t>(r >> 64);
}

inline uint64_t read8(const char *p) {
  uint64_t v;
  std::memcpy(&v, p, 8);
  return v;
}

inline uint64_t read4(const char *p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}

// Keys of at most 15 bytes. The bytes are loaded into (lo, hi) one-to-one for
// a given length, and a Feistel network is a permutation, so two distinct keys
// of the same length never share a signature.
inline __uint128_t hashShort(const char *p, size_t n) {
  uint64_t lo = 0;
  uint64_t hi = 0;
  if (n >= 8) {
    lo = read8(p);
    hi = read8(p + n - 8);
  } else if (n >= 4) {
    lo = read4(p) << 32 | read4(p + n - 4);
  } else if (n > 0) {
    lo = uint64_t(uint8_t(p[0])) << 16 | uint64_t(uint8_t(p[n >> 1])) << 8 |
         uint8_t(p[n - 1]);
  }
  hi ^= mum(lo ^ 0xa0761d6478bd642fULL, 0xe7037ed1a0b428dbULL ^ n);
  lo ^= mum(hi ^ 0x8ebc6af09c88c6e3ULL, 0x589965cc75374cc3ULL);
  hi ^= mum(lo ^ 0x1d8e4e27c47d124fULL, 0x9E3779B97F4A7C15ULL);
  lo ^= mum(hi ^ 0xbf58476d1ce4e5b9ULL, 0x94d049bb133111ebULL);
  return static_cast<__uint128_t>(hi) << 64 | lo;
}

} // namespace detail

// One 128-bit signature per key drives everything: the bucket id (low half),
// the equation positions (both halves) and the prefilter probe (high half).
// It is a fixed function of the key, so every group, lane and worker agrees on
// a key's signature without coordination.
inline __uint128_t hashKey(const char *data, size_t length) {
  if (length < 16) {
    return detail::hashShort(data, length);
  }
  uint64_t hash1 = 0;
  uint64_t hash2 = 0;
  SpookyHash::Hash128(static_cast<const void *>(data), length, &hash1, &hash2);
  return (static_cast<__uint128_t>(hash1) << 64) | hash2;
}

inline __uint128_t hashKey(const std::string &key) {
  return hashKey(key.data(), key.size());
}

// The prefilter probe hash: the high half of the signature, so a query gets
// it for free.
inline uint64_t filterHash(const __uint128_t &signature) {
  return static_cast<uint64_t>(signature >> 64);
}

inline uint64_t filterHash(const char *data, size_t length) {
  return filterHash(hashKey(data, length));
}

inline uint32_t getBucketID(const __uint128_t &signature,
                            uint32_t num_buckets) {
  uint64_t bucket_hash = static_cast<uint64_t>(signature);
  return ((__uint128_t)(bucket_hash >> 1) * (__uint128_t)(num_buckets << 1)) >>
         64;
}

} // namespace caramel
