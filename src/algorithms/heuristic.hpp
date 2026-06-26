#pragma once

#include <string>
#include <vector>

#include "graph/graph.hpp"

namespace graphene {

// A* heuristic: a lower-bound estimate of the remaining cost from a node to the
// fixed destination of the current query.
//
// `prepare` is called once per query to bind the graph and destination; it is
// where any per-graph precomputation (e.g. unit calibration) happens. `estimate`
// is then called many times in the hot loop and must be cheap.
class Heuristic {
public:
    virtual ~Heuristic() = default;
    virtual std::string name() const = 0;
    virtual void prepare(const Graph& g, NodeId dst) = 0;
    virtual double estimate(NodeId from) const = 0;
};

// Always returns 0. A* with this heuristic must reproduce Dijkstra exactly —
// the key correctness check of Phase 2.
class ZeroHeuristic : public Heuristic {
public:
    std::string name() const override { return "zero"; }
    void prepare(const Graph&, NodeId) override {}
    double estimate(NodeId) const override { return 0.0; }
};

// Straight-line distance to the destination, scaled into the same units as the
// edge weights.
//
// Geometry: lat/lon are projected once (per graph) onto a local plane in metres
// using an equirectangular projection about the graph's mean latitude. At
// city/state scale this is within a fraction of a percent of the true
// great-circle distance, and crucially it makes `estimate` a couple of
// multiplies and a sqrt — no trigonometry in the hot loop.
//
// Admissibility: a heuristic must never overestimate. We auto-calibrate a scale
// factor `s = min over edges (weight / planarDistance(u, v))` — the smallest
// cost-per-metre anywhere in the graph (i.e. the fastest edge). Then
//
//     h(n) = s * planarDistance(n, dst)
//
// is guaranteed <= the true remaining cost, and because Euclidean distance in
// the projected plane obeys the triangle inequality the heuristic is also
// *consistent*, so A* may stop the first time it pops the destination. The same
// formula works for real road networks (weights = travel time or distance) and
// for the synthetic generators.
class EuclideanHeuristic : public Heuristic {
public:
    std::string name() const override { return "euclidean"; }
    void prepare(const Graph& g, NodeId dst) override;
    double estimate(NodeId from) const override;

private:
    // Project every node onto the local plane and calibrate `scale_`. Cached per
    // graph so the benchmark's thousands of queries share one O(n) precompute.
    void buildProjection(const Graph& g);

    const Graph* graph_ = nullptr;  // graph the projection/scale was built for
    std::vector<double> projX_;     // projected x (metres), per node
    std::vector<double> projY_;     // projected y (metres), per node
    double scale_ = 0.0;            // weight units per metre (auto-calibrated)
    double dstX_ = 0.0;             // destination projection for this query
    double dstY_ = 0.0;
};

}  // namespace graphene
