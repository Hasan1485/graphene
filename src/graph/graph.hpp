#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace graphene {

// ---------------------------------------------------------------------------
// Core scalar types
// ---------------------------------------------------------------------------
// NodeId: a 0-indexed node identifier. uint32_t comfortably covers the largest
//         DIMACS USA network (~24M nodes) while staying half the size of a
//         64-bit id, which keeps the CSR arrays cache-friendly.
// Weight: an integer edge weight (DIMACS weights are integers — travel time or
//         distance). Integers are faster to compare than doubles and avoid any
//         floating-point ambiguity in shortest-path correctness.
using NodeId = uint32_t;
using Weight = uint32_t;

// Sentinel values used throughout the algorithms.
inline constexpr NodeId kInvalidNode = static_cast<NodeId>(-1);

// A geographic coordinate (degrees). Empty/zero when a graph has no coordinates.
struct Coord {
    double lat = 0.0;
    double lon = 0.0;
};

// ---------------------------------------------------------------------------
// Graph — Compressed Sparse Row (CSR) adjacency
// ---------------------------------------------------------------------------
// CSR stores the whole adjacency structure in three flat arrays. It is the
// representation real routing engines use because it is cache-friendly (a node's
// out-edges are contiguous) and trivially serializable.
//
// For a node u, its out-edges live at indices [offsets[u], offsets[u+1]):
//
//     for (uint32_t i = offsets[u]; i < offsets[u + 1]; ++i) {
//         NodeId v = targets[i];   // edge u -> v
//         Weight w = weights[i];   // with weight w
//     }
//
// The arrays are public by design: the hot inner loops of the algorithms read
// them directly, and small accessor methods are provided for readability.
class Graph {
public:
    std::vector<uint32_t> offsets;  // size n + 1 (prefix sums of out-degree)
    std::vector<NodeId> targets;    // size m (edge endpoints)
    std::vector<Weight> weights;    // size m (edge weights)
    std::vector<Coord> coords;      // size n, or empty if the graph has none

    // Number of nodes. offsets has n + 1 entries, so n = size - 1.
    std::size_t numNodes() const { return offsets.empty() ? 0 : offsets.size() - 1; }

    // Number of directed edges.
    std::size_t numEdges() const { return targets.size(); }

    // Out-degree of node u.
    uint32_t degree(NodeId u) const { return offsets[u + 1] - offsets[u]; }

    // Average out-degree (edges per node); 0 for an empty graph.
    double avgDegree() const {
        const std::size_t n = numNodes();
        return n == 0 ? 0.0 : static_cast<double>(numEdges()) / static_cast<double>(n);
    }

    // True if per-node coordinates are present (needed for the A* heuristic).
    bool hasCoords() const { return !coords.empty(); }
};

}  // namespace graphene
