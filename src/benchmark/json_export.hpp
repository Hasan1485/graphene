#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "algorithms/algorithm.hpp"
#include "benchmark/benchmark.hpp"

namespace graphene {

// Convert benchmark/algorithm structures into nlohmann::json. Keeping these as
// free functions (rather than scattering serialization through the modules)
// means the core library has no opinion on output format.
nlohmann::json toJson(const Result& r);
nlohmann::json toJson(const AlgorithmSummary& s);
nlohmann::json toJson(const BenchmarkReport& report);

// Serialize a report to a JSON file (pretty-printed). Throws on I/O failure.
void writeReport(const BenchmarkReport& report, const std::string& path);

}  // namespace graphene
