#pragma once

#include "algorithms/algorithm.hpp"

namespace graphene {

// Dijkstra's algorithm with a binary heap (std::priority_queue) and lazy
// deletion: instead of decreasing keys we push duplicates and skip stale pops.
// This is simple and, for sparse road networks, as fast in practice as a
// decrease-key heap. Produces the exact least-weight path.
class Dijkstra : public Algorithm {
public:
    std::string name() const override { return "dijkstra"; }
    Result solve(const Graph& g, NodeId src, NodeId dst) override;
};

}  // namespace graphene
