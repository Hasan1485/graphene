#pragma once

#include <memory>
#include <string>

#include "algorithms/algorithm.hpp"
#include "algorithms/heuristic.hpp"

namespace graphene {

// A* search. Identical to Dijkstra except the priority of a node is
// f = g + h, where g is the best-known cost from the source and h is the
// heuristic's lower-bound estimate to the destination.
//
// With an admissible & consistent heuristic the path is still optimal, but the
// search is "pulled" toward the goal and expands far fewer nodes. Using
// ZeroHeuristic makes A* behave exactly like Dijkstra (the correctness check).
class AStar : public Algorithm {
public:
    explicit AStar(std::shared_ptr<Heuristic> heuristic)
        : heuristic_(std::move(heuristic)), name_("astar-" + heuristic_->name()) {}

    std::string name() const override { return name_; }
    Result solve(const Graph& g, NodeId src, NodeId dst) override;

private:
    std::shared_ptr<Heuristic> heuristic_;
    std::string name_;
};

}  // namespace graphene
