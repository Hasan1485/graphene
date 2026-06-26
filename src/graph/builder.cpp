#include "graph/builder.hpp"

#include <stdexcept>

namespace graphene {

Graph buildCSR(std::size_t numNodes, const std::vector<Edge>& edges, std::vector<Coord> coords) {
    Graph g;

    // offsets has one slot per node plus a trailing total, initialised to zero.
    g.offsets.assign(numNodes + 1, 0);

    // --- Pass 1: count out-degree of each node ---
    // We accumulate counts into offsets[u + 1] so that the prefix sum below
    // naturally lands the start of node u's edges at offsets[u].
    for (const Edge& e : edges) {
        if (e.from >= numNodes || e.to >= numNodes) {
            throw std::out_of_range("buildCSR: edge endpoint exceeds node count");
        }
        ++g.offsets[e.from + 1];
    }

    // --- Pass 2: prefix-sum the degrees into offsets ---
    for (std::size_t i = 1; i <= numNodes; ++i) {
        g.offsets[i] += g.offsets[i - 1];
    }

    const std::size_t numEdges = edges.size();
    g.targets.resize(numEdges);
    g.weights.resize(numEdges);

    // --- Pass 3: scatter each edge into its slot ---
    // `cursor` is a private copy of offsets so we can advance per-node write
    // positions without clobbering the real offsets array.
    std::vector<uint32_t> cursor(g.offsets.begin(), g.offsets.end());
    for (const Edge& e : edges) {
        const uint32_t pos = cursor[e.from]++;
        g.targets[pos] = e.to;
        g.weights[pos] = e.weight;
    }

    g.coords = std::move(coords);
    return g;
}

}  // namespace graphene
