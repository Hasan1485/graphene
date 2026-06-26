#include "algorithms/heuristic.hpp"

#include <cmath>
#include <limits>

namespace graphene {

namespace {
constexpr double kEarthRadiusMeters = 6'371'000.0;
constexpr double kDegToRad = M_PI / 180.0;
}  // namespace

void EuclideanHeuristic::buildProjection(const Graph& g) {
    graph_ = &g;
    scale_ = 0.0;
    projX_.clear();
    projY_.clear();

    const std::size_t n = g.numNodes();
    if (!g.hasCoords() || n == 0) {
        return;  // no geometry -> heuristic degenerates to zero (still valid)
    }

    // Reference latitude for the equirectangular projection: the graph's mean.
    double sumLat = 0.0;
    for (const Coord& c : g.coords) sumLat += c.lat;
    const double cosRefLat = std::cos((sumLat / static_cast<double>(n)) * kDegToRad);

    // Project every node to local-plane metres once.
    projX_.resize(n);
    projY_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        projX_[i] = kEarthRadiusMeters * (g.coords[i].lon * kDegToRad) * cosRefLat;
        projY_[i] = kEarthRadiusMeters * (g.coords[i].lat * kDegToRad);
    }

    // Calibrate the scale: smallest cost-per-metre over all edges. Uses the same
    // projected distance as estimate(), which is what keeps the bound valid.
    double minRatio = std::numeric_limits<double>::infinity();
    for (NodeId u = 0; u < n; ++u) {
        for (uint32_t i = g.offsets[u]; i < g.offsets[u + 1]; ++i) {
            const NodeId v = g.targets[i];
            const double dx = projX_[u] - projX_[v];
            const double dy = projY_[u] - projY_[v];
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.0) {
                const double ratio = static_cast<double>(g.weights[i]) / dist;
                if (ratio < minRatio) minRatio = ratio;
            }
        }
    }
    if (std::isfinite(minRatio)) {
        scale_ = minRatio;
    }
}

void EuclideanHeuristic::prepare(const Graph& g, NodeId dst) {
    // Recompute the projection only when the graph instance changes; the
    // benchmark runs thousands of queries against the same graph.
    if (graph_ != &g) {
        buildProjection(g);
    }
    if (!projX_.empty()) {
        dstX_ = projX_[dst];
        dstY_ = projY_[dst];
    }
}

double EuclideanHeuristic::estimate(NodeId from) const {
    if (scale_ == 0.0 || projX_.empty()) {
        return 0.0;
    }
    const double dx = projX_[from] - dstX_;
    const double dy = projY_[from] - dstY_;
    return scale_ * std::sqrt(dx * dx + dy * dy);
}

}  // namespace graphene
