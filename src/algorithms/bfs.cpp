#include "algorithms/bfs.hpp"

#include <algorithm>
#include <chrono>
#include <queue>
#include <vector>

namespace graphene {

Result BFS::solve(const Graph& g, NodeId src, NodeId dst) {
    const auto start = std::chrono::steady_clock::now();
    Result result;

    const std::size_t n = g.numNodes();
    std::vector<NodeId> parent(n, kInvalidNode);
    std::vector<char> visited(n, 0);

    std::queue<NodeId> frontier;
    frontier.push(src);
    visited[src] = 1;

    while (!frontier.empty()) {
        const NodeId u = frontier.front();
        frontier.pop();
        ++result.nodesExpanded;

        if (u == dst) break;  // shortest hop-path to dst found

        for (uint32_t i = g.offsets[u]; i < g.offsets[u + 1]; ++i) {
            const NodeId v = g.targets[i];
            if (!visited[v]) {
                visited[v] = 1;
                parent[v] = u;
                frontier.push(v);
            }
        }
    }

    // Reconstruct the path by walking parents back from dst (if reached).
    if (visited[dst]) {
        for (NodeId cur = dst; cur != kInvalidNode; cur = parent[cur]) {
            result.path.push_back(cur);
            if (cur == src) break;
        }
        std::reverse(result.path.begin(), result.path.end());
        result.cost = static_cast<Weight>(result.path.size() - 1);  // hop count
    }

    const auto finish = std::chrono::steady_clock::now();
    result.timeMs = std::chrono::duration<double, std::milli>(finish - start).count();
    return result;
}

}  // namespace graphene
