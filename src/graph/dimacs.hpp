#pragma once

#include <string>

#include "graph/graph.hpp"

namespace graphene {

// Loaders for the 9th DIMACS Implementation Challenge USA road-network format.
//
//   .gr (graph):    "p sp <n> <m>" then "a <from> <to> <weight>" lines.
//   .co (coords):   "p aux sp co <n>" then "v <node> <lon> <lat>" lines,
//                    coordinates given as integer microdegrees.
//
// Node ids are 1-indexed in the files and converted to 0-indexed internally.
// Each travel direction is listed as its own "a" line, so the resulting graph
// is already directed and stored as-is.
class Dimacs {
public:
    // Load a .gr file (and optionally its matching .co file) into a CSR Graph.
    // If `coPath` is empty the graph is built without coordinates.
    static Graph load(const std::string& grPath, const std::string& coPath = "");
};

}  // namespace graphene
