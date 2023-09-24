#pragma once

#include "BucketedHashStore.h"
#include "Codec.h"
#include "Csf.h"
#include <src/Modulo2System.h>

namespace caramel {

/**
 * Constructs a Csf from the given keys and values.
 */
CsfPtr constructCsf(const std::vector<std::string> &keys,
                    const std::vector<uint32_t> &values);

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

SubsystemSolutionSeedPair
constructAndSolveSubsystem(const std::vector<Uint128Signature> &key_signatures,
                           const std::vector<uint32_t> &values,
                           const CodeDict &codedict);

} // namespace caramel