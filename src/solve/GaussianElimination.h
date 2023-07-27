#pragma once

#include <src/BitArray.h>
#include <src/Modulo2System.h>

namespace caramel {

/*
We perform Gaussian Elimination by maintaining the state of each equation's
"first_var". The first_var is the index of the first non-zero bit in the
equation. Overall this algorithm works as follows:

    1. Calculate the first var for each equation in relevant_equation_ids.
    2. Iterate through all ordered pairs of equations and swap/xor them around
        to get them into echelon form. We can break down each ordered pair of
        equations into a Top equation and a Bot equation. The general steps in
        this process are:

        A. Check if both equations have the same first var. If so then we set
        Bot equation equal to Bot equation XORed with the Top equation.
        B. Verify that Top equation's first var is greater than Bot equation's
        first var. Otherwise, swap these two equations.

    3. Back-substitution. Go backwards through the matrix (from bottom to top in
    the echelon form matrix) and set the bit of the solution to whatever
    resolves the constant.

Throws UnsolvableSystemException.
*/

BitArrayPtr
gaussianElimination(const DenseSystemPtr &dense_system,
                    const std::vector<uint32_t> &relevant_equation_ids);

} // namespace caramel