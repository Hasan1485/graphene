#pragma once

#include <cstdint>

#include "graph/graph.hpp"

namespace graphene {

// Synthetic graph generators. These are deterministic (no I/O, seeded RNG) so
// every phase can be developed and tested offline before any DIMACS download.
class Generator {
public:
    // 4-neighbour grid of `rows` x `cols` nodes (node id = row * cols + col).
    //
    // Edges are added in both directions. Weights are unit by default; pass a
    // non-zero `maxWeight` to draw weights uniformly from [1, maxWeight] with a
    // seeded RNG (useful to make Dijkstra differ from BFS).
    //
    // Coordinates are set to (lat = row, lon = col) so the A* heuristic has a
    // meaningful geometry to work with.
    static Graph gridGraph(uint32_t rows, uint32_t cols, Weight maxWeight = 1, uint64_t seed = 42);

    // Random sparse graph with `n` nodes and roughly `avgDegree` out-edges per
    // node. Deterministic for a given seed. Edges are undirected (added both
    // ways) so the graph is well connected for shortest-path queries. Random
    // 2D coordinates are generated so the A* heuristic is usable.
    static Graph randomSparse(uint32_t n, uint32_t avgDegree, uint64_t seed = 42);
};

}  // namespace graphene
