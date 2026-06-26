// graphene_api — Drogon HTTP server exposing the routing engine.
//
// The graph is loaded exactly once at startup (binary cache preferred, DIMACS as
// a fallback which is then cached as binary). Handlers are intentionally thin:
// they validate input, call into the core library, and serialize the result.
//
// Endpoints:
//   GET  /graph/info  -> { nodes, edges, avgDegree }
//   POST /query       -> body { src, dst, algorithms:[...] }
//   POST /benchmark   -> body { numQueries, seed, algorithms:[...] }
//
// Usage: graphene_api [graph.bin | file.gr [file.co]]
//   GRAPHENE_PORT environment variable overrides the default port (8080).

#include <drogon/drogon.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "algorithms/factory.hpp"
#include "benchmark/benchmark.hpp"
#include "benchmark/json_export.hpp"
#include "graph/dimacs.hpp"
#include "graph/generator.hpp"
#include "graph/serialize.hpp"
#include "util/log.hpp"

using namespace drogon;
using json = nlohmann::json;

namespace {

// The single graph instance shared (read-only) by all request handlers.
graphene::Graph g_graph;

// Build a JSON HTTP response with the given status code.
HttpResponsePtr jsonResponse(const json& body, HttpStatusCode code = k200OK) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(code);
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setBody(body.dump());
    return resp;
}

HttpResponsePtr errorResponse(const std::string& message, HttpStatusCode code) {
    return jsonResponse(json{{"error", message}}, code);
}

// Resolve the requested algorithm names, defaulting if none were supplied.
// Returns false and fills `err` if any name is unknown.
bool resolveAlgorithms(const json& body, const std::vector<std::string>& fallback,
                       std::vector<std::string>& out, std::string& err) {
    out.clear();
    if (body.contains("algorithms") && body["algorithms"].is_array()) {
        for (const auto& a : body["algorithms"]) {
            out.push_back(a.get<std::string>());
        }
    }
    if (out.empty()) out = fallback;

    for (const std::string& name : out) {
        if (!graphene::makeAlgorithm(name)) {
            err = "unknown algorithm: " + name;
            return false;
        }
    }
    return true;
}

// Load the graph from the command-line arguments (or generate a demo grid).
void loadGraphFromArgs(int argc, char** argv) {
    if (argc < 2) {
        graphene::log::info("no graph specified; generating a 200x200 demo grid");
        g_graph = graphene::Generator::gridGraph(200, 200, /*maxWeight=*/10);
        return;
    }

    const std::string path = argv[1];
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".bin") {
        graphene::log::info("loading binary graph: ", path);
        g_graph = graphene::Serializer::load(path);
        return;
    }

    // DIMACS path: load, then write a .bin cache next to it for fast restarts.
    const std::string co = (argc >= 3) ? argv[2] : "";
    graphene::log::info("loading DIMACS graph: ", path);
    g_graph = graphene::Dimacs::load(path, co);

    const std::string cache = path + ".bin";
    graphene::log::info("caching binary graph: ", cache);
    graphene::Serializer::save(g_graph, cache);
}

// ----- Handlers ------------------------------------------------------------

void handleGraphInfo(const HttpRequestPtr&,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
    callback(jsonResponse(json{
        {"nodes", g_graph.numNodes()},
        {"edges", g_graph.numEdges()},
        {"avgDegree", g_graph.avgDegree()},
    }));
}

void handleQuery(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback) {
    json body;
    try {
        body = json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(errorResponse("invalid JSON body", k400BadRequest));
        return;
    }

    if (!body.contains("src") || !body.contains("dst")) {
        callback(errorResponse("missing 'src' or 'dst'", k400BadRequest));
        return;
    }

    const auto n = static_cast<int64_t>(g_graph.numNodes());
    const int64_t src = body["src"].get<int64_t>();
    const int64_t dst = body["dst"].get<int64_t>();
    if (src < 0 || src >= n || dst < 0 || dst >= n) {
        callback(
            errorResponse("src/dst out of range [0, " + std::to_string(n) + ")", k400BadRequest));
        return;
    }

    std::vector<std::string> algos;
    std::string err;
    if (!resolveAlgorithms(body, {"dijkstra", "astar-euclidean"}, algos, err)) {
        callback(errorResponse(err, k400BadRequest));
        return;
    }

    json results = json::array();
    for (const std::string& name : algos) {
        auto algo = graphene::makeAlgorithm(name);
        const graphene::Result r = algo->solve(g_graph, static_cast<graphene::NodeId>(src),
                                               static_cast<graphene::NodeId>(dst));
        json entry = graphene::toJson(r);
        entry["algorithm"] = algo->name();
        results.push_back(std::move(entry));
    }
    callback(jsonResponse(results));
}

void handleBenchmark(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
    json body;
    try {
        body = json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(errorResponse("invalid JSON body", k400BadRequest));
        return;
    }

    const uint64_t numQueries = body.value("numQueries", uint64_t{100});
    const uint64_t seed = body.value("seed", uint64_t{42});

    std::vector<std::string> algos;
    std::string err;
    if (!resolveAlgorithms(body, graphene::defaultAlgorithmNames(), algos, err)) {
        callback(errorResponse(err, k400BadRequest));
        return;
    }

    const auto queries = graphene::generateQueries(g_graph.numNodes(), numQueries, seed);
    const auto report = graphene::runBenchmark(g_graph, algos, queries, seed);
    callback(jsonResponse(graphene::toJson(report)));
}

}  // namespace

int main(int argc, char** argv) {
    try {
        loadGraphFromArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "failed to load graph: " << e.what() << "\n";
        return 1;
    }

    graphene::log::info("graph ready: ", g_graph.numNodes(), " nodes, ", g_graph.numEdges(),
                        " edges");

    const char* portEnv = std::getenv("GRAPHENE_PORT");
    const uint16_t port = portEnv ? static_cast<uint16_t>(std::stoi(portEnv)) : 8080;

    app().registerHandler("/graph/info", &handleGraphInfo, {Get});
    app().registerHandler("/query", &handleQuery, {Post});
    app().registerHandler("/benchmark", &handleBenchmark, {Post});

    graphene::log::info("listening on http://0.0.0.0:", port);
    app().addListener("0.0.0.0", port).setThreadNum(0).run();
    return 0;
}
