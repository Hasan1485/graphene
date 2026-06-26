#include "graph/dimacs.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "graph/builder.hpp"

namespace graphene {

namespace {

// Read an entire file into memory. DIMACS files are large but flat; a single
// buffered read plus manual scanning is far faster than stream `>>` extraction.
std::string readWholeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error("DIMACS: cannot open file: " + path);
    }
    const std::streamsize size = in.tellg();
    in.seekg(0);
    std::string buffer(static_cast<std::size_t>(size), '\0');
    in.read(buffer.data(), size);
    return buffer;
}

inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Advance `p` to the next number (first digit or leading minus sign).
inline void skipToNumber(const char*& p, const char* end) {
    while (p < end && !isDigit(*p) && *p != '-') ++p;
}

// Parse the next signed integer at or after `p` (skipping any leading
// non-numeric characters), leaving `p` just past the last digit.
inline long parseSigned(const char*& p, const char* end) {
    while (p < end && !isDigit(*p) && *p != '-') ++p;
    bool neg = false;
    if (p < end && *p == '-') {
        neg = true;
        ++p;
    }
    long value = 0;
    while (p < end && isDigit(*p)) {
        value = value * 10 + (*p - '0');
        ++p;
    }
    return neg ? -value : value;
}

// Skip to the end of the current line.
inline void skipLine(const char*& p, const char* end) {
    while (p < end && *p != '\n') ++p;
}

}  // namespace

Graph Dimacs::load(const std::string& grPath, const std::string& coPath) {
    // --- Parse the .gr file into a node count + edge list ---
    const std::string gr = readWholeFile(grPath);
    const char* p = gr.data();
    const char* end = p + gr.size();

    std::size_t numNodes = 0;
    std::vector<Edge> edges;

    while (p < end) {
        // Move to the first non-blank character of the line.
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if (p >= end) break;

        const char tag = *p;
        if (tag == 'a') {
            // "a <from> <to> <weight>" — endpoints are 1-indexed in the file.
            ++p;
            const long from = parseSigned(p, end);
            const long to = parseSigned(p, end);
            const long w = parseSigned(p, end);
            edges.push_back({static_cast<NodeId>(from - 1), static_cast<NodeId>(to - 1),
                             static_cast<Weight>(w)});
        } else if (tag == 'p') {
            // "p sp <n> <m>" — we only need the node count for sizing.
            ++p;
            skipToNumber(p, end);
            numNodes = static_cast<std::size_t>(parseSigned(p, end));
            skipToNumber(p, end);
            const long m = parseSigned(p, end);
            edges.reserve(static_cast<std::size_t>(m));
        }
        // 'c' (comment) and anything else: fall through and skip the line.
        skipLine(p, end);
    }

    if (numNodes == 0) {
        throw std::runtime_error("DIMACS: no 'p sp' problem line found in " + grPath);
    }

    // --- Optionally parse the .co file into coordinates ---
    std::vector<Coord> coords;
    if (!coPath.empty()) {
        const std::string co = readWholeFile(coPath);
        const char* q = co.data();
        const char* qend = q + co.size();

        coords.assign(numNodes, Coord{});
        while (q < qend) {
            while (q < qend && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) ++q;
            if (q >= qend) break;

            if (*q == 'v') {
                // "v <node> <longitude> <latitude>" in integer microdegrees.
                ++q;
                const long node = parseSigned(q, qend);
                const long lon = parseSigned(q, qend);
                const long lat = parseSigned(q, qend);
                const auto idx = static_cast<std::size_t>(node - 1);  // 1- -> 0-indexed
                if (idx < coords.size()) {
                    coords[idx] =
                        Coord{static_cast<double>(lat) / 1e6, static_cast<double>(lon) / 1e6};
                }
            }
            skipLine(q, qend);
        }
    }

    return buildCSR(numNodes, edges, std::move(coords));
}

}  // namespace graphene
