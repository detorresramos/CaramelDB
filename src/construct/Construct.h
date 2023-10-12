#pragma once

#include "BucketedHashStore.h"
#include "Codec.h"
#include "Csf.h"
#include <array>
#include <functional>
#include <src/Modulo2System.h>

namespace caramel {

/**
 * Constructs a Csf from the given keys and values.
 */
template <typename T>
CsfPtr<T> constructCsf(const std::vector<std::string> &keys,
                       const std::vector<T> &values, bool verbose = true);

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
template <typename T>
SparseSystemPtr
constructModulo2System(const std::vector<Uint128Signature> &key_signatures,
                       const std::vector<T> &values,
                       const CodeDict<T> &codedict, uint32_t seed);

template <typename T>
SubsystemSolutionSeedPair
constructAndSolveSubsystem(const std::vector<Uint128Signature> &key_signatures,
                           const std::vector<T> &values,
                           const CodeDict<T> &codedict);

} // namespace caramel