#pragma once

#include "BucketedHashStore.h"
#include "Codec.h"
#include "Modulo2System.h"

namespace caramel {

class Csf;
using CsfPtr = std::shared_ptr<Csf>;

/*
Constructs a binary system of linear equations to solve for each bit of the
encoded values for each key.

Arguments:
    key_signatures: An iterable collection of N unique signatures.
    values: An interable collection of N values corresponding to signatures
    encoded_values: An iterable collection of N bitarrays, representing the
        encoded values.
    seed: A seed for hashing.

    Returns:
            SparseSystemPtr to solve for each key's encoded bits.
*/
SparseSystemPtr
constructModulo2System(const std::vector<Uint128Signature> &key_signatures,
                       const std::vector<uint32_t> &values,
                       const CodeDict &codedict, uint32_t seed);

using SubsystemSolutionSeedPair = std::pair<BitArrayPtr, uint32_t>;

SubsystemSolutionSeedPair
constructAndSolveSubsystem(const std::vector<Uint128Signature> &key_signatures,
                           const std::vector<uint32_t> &values,
                           const CodeDict &codedict);

CsfPtr constructCsf(const std::vector<std::string> &keys,
                    const std::vector<uint32_t> &values);

} // namespace caramel