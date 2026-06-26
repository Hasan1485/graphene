// Correctness tests for the graph core and algorithms.
//
// Uses a tiny self-contained check framework (no external test dependency) and
// returns a non-zero exit code on any failure so ctest reports it.

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/factory.hpp"
#include "graph/builder.hpp"
#include "graph/generator.hpp"
#include "graph/serialize.hpp"

using namespace graphene;

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                                          \
    do {                                                                                     \
        ++g_checks;                                                                          \
        if (!(cond)) {                                                                       \
            ++g_failures;                                                                    \
            std::cerr << "FAIL: " << #cond << "  @ " << __FILE__ << ":" << __LINE__ << "\n"; \
        }                                                                                    \
    } while (0)

// Build the tiny hand-checked graph used for algorithm correctness.
//
//   0 --1--> 1 --1--> 2 --1--> 3        (least-weight path, cost 3, 3 hops)
//   0 ------10------> 3                  (fewest-hops path, cost 10, 1 hop)
//   4                                    (isolated -> unreachable)
Graph makeTinyGraph() {
    std::vector<Edge> edges = {
        {0, 1, 1},
        {1, 2, 1},
        {2, 3, 1},
        {0, 3, 10},
    };
    std::vector<Coord> coords = {
        {0.0, 0.000}, {0.0, 0.001}, {0.0, 0.002}, {0.0, 0.003}, {0.0, 0.004},
    };
    return buildCSR(5, edges, std::move(coords));
}

void testCsrBuild() {
    Graph g = makeTinyGraph();
    CHECK(g.numNodes() == 5);
    CHECK(g.numEdges() == 4);
    CHECK(g.degree(0) == 2);
    CHECK(g.degree(1) == 1);
    CHECK(g.degree(4) == 0);

    // Spot-check node 0's adjacency: edges to 1 (w1) and 3 (w10).
    bool sawTo1 = false, sawTo3 = false;
    for (uint32_t i = g.offsets[0]; i < g.offsets[1]; ++i) {
        if (g.targets[i] == 1) {
            sawTo1 = true;
            CHECK(g.weights[i] == 1);
        }
        if (g.targets[i] == 3) {
            sawTo3 = true;
            CHECK(g.weights[i] == 10);
        }
    }
    CHECK(sawTo1);
    CHECK(sawTo3);
}

void testAlgorithmsTiny() {
    Graph g = makeTinyGraph();

    // BFS finds the fewest-hops path: 0 -> 3 directly (cost = hops = 1).
    auto bfs = makeAlgorithm("bfs")->solve(g, 0, 3);
    CHECK(bfs.reachable());
    CHECK(bfs.path == (std::vector<NodeId>{0, 3}));
    CHECK(bfs.cost == 1);

    // Dijkstra finds the least-weight path: 0 -> 1 -> 2 -> 3 (cost 3).
    auto dij = makeAlgorithm("dijkstra")->solve(g, 0, 3);
    CHECK(dij.reachable());
    CHECK(dij.path == (std::vector<NodeId>{0, 1, 2, 3}));
    CHECK(dij.cost == 3);

    // A* with the zero heuristic must reproduce Dijkstra exactly.
    auto az = makeAlgorithm("astar-zero")->solve(g, 0, 3);
    CHECK(az.cost == dij.cost);
    CHECK(az.path == dij.path);

    // A* with the Euclidean heuristic must find the same optimal cost.
    auto ae = makeAlgorithm("astar-euclidean")->solve(g, 0, 3);
    CHECK(ae.cost == dij.cost);

    // Node 4 is isolated: every algorithm reports it unreachable.
    CHECK(!makeAlgorithm("bfs")->solve(g, 0, 4).reachable());
    CHECK(!makeAlgorithm("dijkstra")->solve(g, 0, 4).reachable());
    CHECK(!makeAlgorithm("astar-euclidean")->solve(g, 0, 4).reachable());
}

void testSerializeRoundTrip() {
    // Phase 1 Definition of Done: generate a 1000-node grid, serialize, reload,
    // and confirm everything matches exactly.
    Graph g = Generator::gridGraph(25, 40, /*maxWeight=*/5);
    CHECK(g.numNodes() == 1000);

    const auto path = std::filesystem::temp_directory_path() / "graphene_roundtrip.bin";
    Serializer::save(g, path.string());
    Graph loaded = Serializer::load(path.string());
    std::filesystem::remove(path);

    CHECK(loaded.numNodes() == g.numNodes());
    CHECK(loaded.numEdges() == g.numEdges());
    CHECK(loaded.offsets == g.offsets);
    CHECK(loaded.targets == g.targets);
    CHECK(loaded.weights == g.weights);
    CHECK(loaded.coords.size() == g.coords.size());

    // Spot-check adjacency of a handful of nodes.
    for (NodeId u : {0u, 1u, 123u, 500u, 999u}) {
        CHECK(loaded.offsets[u] == g.offsets[u]);
        for (uint32_t i = g.offsets[u]; i < g.offsets[u + 1]; ++i) {
            CHECK(loaded.targets[i] == g.targets[i]);
            CHECK(loaded.weights[i] == g.weights[i]);
        }
    }
}

void testHeuristicInvariantOnGrid() {
    // Phase 2 Definition of Done (on synthetic data): for the same query,
    // Dijkstra cost == A*-zero cost == A*-Euclidean cost, and A*-Euclidean
    // expands strictly fewer nodes in aggregate than Dijkstra.
    Graph g = Generator::gridGraph(50, 50, /*maxWeight=*/10);

    auto dijkstra = makeAlgorithm("dijkstra");
    auto astarZero = makeAlgorithm("astar-zero");
    auto astarEuclid = makeAlgorithm("astar-euclidean");

    uint64_t totalDijkstra = 0;
    uint64_t totalEuclid = 0;
    const std::vector<std::pair<NodeId, NodeId>> queries = {
        {0, 2499}, {10, 2000}, {1225, 37}, {49, 2450}, {100, 2399},
    };

    for (auto [s, d] : queries) {
        const auto rd = dijkstra->solve(g, s, d);
        const auto rz = astarZero->solve(g, s, d);
        const auto re = astarEuclid->solve(g, s, d);

        CHECK(rd.cost == rz.cost);
        CHECK(rd.cost == re.cost);
        CHECK(re.nodesExpanded <= rd.nodesExpanded);

        totalDijkstra += rd.nodesExpanded;
        totalEuclid += re.nodesExpanded;
    }

    CHECK(totalEuclid < totalDijkstra);
    std::cout << "  [grid] Dijkstra expanded " << totalDijkstra << " vs A*-Euclidean "
              << totalEuclid << " (lower is better)\n";
}

}  // namespace

int main() {
    std::cout << "running graphene tests...\n";
    testCsrBuild();
    testAlgorithmsTiny();
    testSerializeRoundTrip();
    testHeuristicInvariantOnGrid();

    std::cout << (g_failures == 0 ? "PASS" : "FAIL") << ": " << (g_checks - g_failures) << "/"
              << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
