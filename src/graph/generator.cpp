#include "graph/generator.hpp"

#include <random>
#include <vector>

#include "graph/builder.hpp"

namespace graphene {

namespace {

// Degrees of latitude/longitude assigned per grid cell. Kept small so that even
// a large grid stays inside valid lat/lon ranges, which keeps the projection-
// based heuristic well-defined.
constexpr double kGridDegPerCell = 1e-3;

// Append both directions of an undirected edge with the same weight.
void addUndirected(std::vector<Edge>& edges, NodeId a, NodeId b, Weight w) {
    edges.push_back({a, b, w});
    edges.push_back({b, a, w});
}

}  // namespace

Graph Generator::gridGraph(uint32_t rows, uint32_t cols, Weight maxWeight, uint64_t seed) {
    const std::size_t n = static_cast<std::size_t>(rows) * cols;
    std::vector<Edge> edges;
    edges.reserve(n * 4);  // up to 4 directed edges per node

    std::mt19937_64 rng(seed);
    // Draw weights from [1, maxWeight]; with maxWeight == 1 every weight is 1.
    std::uniform_int_distribution<Weight> weightDist(1, maxWeight < 1 ? 1 : maxWeight);

    auto id = [cols](uint32_t r, uint32_t c) -> NodeId {
        return static_cast<NodeId>(r) * cols + c;
    };

    // Add the edge to the right neighbour and the edge to the neighbour below.
    // Iterating this way visits every adjacency exactly once.
    for (uint32_t r = 0; r < rows; ++r) {
        for (uint32_t c = 0; c < cols; ++c) {
            if (c + 1 < cols) {
                addUndirected(edges, id(r, c), id(r, c + 1), weightDist(rng));
            }
            if (r + 1 < rows) {
                addUndirected(edges, id(r, c), id(r + 1, c), weightDist(rng));
            }
        }
    }

    // Coordinates: place each node on a small lat/lon grid so A* has geometry.
    std::vector<Coord> coords(n);
    for (uint32_t r = 0; r < rows; ++r) {
        for (uint32_t c = 0; c < cols; ++c) {
            coords[id(r, c)] = Coord{r * kGridDegPerCell, c * kGridDegPerCell};
        }
    }

    return buildCSR(n, edges, std::move(coords));
}

Graph Generator::randomSparse(uint32_t n, uint32_t avgDegree, uint64_t seed) {
    std::vector<Edge> edges;
    if (n == 0) {
        return buildCSR(0, edges);
    }

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<Weight> weightDist(1, 1000);
    std::uniform_real_distribution<double> coordDist(0.0, 1.0);

    // 1) Spanning tree: link node i to a random earlier node. This guarantees
    //    the whole graph is connected so shortest-path queries are meaningful.
    for (NodeId i = 1; i < n; ++i) {
        std::uniform_int_distribution<NodeId> earlier(0, i - 1);
        addUndirected(edges, i, earlier(rng), weightDist(rng));
    }

    // 2) Add extra random edges until we reach roughly the requested average
    //    degree. Each undirected edge contributes 2 to the directed edge count,
    //    hence the /2.
    const std::size_t targetUndirected = (static_cast<std::size_t>(n) * avgDegree) / 2;
    std::uniform_int_distribution<NodeId> anyNode(0, n - 1);
    while (edges.size() / 2 < targetUndirected) {
        NodeId u = anyNode(rng);
        NodeId v = anyNode(rng);
        if (u != v) {
            addUndirected(edges, u, v, weightDist(rng));
        }
    }

    // Random coordinates inside a 1-degree box (valid lat/lon for the heuristic).
    std::vector<Coord> coords(n);
    for (NodeId i = 0; i < n; ++i) {
        coords[i] = Coord{coordDist(rng), coordDist(rng)};
    }

    return buildCSR(n, edges, std::move(coords));
}

}  // namespace graphene
