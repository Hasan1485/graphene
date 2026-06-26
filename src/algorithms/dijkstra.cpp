#include "algorithms/dijkstra.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <queue>
#include <vector>

namespace graphene {

Result Dijkstra::solve(const Graph& g, NodeId src, NodeId dst) {
    const auto start = std::chrono::steady_clock::now();
    Result result;

    constexpr Weight kInf = std::numeric_limits<Weight>::max();
    const std::size_t n = g.numNodes();

    std::vector<Weight> dist(n, kInf);
    std::vector<NodeId> parent(n, kInvalidNode);
    std::vector<char> settled(n, 0);

    // Min-heap entries: (tentative distance, node). std::priority_queue is a
    // max-heap, so we use std::greater to order by smallest distance.
    using Entry = std::pair<Weight, NodeId>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;

    dist[src] = 0;
    pq.push({0, src});

    while (!pq.empty()) {
        const auto [d, u] = pq.top();
        pq.pop();

        // Lazy deletion: skip entries left behind by an earlier, better push.
        if (settled[u]) continue;
        settled[u] = 1;
        ++result.nodesExpanded;

        if (u == dst) break;  // dst finalized -> its distance is optimal

        for (uint32_t i = g.offsets[u]; i < g.offsets[u + 1]; ++i) {
            const NodeId v = g.targets[i];
            if (settled[v]) continue;
            const Weight nd = d + g.weights[i];
            if (nd < dist[v]) {
                dist[v] = nd;
                parent[v] = u;
                pq.push({nd, v});
            }
        }
    }

    // Reconstruct the path if dst was reached.
    if (dist[dst] != kInf) {
        result.cost = dist[dst];
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
