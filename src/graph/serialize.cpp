#include "graph/serialize.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace graphene {

namespace {

constexpr char kMagic[4] = {'G', 'R', 'P', 'H'};
constexpr uint32_t kVersion = 1;

// Write a length-prefixed blob of a trivially-copyable vector.
template <typename T>
void writeVector(std::ofstream& out, const std::vector<T>& v) {
    const uint64_t length = v.size();
    out.write(reinterpret_cast<const char*>(&length), sizeof(length));
    if (length > 0) {
        out.write(reinterpret_cast<const char*>(v.data()),
                  static_cast<std::streamsize>(length * sizeof(T)));
    }
}

// Read a length-prefixed blob into a vector.
template <typename T>
void readVector(std::ifstream& in, std::vector<T>& v) {
    uint64_t length = 0;
    in.read(reinterpret_cast<char*>(&length), sizeof(length));
    if (!in) throw std::runtime_error("serialize: unexpected EOF reading length");
    v.resize(length);
    if (length > 0) {
        in.read(reinterpret_cast<char*>(v.data()),
                static_cast<std::streamsize>(length * sizeof(T)));
        if (!in) throw std::runtime_error("serialize: unexpected EOF reading data");
    }
}

}  // namespace

void Serializer::save(const Graph& g, const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("serialize: cannot open for write: " + path);

    out.write(kMagic, sizeof(kMagic));
    out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

    const uint64_t numNodes = g.numNodes();
    out.write(reinterpret_cast<const char*>(&numNodes), sizeof(numNodes));

    writeVector(out, g.offsets);
    writeVector(out, g.targets);
    writeVector(out, g.weights);
    writeVector(out, g.coords);

    if (!out) throw std::runtime_error("serialize: write failed: " + path);
}

Graph Serializer::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("serialize: cannot open for read: " + path);

    char magic[4];
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        throw std::runtime_error("serialize: bad magic (not a graphene file): " + path);
    }

    uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!in || version != kVersion) {
        throw std::runtime_error("serialize: unsupported file version");
    }

    uint64_t numNodes = 0;
    in.read(reinterpret_cast<char*>(&numNodes), sizeof(numNodes));

    Graph g;
    readVector(in, g.offsets);
    readVector(in, g.targets);
    readVector(in, g.weights);
    readVector(in, g.coords);

    // Sanity check: offsets should hold numNodes + 1 entries (or be empty).
    if (!g.offsets.empty() && g.offsets.size() != numNodes + 1) {
        throw std::runtime_error("serialize: offsets size inconsistent with node count");
    }

    return g;
}

}  // namespace graphene
