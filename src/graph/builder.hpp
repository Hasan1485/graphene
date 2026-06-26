#pragma once

#include <cstddef>
#include <vector>

#include "graph/graph.hpp"

namespace graphene {

// A single directed edge in an edge list, before it is compressed into CSR.
struct Edge {
    NodeId from = 0;
    NodeId to = 0;
    Weight weight = 0;
};

// Build a CSR Graph from an edge list.
//
// Each Edge is treated as a single *directed* edge from -> to. If you want an
// undirected graph, add both directions to the edge list before calling this.
//
// Algorithm (two passes, O(n + m)):
//   1. Count the out-degree of every node.
//   2. Prefix-sum the degrees into `offsets` (size n + 1).
//   3. Place each edge's target/weight at a moving per-node cursor.
//
// `coords` is moved into the resulting graph as-is (pass an empty vector if the
// graph has no coordinates).
Graph buildCSR(std::size_t numNodes, const std::vector<Edge>& edges,
               std::vector<Coord> coords = {});

}  // namespace graphene
