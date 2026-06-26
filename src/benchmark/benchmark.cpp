#include "benchmark/benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

#include "algorithms/factory.hpp"

namespace graphene {

namespace {

// Percentile of an already-sorted vector using the nearest-rank method.
double percentile(const std::vector<double>& sorted, double pct) {
    if (sorted.empty()) return 0.0;
    const double rank = pct / 100.0 * static_cast<double>(sorted.size() - 1);
    const auto idx = static_cast<std::size_t>(std::round(rank));
    return sorted[std::min(idx, sorted.size() - 1)];
}

}  // namespace

std::vector<Query> generateQueries(std::size_t numNodes, uint64_t count, uint64_t seed) {
    std::vector<Query> queries;
    if (numNodes < 2) return queries;

    queries.reserve(count);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<NodeId> pick(0, static_cast<NodeId>(numNodes - 1));

    for (uint64_t i = 0; i < count; ++i) {
        NodeId s = pick(rng);
        NodeId d = pick(rng);
        while (d == s) d = pick(rng);  // avoid trivial self-queries
        queries.push_back({s, d});
    }
    return queries;
}

BenchmarkReport runBenchmark(const Graph& g, const std::vector<std::string>& algorithms,
                             const std::vector<Query>& queries, uint64_t seed,
                             const ProgressFn& onAlgorithm) {
    BenchmarkReport report;
    report.numQueries = queries.size();
    report.seed = seed;
    report.numNodes = g.numNodes();
    report.numEdges = g.numEdges();

    for (const std::string& algoName : algorithms) {
        auto algo = makeAlgorithm(algoName);
        if (!algo) {
            throw std::invalid_argument("unknown algorithm: " + algoName);
        }
        if (onAlgorithm) onAlgorithm(algo->name());

        AlgorithmSummary summary;
        summary.algorithm = algo->name();
        summary.queries = queries.size();

        std::vector<double> times;
        times.reserve(queries.size());
        double sumTime = 0.0;
        double sumExpanded = 0.0;

        for (const Query& q : queries) {
            const Result r = algo->solve(g, q.src, q.dst);

            QueryMetric m;
            m.algorithm = summary.algorithm;
            m.src = q.src;
            m.dst = q.dst;
            m.reachable = r.reachable();
            m.cost = r.cost;
            m.nodesExpanded = r.nodesExpanded;
            m.timeMs = r.timeMs;
            report.perQuery.push_back(std::move(m));

            times.push_back(r.timeMs);
            sumTime += r.timeMs;
            sumExpanded += static_cast<double>(r.nodesExpanded);
            if (r.reachable()) ++summary.reachable;
        }

        std::sort(times.begin(), times.end());
        const double count = static_cast<double>(queries.empty() ? 1 : queries.size());
        summary.meanTimeMs = sumTime / count;
        summary.medianTimeMs = percentile(times, 50.0);
        summary.p95TimeMs = percentile(times, 95.0);
        summary.meanNodesExpanded = sumExpanded / count;

        report.summaries.push_back(std::move(summary));
    }

    return report;
}

}  // namespace graphene
