#pragma once

#include "SpookyHash.h"
#include <stddef.h>
#include <stdint.h>

namespace caramel {

// size of the internal state
#define SC_BLOCKSIZE (SC_NUMVARS * 8U)

// size of buffer of unhashed data, in bytes
#define SC_BUFSIZE (2U * SC_BLOCKSIZE)

void spooky_short(const void *message, size_t length, uint64_t seed,
                  uint64_t *tuple);

void spooky_short_rehash(const Uint128Signature &signature, const uint64_t seed,
                         uint64_t *const tuple);

} // namespace caramel