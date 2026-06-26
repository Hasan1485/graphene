#include "benchmark/json_export.hpp"

#include <fstream>
#include <stdexcept>

namespace graphene {

nlohmann::json toJson(const Result& r) {
    return nlohmann::json{
        {"path", r.path},
        {"cost", r.cost},
        {"nodesExpanded", r.nodesExpanded},
        {"timeMs", r.timeMs},
        {"reachable", r.reachable()},
    };
}

nlohmann::json toJson(const AlgorithmSummary& s) {
    return nlohmann::json{
        {"algorithm", s.algorithm},
        {"queries", s.queries},
        {"reachable", s.reachable},
        {"meanTimeMs", s.meanTimeMs},
        {"medianTimeMs", s.medianTimeMs},
        {"p95TimeMs", s.p95TimeMs},
        {"meanNodesExpanded", s.meanNodesExpanded},
    };
}

nlohmann::json toJson(const BenchmarkReport& report) {
    nlohmann::json summaries = nlohmann::json::array();
    for (const auto& s : report.summaries) {
        summaries.push_back(toJson(s));
    }

    nlohmann::json perQuery = nlohmann::json::array();
    for (const auto& m : report.perQuery) {
        perQuery.push_back({
            {"algorithm", m.algorithm},
            {"src", m.src},
            {"dst", m.dst},
            {"reachable", m.reachable},
            {"cost", m.cost},
            {"nodesExpanded", m.nodesExpanded},
            {"timeMs", m.timeMs},
        });
    }

    return nlohmann::json{
        {"config",
         {
             {"numQueries", report.numQueries},
             {"seed", report.seed},
             {"numNodes", report.numNodes},
             {"numEdges", report.numEdges},
         }},
        {"summary", summaries},
        {"perQuery", perQuery},
    };
}

void writeReport(const BenchmarkReport& report, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    out << toJson(report).dump(2) << '\n';
    if (!out) {
        throw std::runtime_error("failed writing output file: " + path);
    }
}

}  // namespace graphene
