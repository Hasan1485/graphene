#pragma once

#include <memory>
#include <string>
#include <vector>

#include "algorithms/algorithm.hpp"

namespace graphene {

// Construct an algorithm by its string name. Recognised names:
//   "bfs", "dijkstra", "astar" (Euclidean heuristic),
//   "astar-euclidean", "astar-zero"
// Returns nullptr for an unknown name (callers turn this into a 400 / error).
std::unique_ptr<Algorithm> makeAlgorithm(const std::string& name);

// Canonical default set used by "run everything" commands.
const std::vector<std::string>& defaultAlgorithmNames();

}  // namespace graphene
