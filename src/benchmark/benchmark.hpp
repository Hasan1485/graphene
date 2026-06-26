#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "graph/graph.hpp"

namespace graphene {

// A single (source, destination) routing query.
struct Query {
    NodeId src = 0;
    NodeId dst = 0;
};

// Metrics for one algorithm running one query.
struct QueryMetric {
    std::string algorithm;
    NodeId src = 0;
    NodeId dst = 0;
    bool reachable = false;
    Weight cost = 0;
    uint64_t nodesExpanded = 0;
    double timeMs = 0.0;
};

// Aggregated metrics for one algorithm across the whole query set.
struct AlgorithmSummary {
    std::string algorithm;
    uint64_t queries = 0;
    uint64_t reachable = 0;
    double meanTimeMs = 0.0;
    double medianTimeMs = 0.0;
    double p95TimeMs = 0.0;
    double meanNodesExpanded = 0.0;
};

// Full benchmark output: the configuration, every per-query measurement, and the
// per-algorithm summaries.
struct BenchmarkReport {
    uint64_t numQueries = 0;
    uint64_t seed = 0;
    std::size_t numNodes = 0;
    std::size_t numEdges = 0;
    std::vector<QueryMetric> perQuery;
    std::vector<AlgorithmSummary> summaries;
};

// Generate `count` deterministic random (src, dst) queries over [0, numNodes).
// Self-loops (src == dst) are avoided.
std::vector<Query> generateQueries(std::size_t numNodes, uint64_t count, uint64_t seed);

// Optional progress callback, invoked with each algorithm's name just before it
// runs. Lets a caller print live progress without coupling the runner to any
// particular logger.
using ProgressFn = std::function<void(const std::string& algorithmName)>;

// Run every named algorithm over the same query set against `g` and return the
// per-query metrics plus aggregate summaries. Unknown algorithm names throw.
BenchmarkReport runBenchmark(const Graph& g, const std::vector<std::string>& algorithms,
                             const std::vector<Query>& queries, uint64_t seed,
                             const ProgressFn& onAlgorithm = {});

}  // namespace graphene
