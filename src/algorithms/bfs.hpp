#pragma once

#include "algorithms/algorithm.hpp"

namespace graphene {

// Breadth-first search. Ignores edge weights and finds the path with the fewest
// *hops*. It is the sanity baseline: cheap, obviously correct, and a useful
// reference for "how connected are these two nodes". The reported `cost` is the
// number of edges in the path (hop count), not a weighted distance.
class BFS : public Algorithm {
public:
    std::string name() const override { return "bfs"; }
    Result solve(const Graph& g, NodeId src, NodeId dst) override;
};

}  // namespace graphene
