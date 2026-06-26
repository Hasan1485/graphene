// graphene_cli — command-line driver for the routing engine.
//
// Subcommands:
//   gen-grid   <rows> <cols> <out.bin> [maxWeight]   generate a grid graph
//   gen-random <n> <avgDeg> <seed> <out.bin>         generate a random graph
//   convert    <file.gr> <file.co|-> <out.bin>       DIMACS -> binary
//   info       <graph>                               print node/edge stats
//   query      <graph> <src> <dst> [algos...]        run a single query
//   bench      <graph> <numQueries> <seed> <out.json> [algos...]
//
// `<graph>` may be a binary (.bin) cache or a DIMACS .gr file.

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/factory.hpp"
#include "benchmark/benchmark.hpp"
#include "benchmark/json_export.hpp"
#include "graph/dimacs.hpp"
#include "graph/generator.hpp"
#include "graph/serialize.hpp"
#include "util/log.hpp"

namespace {

using namespace graphene;

// Load a graph from a .bin cache (fast) or a DIMACS .gr file (no coordinates).
Graph loadGraphAuto(const std::string& path) {
    const bool isBinary = path.size() >= 4 && path.substr(path.size() - 4) == ".bin";
    log::info("loading ", (isBinary ? "binary" : "DIMACS"), " graph ", path, " ...");

    log::Stopwatch sw;
    Graph g = isBinary ? Serializer::load(path) : Dimacs::load(path);
    log::info("loaded ", g.numNodes(), " nodes, ", g.numEdges(), " edges in ",
              static_cast<long>(sw.ms()), " ms");
    return g;
}

int usage() {
    std::cerr << "usage: graphene_cli <command> [args]\n"
                 "  gen-grid   <rows> <cols> <out.bin> [maxWeight]\n"
                 "  gen-random <n> <avgDeg> <seed> <out.bin>\n"
                 "  convert    <file.gr> <file.co|-> <out.bin>\n"
                 "  info       <graph>\n"
                 "  query      <graph> <src> <dst> [algos...]\n"
                 "  bench      <graph> <numQueries> <seed> <out.json> [algos...]\n";
    return 1;
}

int cmdGenGrid(const std::vector<std::string>& a) {
    if (a.size() < 3) return usage();
    const auto rows = static_cast<uint32_t>(std::stoul(a[0]));
    const auto cols = static_cast<uint32_t>(std::stoul(a[1]));
    const std::string out = a[2];
    const Weight maxW = a.size() > 3 ? static_cast<Weight>(std::stoul(a[3])) : 1;

    log::info("generating ", rows, "x", cols, " grid (maxWeight=", maxW, ") ...");
    log::Stopwatch sw;
    Graph g = Generator::gridGraph(rows, cols, maxW);
    log::info("generated ", g.numNodes(), " nodes, ", g.numEdges(), " edges in ",
              static_cast<long>(sw.ms()), " ms");
    Serializer::save(g, out);
    log::info("wrote ", out);
    return 0;
}

int cmdGenRandom(const std::vector<std::string>& a) {
    if (a.size() < 4) return usage();
    const auto n = static_cast<uint32_t>(std::stoul(a[0]));
    const auto avgDeg = static_cast<uint32_t>(std::stoul(a[1]));
    const auto seed = static_cast<uint64_t>(std::stoull(a[2]));
    const std::string out = a[3];

    log::info("generating random graph (n=", n, ", avgDeg=", avgDeg, ", seed=", seed, ") ...");
    log::Stopwatch sw;
    Graph g = Generator::randomSparse(n, avgDeg, seed);
    log::info("generated ", g.numNodes(), " nodes, ", g.numEdges(), " edges in ",
              static_cast<long>(sw.ms()), " ms");
    Serializer::save(g, out);
    log::info("wrote ", out);
    return 0;
}

int cmdConvert(const std::vector<std::string>& a) {
    if (a.size() < 3) return usage();
    const std::string gr = a[0];
    const std::string co = (a[1] == "-") ? "" : a[1];
    const std::string out = a[2];

    log::info("parsing DIMACS ", gr, (co.empty() ? "" : " + "), co, " ...");
    log::Stopwatch sw;
    Graph g = Dimacs::load(gr, co);
    log::info("built CSR: ", g.numNodes(), " nodes, ", g.numEdges(),
              " edges, coords=", (g.hasCoords() ? "yes" : "no"), " in ", static_cast<long>(sw.ms()),
              " ms");
    Serializer::save(g, out);
    log::info("wrote binary cache ", out);
    return 0;
}

int cmdInfo(const std::vector<std::string>& a) {
    if (a.empty()) return usage();
    Graph g = loadGraphAuto(a[0]);
    std::cout << "nodes:     " << g.numNodes() << "\n"
              << "edges:     " << g.numEdges() << "\n"
              << "avgDegree: " << g.avgDegree() << "\n"
              << "coords:    " << (g.hasCoords() ? "yes" : "no") << "\n";
    return 0;
}

int cmdQuery(const std::vector<std::string>& a) {
    if (a.size() < 3) return usage();
    Graph g = loadGraphAuto(a[0]);
    const auto src = static_cast<NodeId>(std::stoul(a[1]));
    const auto dst = static_cast<NodeId>(std::stoul(a[2]));

    std::vector<std::string> algos(a.begin() + 3, a.end());
    if (algos.empty()) algos = defaultAlgorithmNames();

    log::info("querying ", src, " -> ", dst, " with ", algos.size(), " algorithm(s)");
    for (const std::string& name : algos) {
        auto algo = makeAlgorithm(name);
        if (!algo) {
            std::cerr << "unknown algorithm: " << name << "\n";
            return 1;
        }
        const Result r = algo->solve(g, src, dst);
        std::cout << algo->name() << ": ";
        if (r.reachable()) {
            std::cout << "cost=" << r.cost << " hops=" << (r.path.size() - 1)
                      << " expanded=" << r.nodesExpanded << " time=" << r.timeMs << "ms\n";
        } else {
            std::cout << "unreachable (expanded=" << r.nodesExpanded << ")\n";
        }
    }
    return 0;
}

int cmdBench(const std::vector<std::string>& a) {
    if (a.size() < 4) return usage();
    Graph g = loadGraphAuto(a[0]);
    const auto numQueries = static_cast<uint64_t>(std::stoull(a[1]));
    const auto seed = static_cast<uint64_t>(std::stoull(a[2]));
    const std::string out = a[3];

    std::vector<std::string> algos(a.begin() + 4, a.end());
    if (algos.empty()) algos = defaultAlgorithmNames();

    log::info("generating ", numQueries, " random queries (seed=", seed, ") ...");
    const auto queries = generateQueries(g.numNodes(), numQueries, seed);

    log::info("benchmarking ", algos.size(), " algorithm(s) x ", queries.size(),
              " queries (this can take a while on large graphs) ...");
    log::Stopwatch sw;
    const auto report = runBenchmark(g, algos, queries, seed, [&](const std::string& name) {
        log::info("  running ", name, " ...");
    });
    log::info("done in ", static_cast<long>(sw.ms()), " ms");

    writeReport(report, out);
    log::info("wrote results JSON to ", out);

    // Aggregate summary -> stdout (the actual command output).
    std::printf("\n=== benchmark summary: %llu queries on %zu nodes / %zu edges ===\n",
                static_cast<unsigned long long>(report.numQueries), report.numNodes,
                report.numEdges);
    std::printf("%-18s %10s %10s %10s %15s\n", "algorithm", "mean(ms)", "median(ms)", "p95(ms)",
                "meanExpanded");
    std::printf("%-18s %10s %10s %10s %15s\n", "------------------", "--------", "--------",
                "--------", "------------");
    for (const auto& s : report.summaries) {
        std::printf("%-18s %10.4f %10.4f %10.4f %15.1f\n", s.algorithm.c_str(), s.meanTimeMs,
                    s.medianTimeMs, s.p95TimeMs, s.meanNodesExpanded);
    }
    std::fflush(stdout);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string cmd = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);

    try {
        if (cmd == "gen-grid") return cmdGenGrid(args);
        if (cmd == "gen-random") return cmdGenRandom(args);
        if (cmd == "convert") return cmdConvert(args);
        if (cmd == "info") return cmdInfo(args);
        if (cmd == "query") return cmdQuery(args);
        if (cmd == "bench") return cmdBench(args);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return usage();
}
