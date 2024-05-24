#pragma once

#include <src/Modulo2System.h>

namespace caramel {

/*
We implement the hypergraph peeling method of "Cache-Oblivious Peeling of
Random Hypergraphs" by Belazzougui1, Boldi, Ottaviano, Venturini, and Vigna
(https://arxiv.org/pdf/1312.0526.pdf). The method described in the paper is
a slightly more complicated version of the one that is actually used in
practice and in the reference implementation. Also, note that in the context
of linear systems, the term "edge" refers to "equation" and "vertex" refers to
"variable". We use the simpler method, which proceeds as follows:

Given a hypergraph, we begin by scanning the hypergraph for a degree-1 variable
(that is, a variable that appears in exactly one equation). We peel this
equation from the system, and observe that this removal could have "freed up"
other edges. We can identify candidate equations to peel by checking the other
variables from the equation we just removed - if one of these variables now
appears in exactly 1 equation, we have found our next equation to peel. We
recursively repeat this process until we cannot peel any more edges.
*/

std::tuple<std::vector<uint32_t>, std::vector<uint32_t>, std::vector<uint32_t>,
           SparseSystemPtr>
peelHypergraph(const SparseSystemPtr &sparse_system,
               const std::vector<uint64_t> &equation_ids);

BitArrayPtr solvePeeledFromDense(const std::vector<uint32_t> &peeled_ids,
                                 const std::vector<uint32_t> &solution_order,
                                 const SparseSystemPtr &sparse_system,
                                 const BitArrayPtr &dense_solution);

} // namespace caramel