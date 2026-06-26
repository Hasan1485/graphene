#pragma once

#include <string>

#include "graph/graph.hpp"

namespace graphene {

// Binary (de)serialization of a CSR Graph.
//
// Format (little-endian, host byte order — built and consumed on the same
// machine class):
//
//   magic   : 4 bytes  "GRPH"
//   version : uint32
//   numNodes: uint64
//   offsets : [uint64 length][raw uint32 * length]
//   targets : [uint64 length][raw uint32 * length]
//   weights : [uint64 length][raw uint32 * length]
//   coords  : [uint64 length][raw Coord  * length]   (length 0 if no coords)
//
// This makes startup instant after the first DIMACS conversion: a multi-second
// text parse becomes a single bulk read.
class Serializer {
public:
    // Write `g` to `path`. Throws std::runtime_error on I/O failure.
    static void save(const Graph& g, const std::string& path);

    // Read a Graph from `path`. Throws std::runtime_error on I/O failure or if
    // the magic/version header does not match.
    static Graph load(const std::string& path);
};

}  // namespace graphene
