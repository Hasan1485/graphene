#include "algorithms/astar.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace graphene {

Result AStar::solve(const Graph& g, NodeId src, NodeId dst) {
    const auto start = std::chrono::steady_clock::now();
    Result result;

    constexpr Weight kInf = std::numeric_limits<Weight>::max();
    const std::size_t n = g.numNodes();

    // Bind the heuristic to this query's graph and destination.
    heuristic_->prepare(g, dst);

    std::vector<Weight> g_cost(n, kInf);  // best-known cost from src (the "g")
    std::vector<NodeId> parent(n, kInvalidNode);
    std::vector<char> settled(n, 0);

    // Heap entries: (f = g + h, node). f is a double because h is real-valued.
    using Entry = std::pair<double, NodeId>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;

    g_cost[src] = 0;
    pq.push({heuristic_->estimate(src), src});

    while (!pq.empty()) {
        const NodeId u = pq.top().second;
        pq.pop();

        // Lazy deletion: a node may sit in the heap multiple times.
        if (settled[u]) continue;
        settled[u] = 1;
        ++result.nodesExpanded;

        // Consistent heuristic => the first pop of dst is optimal.
        if (u == dst) break;

        const Weight gu = g_cost[u];
        for (uint32_t i = g.offsets[u]; i < g.offsets[u + 1]; ++i) {
            const NodeId v = g.targets[i];
            if (settled[v]) continue;
            const Weight nd = gu + g.weights[i];
            if (nd < g_cost[v]) {
                g_cost[v] = nd;
                parent[v] = u;
                pq.push({static_cast<double>(nd) + heuristic_->estimate(v), v});
            }
        }
    }

    // Reconstruct the path if dst was reached.
    if (g_cost[dst] != kInf) {
        result.cost = g_cost[dst];
        for (NodeId cur = dst; cur != kInvalidNode; cur = parent[cur]) {
            result.path.push_back(cur);
            if (cur == src) break;
        }
        std::reverse(result.path.begin(), result.path.end());
    }

    const auto finish = std::chrono::steady_clock::now();
    result.timeMs = std::chrono::duration<double, std::milli>(finish - start).count();
    return result;
}

}  // namespace graphene
