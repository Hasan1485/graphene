#include "algorithms/factory.hpp"

#include "algorithms/astar.hpp"
#include "algorithms/bfs.hpp"
#include "algorithms/dijkstra.hpp"
#include "algorithms/heuristic.hpp"

namespace graphene {

std::unique_ptr<Algorithm> makeAlgorithm(const std::string& name) {
    if (name == "bfs") {
        return std::make_unique<BFS>();
    }
    if (name == "dijkstra") {
        return std::make_unique<Dijkstra>();
    }
    if (name == "astar" || name == "astar-euclidean") {
        return std::make_unique<AStar>(std::make_shared<EuclideanHeuristic>());
    }
    if (name == "astar-zero") {
        return std::make_unique<AStar>(std::make_shared<ZeroHeuristic>());
    }
    return nullptr;
}

const std::vector<std::string>& defaultAlgorithmNames() {
    static const std::vector<std::string> names = {"bfs", "dijkstra", "astar-zero",
                                                   "astar-euclidean"};
    return names;
}

}  // namespace graphene
