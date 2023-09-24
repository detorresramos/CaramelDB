// Spooky Hash
// A 128-bit noncryptographic hash, for checksums and table lookup
// By Bob Jenkins.  Public domain.
//   Oct 31 2010: published framework, disclaimer ShortHash isn't right
//   Nov 7 2010: disabled ShortHash
//   Oct 31 2011: replace End, ShortMix, ShortEnd, enable ShortHash again
//   April 10 2012: buffer overflow on platforms without unaligned reads
//   July 12 2012: was passing out variables in final to in/out in short
//   July 30 2012: I reintroduced the buffer overflow
//   August 5 2012: SpookyV2: d = should be d += in short hash, and remove extra
//   mix from long hash

#include "SpookyHash.h"
#include <memory.h>

#define ALLOW_UNALIGNED_READS 1

namespace caramel {

//
// short hash ... it could be used on any message,
// but it's used by Spooky just for short messages.
//
void SpookyHash::Short(const void *message, size_t length, uint64 *hash1,
                       uint64 *hash2) {
  uint64 buf[2 * sc_numVars];
  union {
    const uint8 *p8;
    uint32 *p32;
    uint64 *p64;
    size_t i;
  } u;

  u.p8 = (const uint8 *)message;

  if (!ALLOW_UNALIGNED_READS && (u.i & 0x7)) {
    memcpy(buf, message, length);
    u.p64 = buf;
  }

  size_t remainder = length % 32;
  uint64 a = *hash1;
  uint64 b = *hash2;
  uint64 c = sc_const;
  uint64 d = sc_const;

  if (length > 15) {
    const uint64 *end = u.p64 + (length / 32) * 4;

    // handle all complete sets of 32 bytes
    for (; u.p64 < end; u.p64 += 4) {
      c += u.p64[0];
      d += u.p64[1];
      ShortMix(a, b, c, d);
      a += u.p64[2];
      b += u.p64[3];
    }

    // Handle the case of 16+ remaining bytes.
    if (remainder >= 16) {
      c += u.p64[0];
      d += u.p64[1];
      ShortMix(a, b, c, d);
      u.p64 += 2;
      remainder -= 16;
    }
  }

  // Handle the last 0..15 bytes, and its length
  d += ((uint64)length) << 56;
  switch (remainder) {
  case 15:
    d += ((uint64)u.p8[14]) << 48;
    [[fallthrough]];
  case 14:
    d += ((uint64)u.p8[13]) << 40;
    [[fallthrough]];
  case 13:
    d += ((uint64)u.p8[12]) << 32;
    [[fallthrough]];
  case 12:
    d += u.p32[2];
    c += u.p64[0];
    break;
  case 11:
    d += ((uint64)u.p8[10]) << 16;
    [[fallthrough]];
  case 10:
    d += ((uint64)u.p8[9]) << 8;
    [[fallthrough]];
  case 9:
    d += (uint64)u.p8[8];
    [[fallthrough]];
  case 8:
    c += u.p64[0];
    break;
  case 7:
    c += ((uint64)u.p8[6]) << 48;
    [[fallthrough]];
  case 6:
    c += ((uint64)u.p8[5]) << 40;
    [[fallthrough]];
  case 5:
    c += ((uint64)u.p8[4]) << 32;
    [[fallthrough]];
  case 4:
    c += u.p32[0];
    break;
  case 3:
    c += ((uint64)u.p8[2]) << 16;
    [[fallthrough]];
  case 2:
    c += ((uint64)u.p8[1]) << 8;
    [[fallthrough]];
  case 1:
    c += (uint64)u.p8[0];
    break;
  case 0:
    c += sc_const;
    d += sc_const;
  }
  ShortEnd(a, b, c, d);
  *hash1 = a;
  *hash2 = b;
}

// do the whole hash in one call
void SpookyHash::Hash128(const void *message, size_t length, uint64 *hash1,
                         uint64 *hash2) {
  if (length < sc_bufSize) {
    Short(message, length, hash1, hash2);
    return;
  }

  uint64 h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;
  uint64 buf[sc_numVars];
  uint64 *end;
  union {
    const uint8 *p8;
    uint64 *p64;
    size_t i;
  } u;
  size_t remainder;

  h0 = h3 = h6 = h9 = *hash1;
  h1 = h4 = h7 = h10 = *hash2;
  h2 = h5 = h8 = h11 = sc_const;

  u.p8 = (const uint8 *)message;
  end = u.p64 + (length / sc_blockSize) * sc_numVars;

  // handle all whole sc_blockSize blocks of bytes
  if (ALLOW_UNALIGNED_READS || ((u.i & 0x7) == 0)) {
    while (u.p64 < end) {
      Mix(u.p64, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
      u.p64 += sc_numVars;
    }
  } else {
    while (u.p64 < end) {
      memcpy(buf, u.p64, sc_blockSize);
      Mix(buf, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
      u.p64 += sc_numVars;
    }
  }

  // handle the last partial block of sc_blockSize bytes
  remainder = (length - ((const uint8 *)end - (const uint8 *)message));
  memcpy(buf, end, remainder);
  memset(((uint8 *)buf) + remainder, 0, sc_blockSize - remainder);
  ((uint8 *)buf)[sc_blockSize - 1] = remainder;

  // do some final mixing
  End(buf, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
  *hash1 = h0;
  *hash2 = h1;
}

} // namespace caramel