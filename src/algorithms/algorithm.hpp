#pragma once

#include <string>
#include <vector>

#include "graph/graph.hpp"

namespace graphene {

// The outcome of a single shortest-path query.
struct Result {
    std::vector<NodeId> path;    // src..dst inclusive; empty = unreachable
    Weight cost = 0;             // total path cost (hop count for BFS)
    uint64_t nodesExpanded = 0;  // settled nodes (popped & finalized)
    double timeMs = 0.0;         // wall time of the solve only

    bool reachable() const { return !path.empty(); }
};

// Common interface for every routing algorithm. Implementations must be
// stateless across calls (all per-query state lives inside solve) so a single
// instance can be reused for many queries.
class Algorithm {
public:
    virtual ~Algorithm() = default;

    // Short, stable identifier used in benchmarks and the API (e.g. "dijkstra").
    virtual std::string name() const = 0;

    // Compute a shortest path from `src` to `dst` in `g`.
    virtual Result solve(const Graph& g, NodeId src, NodeId dst) = 0;
};

}  // namespace graphene
