# Graphene — Interview Preparation Document

> C++20 Shortest-Path Routing Engine — Fast, demonstrably correct, built over real road networks.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Architecture & Folder Structure](#architecture--folder-structure)
3. [C++ Concepts & Tooling Learned](#c-concepts--tooling-learned)
4. [OOP Principles Used (with Code Examples)](#oop-principles-used-with-code-examples)
5. [Module-by-Module Deep Dive](#module-by-module-deep-dive)
6. [Benchmarking: Methodology & Results](#benchmarking-methodology--results)
7. [Common Interview Questions & Answers](#common-interview-questions--answers)
8. [Potential Improvements](#potential-improvements)
9. [Quick Revision Cheatsheet](#quick-revision-cheatsheet)

---

## Project Overview

**Graphene** finds the shortest path between two intersections (nodes) on a road network (graph). It supports three routing algorithms, a command-line interface (CLI), and an HTTP API server. Written in modern C++20.

### Key Features

- **CSR Graph** — Compressed Sparse Row format: 3 flat arrays, cache-friendly, trivially serializable.
- **Three algorithms**: BFS (hop-count baseline), Dijkstra (binary heap with lazy deletion), A\* (heuristic-guided).
- **Self-calibrating A\* heuristic** — straight-line distance (equirectangular projection to a local metric plane) scaled by the graph's fastest edge. Automatically admissible on any graph.
- **Binary caching** — first DIMACS text load is cached to `.bin` for instant restarts.
- **Benchmark harness** — runs all algorithms over the same deterministic query set, exports per-query + aggregate stats (mean/median/p95) as JSON.
- **HTTP API** built with Drogon — three endpoints: graph info, single query, and benchmark.

### Tech Stack

| Technology | Purpose |
|---|---|
| C++20 | Language standard |
| CMake | Build system generator |
| vcpkg | Package manager (fetches drogon, nlohmann-json) |
| Drogon | C++ HTTP web framework |
| nlohmann/json | JSON parsing & serialization |
| clang-format | Code formatting (like Prettier for C++) |
| Ninja | Build executor |

---

## Architecture & Folder Structure

```
graphene/
├── src/
│   ├── graph/           ← Core data structure (THE MAP)
│   │   ├── graph.hpp         Graph class (3 CSR arrays + coords + helpers)
│   │   ├── builder.hpp/cpp   buildCSR() — converts edge list → CSR format (3-pass algorithm)
│   │   ├── generator.hpp/cpp Creates synthetic graphs (grid, random sparse)
│   │   ├── dimacs.hpp/cpp    Parses DIMACS road network text files
│   │   └── serialize.hpp/cpp Binary .bin save/load (instant startup)
│   ├── algorithms/      ← Routing logic (THE BRAIN)
│   │   ├── algorithm.hpp     Algorithm base class (interface) + Result struct
│   │   ├── bfs.hpp/cpp       BFS — fewest hops (queue-based)
│   │   ├── dijkstra.hpp/cpp  Dijkstra — shortest weighted path (min-heap + lazy deletion)
│   │   ├── astar.hpp/cpp     A* — heuristic-guided search (takes a Heuristic via constructor)
│   │   ├── heuristic.hpp/cpp ZeroHeuristic (for testing), EuclideanHeuristic (for real data)
│   │   └── factory.hpp/cpp   makeAlgorithm("dijkstra") — string → Algorithm object
│   ├── benchmark/       ← Performance measurement
│   │   ├── benchmark.hpp/cpp Query generation, runner, aggregate stats
│   │   └── json_export.hpp/cpp Converts results → JSON, writes to file
│   ├── api/main.cpp     ← HTTP server entry point
│   ├── cli/main.cpp     ← Command-line entry point
│   └── util/log.hpp     ← Logging helpers
├── tests/test_graph.cpp  ← Correctness tests (ctest)
├── data/                 ← Graph files + benchmark results (gitignored)
├── build/                ← Compiled binaries (gitignored)
├── vcpkg/                ← C++ package manager clone
├── vcpkg_installed/      ← Installed third-party libraries
├── CMakeLists.txt        ← Build recipe
└── vcpkg.json            ← Dependency manifest ("drogon", "nlohmann-json")
```

### High-Level Data Flow

```
┌─────────────┐     ┌─────────────┐
│  CLI tool   │     │  HTTP API   │
│  (terminal) │     │  (server)   │
└──────┬──────┘     └──────┬──────┘
       │                   │
       └────────┬──────────┘
                │
       ┌────────▼────────┐
       │  Core Library   │
       │  (graphene_core)│
       │                 │
       │  graph/   ──→ algorithms/ ──→ benchmark/
       │  (data)       (logic)        (metrics)
       └────────────────┘
```

Both the CLI and API share the exact same compiled static library (`build/libgraphene_core.a`). Zero logic duplication.

### Graph Creation Flow

```
Path 1 (real data → binary cache):
    DIMACS .gr text file
         ↓
    Dimacs::load() — parses text → builds edge list + coords
         ↓
    buildCSR() — edge list → CSR arrays
         ↓
    Graph object (in memory)
         ↓
    Serializer::save("graph.bin") — dumps raw arrays to disk

Path 2 (synthetic testing):
    Generator::gridGraph(rows, cols) or randomSparse()
         ↓
    buildCSR() — edge list → CSR arrays
         ↓
    Graph object

Path 3 (fast restart):
    graph.bin file on disk
         ↓
    Serializer::load("graph.bin") — bulk read, instant
         ↓
    Graph object (bypasses builder entirely!)
```

### Query Execution Flow

```
1. Load Graph (once per CLI command, once at API startup)
2. User specifies: source node, destination node, algorithm name(s)
3. makeAlgorithm("dijkstra") → unique_ptr<Algorithm> pointing to a Dijkstra object
4. algo->solve(graph, src, dst) → reads CSR arrays, runs search, returns Result
5. Result contains: path (vector of NodeId), cost (total weight),
   nodesExpanded (efficiency metric), timeMs (wall clock)
6. CLI prints it / API sends it as JSON
```

---

## C++ Concepts & Tooling Learned

### Build System

| Tool | What it does | Simple Analogy |
|---|---|---|
| **CMake** | Reads `CMakeLists.txt` recipe, auto-generates build files (Ninja, Make, Xcode, VS) | Blueprint → Construction plan |
| **vcpkg** | Downloads & builds third-party C++ libraries | App Store for C++ code |
| **Ninja** | The actual compiler driver (compiles .cpp → .o → executable) | Construction workers |
| **compile_commands.json** | Auto-generated file that tells your editor exact compiler flags for autocomplete/error highlighting | GPS coordinates for the editor |

### C++ Syntax & Concepts

| Concept | Explanation | Where in Graphene |
|---|---|---|
| `.hpp` vs `.cpp` | `.hpp` = declarations (what exists). `.cpp` = implementations (how it works) | `graph.hpp` declares Graph; `builder.cpp` implements `buildCSR()` |
| `#pragma once` | Include guard — prevents header from being compiled twice if included from multiple places | Every `.hpp` file |
| `#include <...>` vs `"..."` | `<>` = system/external headers. `""` = project's own headers | `#include <vector>` vs `#include "graph/graph.hpp"` |
| `namespace` | Groups code under a name to prevent name clashes | `namespace graphene { ... }` wraps every file |
| `using Alias = Type;` | Creates a nickname for an existing type | `using NodeId = uint32_t;` |
| `constexpr` | Value computed at compile time (not runtime) | `constexpr NodeId kInvalidNode = static_cast<NodeId>(-1);` |
| `struct` vs `class` | Struct: members public by default. Class: private by default | `struct Coord { ... };` vs `class Graph { public: ... };` |
| `static_cast<T>(val)` | Explicit, safe type conversion | `static_cast<NodeId>(from - 1)` |
| `std::size_t` | Unsigned integer type for sizes (returned by `sizeof`) | `numNodes()` returns `std::size_t` |
| `uint32_t / uint64_t` | Fixed-width unsigned integers (guaranteed exact bit width) | `NodeId = uint32_t`, `Weight = uint32_t` |
| `std::vector<T>` | Dynamic resizable array, stored on heap | All 4 graph arrays |
| `std::unique_ptr<T>` | Smart pointer — owns the object, auto-deletes when out of scope | `makeAlgorithm()` returns `unique_ptr<Algorithm>` |
| `std::shared_ptr<T>` | Smart pointer — shared ownership with reference counting | `AStar` holds `shared_ptr<Heuristic>` |
| `make_unique<T>(args)` | Creates object on heap, returns `unique_ptr` (RAII friendly) | `make_unique<Dijkstra>()` |
| `auto` | Type deduction — compiler figures out the type | `auto algo = makeAlgorithm(name);` |
| `virtual` + `= 0` | "Pure virtual" = abstract method (child classes MUST override) | `virtual Result solve(...) = 0;` |
| `override` | Explicit marker: "I am intentionally overriding a parent method" | `std::string name() const override;` |
| `const` after function | Promise: "this method will NOT modify the object's state" | `std::size_t numNodes() const;` |
| `std::move()` | Transfers ownership without copying (efficiency) | `g.coords = std::move(coords);` |
| `try-catch` | Exception handling | CLI/API `main()` wrap graph loading in try-catch |
| Template functions | Write once, works with any type T | `writeVector<T>()` in `serialize.cpp` |
| Lambda expressions | Inline anonymous function | `auto id = [cols](r, c) { return r * cols + c; };` in generator.cpp |
| Anonymous namespace | Everything inside is file-private (replaces `static` in C) | `namespace { ... }` in cli/main.cpp and api/main.cpp |

---

## OOP Principles Used (with Code Examples)

### 1. Inheritance — "is-a" relationship

**Definition**: A child class inherits properties and methods from a parent. Child IS-A Parent.

```cpp
// PARENT — algorithm.hpp
class Algorithm {
public:
    virtual ~Algorithm() = default;
    virtual std::string name() const = 0;      // pure virtual
    virtual Result solve(const Graph& g, NodeId src, NodeId dst) = 0;
};

// CHILDREN
class BFS      : public Algorithm { ... };  // BFS IS-AN Algorithm
class Dijkstra : public Algorithm { ... };  // Dijkstra IS-AN Algorithm
class AStar    : public Algorithm { ... };  // AStar IS-AN Algorithm
// heuristic.hpp:
class ZeroHeuristic      : public Heuristic { ... };
class EuclideanHeuristic : public Heuristic { ... };
```

**Where**: `src/algorithms/` — BFS, Dijkstra, AStar all inherit from Algorithm. Heuristics inherit from Heuristic.

---

### 2. Polymorphism — same call, different behavior

**Definition**: Calling the same function name on different objects gets different behavior — resolved at runtime via virtual dispatch.

```cpp
// The caller (cli, api, benchmark) doesn't know or care what concrete type algo points to:
std::unique_ptr<Algorithm> algo = makeAlgorithm("dijkstra");
Result r = algo->solve(g, src, dst);   // calls Dijkstra::solve()

algo = makeAlgorithm("bfs");
r = algo->solve(g, src, dst);          // exact same line, now calls BFS::solve()

algo = makeAlgorithm("astar-euclidean");
r = algo->solve(g, src, dst);          // calls AStar::solve() with Euclidean heuristic
```

The `virtual` keyword makes this possible. Without it, C++ would always call `Algorithm::solve()` (which is `= 0`, an error).

**Where**: Every call site in CLI, API, and benchmark — `algo->solve()` works polymorphically.

---

### 3. Abstraction — hide complexity behind a simple interface

**Definition**: Users of a class interact with a simple interface and don't need to know internal implementation details.

```cpp
// The user sees only THIS:
auto algo = makeAlgorithm("dijkstra");
Result r = algo->solve(g, src, dst);
// Done. They have NO idea about:
//   - Binary heap with std::priority_queue
//   - Lazy deletion (pushing duplicates, skipping stale pops)
//   - Distance arrays initialized to kInf
//   - Parent pointer reconstruction from dst back to src
//   - Timer using std::chrono::steady_clock
```

Same with `Graph` helpers:

```cpp
int n = g.numNodes();      // abstracted: hides offsets.empty() ? 0 : offsets.size() - 1
int d = g.degree(u);       // abstracted: hides offsets[u+1] - offsets[u]
```

**Where**: `algorithm.hpp` (the base class), `graph.hpp` (the helper methods).

---

### 4. Composition — "has-a" relationship

**Definition**: A class contains another object as a member. HAS-A instead of IS-A.

```cpp
// astar.hpp
class AStar : public Algorithm {            // AStar IS-AN Algorithm (inheritance)
private:
    std::shared_ptr<Heuristic> heuristic_;  // AStar HAS-A Heuristic (composition)
    std::string name_;
};
```

Unlike inheritance, the heuristic can be swapped at runtime without changing `AStar`'s code at all.

**Where**: `astar.hpp` — AStar contains a Heuristic.

---

### 5. Dependency Injection — receive dependencies, don't create them

**Definition**: Instead of a class creating its own dependencies internally, they are passed in from outside. Makes the class flexible and testable.

```cpp
// astar.hpp — constructor: the injected Heuristic
explicit AStar(std::shared_ptr<Heuristic> heuristic)
    : heuristic_(std::move(heuristic)), name_("astar-" + heuristic_->name()) {}
    //                                    ^^^ injected, not created inside AStar

// factory.cpp — the caller chooses what to inject:
make_unique<AStar>(make_shared<EuclideanHeuristic>());    // → real A*
make_unique<AStar>(make_shared<ZeroHeuristic>());         // → A* that behaves like Dijkstra
```

**Benefit**: To add a new heuristic (e.g., ManhattanDistance), you create the class and pass it to AStar's constructor. AStar itself never changes.

**Where**: `astar.hpp` (constructor), `factory.cpp` (injection site).

---

### 6. Factory Pattern — centralized object creation

**Definition**: A function that creates and returns objects of different types based on a string/int/enum, hiding construction details in one place.

```cpp
// factory.cpp — THE ONLY PLACE that knows how to build all algorithm types
std::unique_ptr<Algorithm> makeAlgorithm(const std::string& name) {
    if (name == "bfs")                              return make_unique<BFS>();
    if (name == "dijkstra")                         return make_unique<Dijkstra>();
    if (name == "astar" || name == "astar-euclidean") return make_unique<AStar>(make_shared<EuclideanHeuristic>());
    if (name == "astar-zero")                       return make_unique<AStar>(make_shared<ZeroHeuristic>());
    return nullptr;  // unknown name
}
```

**Without factory** — the if-else chain would be duplicated in: `cli/main.cpp`, `api/main.cpp`, `benchmark.cpp`, AND `resolveAlgorithms()` for validation.

**With factory** — one place. Add a new algorithm? Change ONE file.

**Where**: `factory.hpp/cpp`.

---

### 7. Static Methods / Utility Classes — no state, no object

**Definition**: Methods that belong to the class itself, not to any instance. Called as `ClassName::method()` without creating an object.

```cpp
// These classes have NO state — they're organizational groupings of free functions:
class Dimacs {
public:
    static Graph load(const std::string& grPath, const std::string& coPath = "");
};

class Generator {
public:
    static Graph gridGraph(uint32_t rows, uint32_t cols, Weight maxWeight = 1, uint64_t seed = 42);
    static Graph randomSparse(uint32_t n, uint32_t avgDegree, uint64_t seed = 42);
};

class Serializer {
public:
    static void save(const Graph& g, const std::string& path);
    static Graph load(const std::string& path);
};

// Usage — no object creation needed:
Graph g = Dimacs::load("NY.gr");
Graph g2 = Generator::gridGraph(100, 100);
Serializer::save(g, "output.bin");
```

**Vs. regular classes**: `BFS`, `Dijkstra`, `AStar` have internal state (the `heuristic_` pointer) → need actual objects. `Dimacs`, `Generator`, `Serializer` have no memory of past operations → static methods suffice.

**Where**: `dimacs.hpp`, `generator.hpp`, `serialize.hpp`.

---

### 8. Strategy Pattern — interchangeable algorithms

**Definition**: Define a family of algorithms, encapsulate each one, and make them interchangeable. Strategy lets the algorithm vary independently from clients that use it.

```cpp
// The "Strategy" interface:
class Heuristic {
    virtual void prepare(const Graph& g, NodeId dst) = 0;
    virtual double estimate(NodeId from) const = 0;
};

// Concrete strategies:
class ZeroHeuristic : public Heuristic { ... };      // always returns 0
class EuclideanHeuristic : public Heuristic { ... }; // uses lat/lon geometry

// The "Context" that uses the strategy:
class AStar : public Algorithm {
    shared_ptr<Heuristic> heuristic_;  // ← the current strategy
    Result solve(...) {
        heuristic_->prepare(g, dst);   // ← delegates to strategy
        heuristic_->estimate(v);       // ← delegates to strategy
    }
};
```

**Where**: `heuristic.hpp/cpp` (strategy interface + implementations), `astar.hpp/cpp` (context).

---

## Module-by-Module Deep Dive

### src/graph/ — The Data Structure (The Map)

#### CSR Format (Compressed Sparse Row)

The graph is stored as three flat arrays (NOT a vector of node objects with edge lists):

```
offsets = [0, 2, 3, 3, 4]     // size = numNodes + 1 (start position of each node's edges)
targets = [1, 3, 2, 1]        // size = numEdges (which node each edge goes TO)
weights = [1, 10, 1, 5]       // size = numEdges (cost of traversing each edge)

coords  = [(lat,lon), ...]     // size = numNodes (or empty if none)

For node u, iterate its outgoing edges like this:
    for (uint32_t i = offsets[u]; i < offsets[u + 1]; ++i) {
        NodeId neighbor = targets[i];
        Weight  cost     = weights[i];
    }
```

**Why CSR over vector-of-vectors?**
- Cache-friendly — all edges are contiguous in memory (CPU cache loves sequential access)
- Compact — no pointer overhead per edge (one pointer per edge in vector-of-vectors = 8 bytes wasted)
- Serializable — can dump all three arrays to disk with one `fwrite` call
- Real routing engines (OSRM, GraphHopper) use CSR or its variant

#### The `buildCSR()` 3-Pass Algorithm (O(n+m))

Takes a flat list of `Edge` structs + node count, produces the CSR arrays:

| Pass | What it does | Code detail |
|---|---|---|
| **1. Count** | Count out-degree: `++offsets[from + 1]` for each edge. Stores counts one slot to the right. | Purpose: so the prefix sum naturally lands start positions at `offsets[u]` |
| **2. Prefix sum** | `offsets[i] += offsets[i-1]` — converts "counts" into "start positions" | After this, `offsets[u]` = where node u's edges begin in targets/weights |
| **3. Scatter** | For each edge: `pos = cursor[from]++`, then `targets[pos] = to`, `weights[pos] = weight` | Uses a cursor copy so the original offsets remains as the permanent start positions |

**Time Complexity**: O(n + m) — each node touched once in prefix sum, each edge touched twice (count + scatter).

#### The DIMACS Format

Standard road network format from the 9th DIMACS Implementation Challenge (academic research competition). USA road networks with real weights AND geographic coordinates.

- `.gr` file format: header `"p sp <numNodes> <numEdges>"`, then `"a <from> <to> <weight>"` lines
- `.co` file format: `"v <nodeId> <longitude> <latitude>"` with coordinates in microdegrees (integers ÷ 1,000,000)
- Node IDs are 1-indexed in the files → converted to 0-indexed internally (`from - 1`)
- Each travel direction appears as its own "a" line → graph is already directed

#### Binary Caching (Serializer)

DIMACS text parsing takes multiple seconds. Solution: after first load, dump raw CSR arrays to a `.bin` file.

**.bin file format**:
```
[magic: "GRPH" 4 bytes][version: uint32][numNodes: uint64]
[offsets: length(8 bytes) + raw uint32 array]
[targets: length(8 bytes) + raw uint32 array]
[weights: length(8 bytes) + raw uint32 array]
[coords:  length(8 bytes) + raw Coord struct array]
```

Loading a `.bin` is a single bulk read — bypasses text parsing AND CSR construction entirely. Hundreds of times faster.

---

### src/algorithms/ — The Routing Logic (The Brain)

#### BFS (Breadth-First Search)

| Aspect | Detail |
|---|---|
| **What it finds** | Path with fewest hops (ignores edge weights entirely) |
| **Data structure** | `std::queue` (FIFO) |
| **State arrays** | `visited[]` (bool) + `parent[]` (NodeId) |
| **Termination condition** | When `dst` is first popped from the queue |
| **Path reconstruction** | Walk `parent[]` from dst back to src, then reverse |
| **Cost definition** | Number of edges in the path (`path.size() - 1`) |
| **Complexity** | O(V + E) time, O(V) space |

#### Dijkstra's Algorithm

| Aspect | Detail |
|---|---|
| **What it finds** | Shortest weighted path (guaranteed optimal) |
| **Data structure** | `std::priority_queue` (min-heap via `std::greater`) |
| **State arrays** | `dist[]` (tentative distances, initialized to INF), `parent[]`, `settled[]` (finalized) |
| **Lazy deletion** | Instead of `decrease-key` (complex heap operation), push duplicate (node, better-dist) entries. On pop, `if (settled[u]) continue;` skips stale entries |
| **Relaxation** | For each neighbor v: `if (dist[u] + weight < dist[v]) { update dist[v], parent[v] = u, push(v) }` |
| **Invariant** | The first time a node is popped from the heap, its distance is final (optimal) |
| **Termination** | When `dst` is first popped — its distance is optimal |
| **Complexity** | O((V + E) log V) time, O(V) space |

#### A* Search

| Aspect | Detail |
|---|---|
| **What it finds** | Shortest weighted path (same optimal result as Dijkstra) |
| **Dijkstra difference** | Priority uses `f = g + h` instead of just `g` |
| **g-value** | Best-known cost from source (identical to Dijkstra's `dist[]`) |
| **h-value** | Heuristic estimate from current node to destination |
| **Effect** | The heuristic "pulls" the search toward dst — expands fewer nodes |
| **Requirement for optimality** | Heuristic must be ADMISSIBLE (never overestimate) AND CONSISTENT (obeys triangle inequality) |
| **Correctness gate** | A* with ZeroHeuristic (h = 0) must produce IDENTICAL results to Dijkstra |

#### Heuristic Design — The Clever Part

```cpp
// EuclideanHeuristic auto-calibration (heuristic.cpp):

// STEP 1: Project all lat/lon → local plane in metres (once per graph, cached)
// Uses equirectangular projection about mean latitude:
//   x = earth_radius * lon_radians * cos(mean_lat_radians)
//   y = earth_radius * lat_radians

// STEP 2: Find the "fastest edge" — minimum cost per metre anywhere in the graph
scale = min( weight / straightLineDistance(u, v) ) across ALL edges

// STEP 3: For any node n:
estimate(n) = scale * straightLineDistance(n, dst)
```

**Why this guarantees admissibility:**
- `scale` is the minimum cost-per-metre — the fastest possible travel rate
- `scale * straightLineDistance` uses this fastest rate everywhere
- Real paths use slower edges too → true cost ≥ `scale * straightLineDistance`
- Therefore: `h(n)` always UNDERESTIMATES → admissible → optimal paths guaranteed

**Bonus**: `scale` is computed once and cached per graph. The benchmark's 1000 queries pay the O(E) calibration cost only on the first query.

---

### src/benchmark/ — Performance Measurement

#### What It Measures

For each algorithm, across N random (src, dst) query pairs:

| Metric | What it means | Why it matters |
|---|---|---|
| `meanTimeMs` | Average wall-clock time per query | Overall throughput |
| `medianTimeMs` | 50th percentile — middle value | Less sensitive to outlier long paths |
| `p95TimeMs` | 95th percentile — worst-case typical | Realistic worst-case performance |
| `meanNodesExpanded` | Average nodes settled per query | Machine-independent efficiency metric |

#### Benchmark Flow

```
1. generateQueries(numNodes, count, seed)
   → deterministic random (src, dst) pairs (same seed = same queries = reproducible)

2. For each algorithm:
       For each query:
           algo->solve(graph, query.src, query.dst)
           Record { algorithm, src, dst, cost, nodesExpanded, timeMs, reachable }

3. Compute aggregates for each algorithm:
       meanTimeMs, medianTimeMs(sort + middle), p95TimeMs(sort + 95th percentile)
       meanNodesExpanded

4. Output:
       writeReport(report, "out.json") → JSON file
       Formatted table → stdout (CLI) or JSON response (API)
```

#### How `nodesExpanded` Is Counted

Inside each algorithm, `++result.nodesExpanded` executes when a node is **expanded** — i.e. popped from the frontier and processed. In Dijkstra/A* that pop also *settles* the node (its shortest distance is now final, guarded by `if (settled[u]) continue;`); in BFS the node is simply dequeued (BFS marks nodes visited on enqueue, so each is dequeued exactly once). Either way, it counts nodes the search actually processed — not nodes merely *discovered* and still sitting in the frontier.

---

### CLI vs API — Two Front Doors, Same Engine

| Aspect | CLI (`graphene_cli`) | API (`graphene_api`) |
|---|---|---|
| **Entry point** | `src/cli/main.cpp` | `src/api/main.cpp` |
| **Input** | Command-line arguments | HTTP JSON body |
| **Output** | Terminal text | HTTP JSON response |
| **Graph loading** | Loaded fresh for each command | Loaded ONCE at startup, shared global |
| **Available commands** | 6: gen-grid, gen-random, convert, info, query, bench | 3: GET /graph/info, POST /query, POST /benchmark |
| **Error handling** | Prints to stderr, returns exit code 1 | Returns HTTP 400 with `{"error": "..."}` |
| **Uses same core?** | Yes — same `makeAlgorithm()`, `solve()` | Yes — identical calls |
| **Default algorithms** | All 4 (bench/query if none specified) | Dijkstra+A* (query), All 4 (benchmark) |

#### API Endpoint Details

| Endpoint | Method | Body | Response | Defaults |
|---|---|---|---|---|
| `/graph/info` | GET | None | `{nodes, edges, avgDegree}` | N/A |
| `/query` | POST | `{src, dst, algorithms?}` | Array of per-algorithm Result objects | `algorithms = ["dijkstra", "astar-euclidean"]` |
| `/benchmark` | POST | `{numQueries?, seed?, algorithms?}` | Full BenchmarkReport (config, perQuery, summaries) | `numQueries=100, seed=42, algorithms=all4` |

#### API Input Validation (Fail-Fast Pattern)

Every handler first validates ALL inputs before doing any work:

1. **Parse JSON body** → 400 if invalid
2. **Check required fields** (src/dst) → 400 if missing
3. **Validate ranges** (src/dst must be ≥ 0 and < numNodes) → 400 if out of bounds
4. **Validate algorithm names** → 400 if unknown (calls `makeAlgorithm()` to check)

Only when ALL checks pass → run the algorithms → send results.

---

## Benchmarking: Methodology & Results

### How to Run Benchmarks

```bash
# Synthetic (no download):
./graphene_cli gen-grid 1000 1000 data/grid.bin 10   # 1M-node grid, weights 1-10
./graphene_cli bench data/grid.bin 1000 42 data/bench.json

# Real road network (New York):
# Download BOTH the graph (.gr) and the coordinates (.co). The .co file is
# REQUIRED for the A*-Euclidean heuristic — without coordinates it silently
# degenerates to a zero heuristic and behaves exactly like Dijkstra.
curl -O http://www.diag.uniroma1.it/~challenge9/data/USA-road-d/USA-road-d.NY.gr.gz
curl -O http://www.diag.uniroma1.it/~challenge9/data/USA-road-d/USA-road-d.NY.co.gz
gunzip USA-road-d.NY.gr.gz USA-road-d.NY.co.gz
./graphene_cli convert USA-road-d.NY.gr USA-road-d.NY.co data/NY.bin
./graphene_cli bench data/NY.bin 1000 42 data/NY-bench.json
```

> **Note:** Passing `-` instead of the `.co` file skips coordinate loading. The
> graph still works, but A*-Euclidean loses its heuristic and matches Dijkstra's
> node-expansion count — so the speedup below would disappear.

### Benchmark Results: New York Road Network

**Graph**: 264,346 nodes, 733,846 edges, Apple Silicon, Release build, 1000 random queries (seed=42).

| Algorithm | Mean (ms) | Median (ms) | P95 (ms) | Mean Nodes Expanded |
|---|---|---|---|---|
| BFS | 2.41 | 2.50 | 4.55 | 134,746 |
| Dijkstra | 8.83 | 9.03 | 16.48 | 134,708 |
| A*-Zero | 10.59 | 10.71 | 19.90 | 134,708 |
| **A*-Euclidean** | **6.07** | **4.91** | **15.91** | **62,345** |

### Reading the Numbers

1. **A*-zero expands the exact same node count as Dijkstra** (both 134,708) — this is the CORRECTNESS GATE. A* with a zero heuristic IS Dijkstra. If they differed, there would be a bug.

2. **A*-Euclidean expands ~2.2× fewer nodes** than Dijkstra. The heuristic pulls the frontier toward the destination, reducing the search space by over half.

3. **BFS is fastest in wall-clock time** but solves a DIFFERENT problem. BFS ignores weights and finds the path with the fewest road segments — not the shortest by distance/time. Its `cost` is NOT comparable to weighted algorithms.

4. **A*-zero is slightly slower than Dijkstra** despite expanding the same node count. The extra overhead: every queue push/pop computes `f = g + 0.0`, and `f` is a `double` vs `Weight` (integer). Pure Dijkstra compares integers, which is marginally faster.

5. **P95 latencies are ~2× the mean** — a small number of long-distance queries dominate the tail.

### Machine-Independent Metric: `nodesExpanded`

This is the PURE algorithmic metric. It doesn't depend on CPU speed, background load, or cache warmth. A* consistently shows ~54% fewer nodes expanded than Dijkstra — measuring how much the heuristic prunes the search space.

---

## Common Interview Questions & Answers

### Q1: "Explain the project at a high level."

> It's a shortest-path routing engine in C++20. You give it a road network graph and two points; it finds the optimal path using one of three algorithms. The graph uses CSR format (cache-friendly flat arrays), the A* heuristic self-calibrates from the graph data so it works without manual tuning, and there's both a CLI tool (for scripting) and an HTTP API (for serving requests). A benchmark harness compares algorithm performance by running identical query sets and exporting per-query metrics + aggregates as JSON.

### Q2: "Why is the graph stored in CSR format?"

> CSR stores a graph as three flat arrays instead of objects with pointer-based edge lists. This means: (1) all edges of a node sit contiguously in memory — the CPU cache can prefetch them, (2) no memory wasted on pointers per edge, (3) the entire graph can be serialized to disk with a single write call and loaded back with a single read. Real production routing engines (OSRM, GraphHopper) use CSR.

### Q3: "Explain the difference between BFS, Dijkstra, and A*."

| | BFS | Dijkstra | A* |
|---|---|---|---|
| **Frontier** | Queue (FIFO) | Min-heap (by distance) | Min-heap (by distance + heuristic) |
| **Optimal for** | Fewest hops | Shortest weighted path | Shortest weighted path |
| **Uses edge weights?** | No | Yes | Yes |
| **Expands nodes** | All directions equally | All directions, cheapest first | Biased toward destination |
| **Termination** | First pop of dst | First pop of dst | First pop of dst (with consistent heuristic) |

### Q4: "How does Dijkstra guarantee the shortest path?"

> Dijkstra maintains the invariant: when a node is first popped from the priority queue, its distance is the absolute minimum possible from the source. This is because all edge weights are non-negative, so any path through an unprocessed node would be at least as long as the current best. The algorithm terminates as soon as the destination is popped — by invariant, no better path can exist.

### Q5: "Explain lazy deletion in Dijkstra."

> Instead of implementing `decrease_key` on the priority queue (which requires finding and updating an existing entry — complex in C++'s std::priority_queue), we push a duplicate entry when a better distance is found. On pop, if the node is already settled (marked finalized), we skip it. This is simpler code and practically as fast for sparse graphs.

### Q6: "How does A* guarantee optimality?"

> The heuristic must be ADMISSIBLE — it must never OVERESTIMATE the remaining cost to the destination. Our Euclidean heuristic self-calibrates a scale factor `s = min(weight / distance)` over all edges (the fastest cost-per-metre). Since `h(n) = s * straightLineDistance(n, dst)` uses the fastest possible rate, it's guaranteed ≤ true remaining cost. Additionally, since Euclidean distance obeys the triangle inequality on the projected plane, the heuristic is also CONSISTENT — allowing A* to stop the first time it pops dst.

### Q7: "What OOP patterns did you use and why?"

| Pattern | Where | Why |
|---|---|---|
| **Inheritance** | `BFS/Dijkstra/AStar : Algorithm` | Write generic code that works with ANY algorithm |
| **Polymorphism** | `algo->solve()` in CLI/API | Change the string, change behavior — no code changes |
| **Factory** | `makeAlgorithm(string)` | Centralized creation — add algorithm = change one file |
| **Dependency Injection** | `AStar(shared_ptr<Heuristic>)` | Swappable heuristic without changing AStar |
| **Strategy** | `Heuristic` interface + implementations | Different guessing strategies, same context (AStar) |

### Q8: "Walk me through `buildCSR()`."

> Three-pass O(n+m) algorithm:
> 1. COUNT: `++offsets[from + 1]` for each edge — stores out-degree counts one slot to the right
> 2. PREFIX SUM: `offsets[i] += offsets[i-1]` — converts degree counts into starting indices for each node's edges
> 3. SCATTER: For each edge, `pos = cursor[from]++`, write `targets[pos] = to` and `weights[pos] = weight`
>
> After all three passes, for any node u: its outgoing edges live at `[offsets[u], offsets[u+1])` in the targets and weights arrays.

### Q9: "Why separate CLI and API into different executables?"

> Separation of concerns. The CLI is for offline use — scripting, development, one-off queries. The API is for serving requests over HTTP — the graph loads once at startup and stays in memory. Both link against the exact same static library — zero code duplication in the routing logic.

### Q10: "How do you validate user input in the API?"

> Fail-fast pattern: each endpoint parses JSON, checks required fields exist, validates numeric ranges (node IDs must be ≥ 0 and < graph size), validates algorithm names (calls `makeAlgorithm()` — returns null if unknown). Returns HTTP 400 with a descriptive error message on the first failure. Only runs algorithms after all checks pass.

### Q11: "What does `nodesExpanded` measure and why is it important?"

> The number of nodes whose shortest path was finalized (settled) during the search. It's a machine-independent metric — unlike wall-clock time, it doesn't depend on CPU speed, background processes, or cache state. A good heuristic reduces this metric: the fewer nodes expanded, the less work the algorithm did. Across 1000 NY queries, A*-Euclidean expands ~62K nodes on average vs Dijkstra's ~135K — directly measuring how much the heuristic prunes the search space.

### Q12: "How does binary caching work?"

> After sluggish DIMACS text parsing, `Serializer::save()` writes a `.bin` file: 4 magic bytes ("GRPH"), version number, node count, then each array prefixed by its byte length and followed by its raw memory content. Loading is `Serializer::load()` — validates magic bytes, then bulk-reads directly into vectors. Skips text parsing entirely — hundreds of times faster.

### Q13: "What tests exist and what do they verify?"

> 1. `testCsrBuild`: Verifies CSR arrays have correct structure after building
> 2. `testAlgorithmsTiny`: On a hand-crafted 5-node graph: BFS finds fewest hops, Dijkstra finds least weight, A*-zero == Dijkstra, isolated node unreachable
> 3. `testSerializeRoundTrip`: Generates 1000-node grid, saves to .bin, reloads, verifies ALL arrays match byte-for-byte
> 4. `testHeuristicInvariantOnGrid`: On 50×50 grid: A*-zero cost == Dijkstra cost, A*-Euclidean cost == Dijkstra cost, AND A*-Euclidean expands FEWER nodes in aggregate

### Q14: "What is P95 and why report it?"

> P95 (95th percentile) is the value below which 95% of observations fall. For latency, it's the "worst-case typical" — 5% of queries take longer, but 95% are below this threshold. Mean can be skewed by outliers; median captures the typical case; P95 captures the tail. Together they give a complete picture of performance distribution.

### Q15: "What is `virtual = 0`?"

> "Pure virtual" — declares an abstract method that child classes MUST override. `virtual Result solve(...) = 0;` means `Algorithm` itself cannot be instantiated. `BFS`, `Dijkstra`, `AStar` each provide their own `solve()` implementation. This is how C++ implements interfaces (conceptually identical to Java's `interface`).

### Q16: "Why `unique_ptr` instead of raw pointers?"

> Safety — the object is automatically deleted when the pointer goes out of scope (RAII). No manual `delete`, no memory leaks. `make_unique<T>(args)` constructs on heap + wraps in unique_ptr in one step.

---

## Potential Improvements

### Performance

1. **Parallel benchmark**: Run each algorithm's queries on separate threads.

2. **Bidirectional Dijkstra**: Run simultaneous searches from source and destination. Halves the search radius.

3. **Contraction Hierarchies (CH)**: Preprocess graph to add shortcut edges. Sub-millisecond queries on continent-scale networks. Used by production routers.

4. **ALT (A* with Landmarks & Triangle inequality)**: Precompute distances to landmark nodes. Use triangle inequality for a tighter, more informed heuristic.

5. **Memory-mapped graph loading**: `mmap` the `.bin` file instead of reading into heap buffer. OS lazily loads pages — instant startup even for huge graphs.

6. **SIMD edge relaxation**: Process multiple edges simultaneously using AVX/NEON vector instructions.

### Code Quality

7. **Proper test framework**: Replace hand-rolled `CHECK` macro with GoogleTest or Catch2.

8. **More test coverage**: API handler tests, factory edge cases, heuristic calibration with no coordinates, serializer error paths.

9. **Graph validation function**: Add `validate()` to check CSR consistency.

10. **Streaming benchmark output**: Write per-query results as computed instead of holding everything in memory.

### Features

11. **More graph formats**: OpenStreetMap PBF/O5M direct loading.

12. **Path geometry**: Return lat/lon polyline for the path, not just node IDs.

13. **Alternative cost functions**: Time-optimized vs distance-optimized vs fuel-consumption routing.

14. **Graph visualization**: GeoJSON export with highlighted path for map rendering.

### Build & DevOps

15. **Docker image**: Reproducible build environment.

16. **GitHub Actions CI**: Build + tests + benchmarks on every PR.

17. **Profiling build targets**: CMake options for PGO, AddressSanitizer, ThreadSanitizer.

18. **Multi-package-manager support**: Conan manifest in addition to vcpkg.

---

## Quick Revision Cheatsheet

### Project in 3 Sentences

Graphene finds shortest paths on road networks using CSR graphs and three algorithms (BFS/Dijkstra/A*). It loads real DIMACS road data or generates synthetic graphs, has both CLI and HTTP API interfaces, and a benchmark harness measuring per-algorithm performance. The A* heuristic is self-calibrating — works on any graph without manual tuning.

### Key Files

| File | One-Liner |
|---|---|
| `graph/graph.hpp` | CSR data structure: 4 arrays (offsets, targets, weights, coords) + helpers |
| `graph/builder.cpp` | 3-pass algorithm: edge list → CSR arrays |
| `graph/dimacs.cpp` | Parses real DIMACS road network text files |
| `graph/serialize.cpp` | Fast binary .bin save/load (instant restarts) |
| `algorithms/algorithm.hpp` | Base class (interface) + Result struct |
| `algorithms/dijkstra.cpp` | Dijkstra with binary heap + lazy deletion |
| `algorithms/astar.cpp` | A* with pluggable heuristic (Dependency Injection) |
| `algorithms/heuristic.cpp` | Euclidean projection + self-calibration (admissibility guarantee) |
| `algorithms/factory.cpp` | `makeAlgorithm("dijkstra")` — string → Algorithm object |
| `cli/main.cpp` | 6 commands: gen-grid, gen-random, convert, info, query, bench |
| `api/main.cpp` | 3 HTTP endpoints: GET /graph/info, POST /query, POST /benchmark |
| `tests/test_graph.cpp` | Correctness tests: CSR build, algorithms, serialize, heuristic invariant |

### OOP Concepts Quick Map

| Concept | Where | Concrete Code |
|---|---|---|
| **Inheritance** | algorithms/ | `class BFS : public Algorithm` |
| **Polymorphism** | CLI/API/benchmark | `algo->solve()` — different code based on runtime type |
| **Abstraction** | algorithm.hpp | `virtual Result solve(...) = 0;` |
| **Composition** | astar.hpp | `shared_ptr<Heuristic> heuristic_;` |
| **Dependency Injection** | astar.hpp constructor | `AStar(shared_ptr<Heuristic>)` |
| **Factory** | factory.cpp | `makeAlgorithm("dijkstra")` |
| **Strategy** | heuristic.hpp/cpp | Multiple Heuristic implementations, interchangeable |
| **Static Methods** | dimacs/generator/serializer | `Dimacs::load()` — no object needed |

### Algorithm Comparison

| | BFS | Dijkstra | A* |
|---|---|---|---|
| **Frontier structure** | Queue | Min-heap (dist) | Min-heap (dist + heuristic) |
| **Finds** | Fewest hops | Least weight | Least weight |
| **Uses weights** | No | Yes | Yes |
| **Needs coordinates** | No | No | Yes (for Euclidean heuristic) |
| **Nodes expanded** | Most | Medium | Least |
| **Time complexity** | O(V+E) | O((V+E)logV) | O((V+E)logV) |

### Build Commands

```bash
# Configure + build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Generate + query a synthetic graph
./build/graphene_cli gen-grid 100 100 data/grid.bin 10
./build/graphene_cli query data/grid.bin 0 5000

# Benchmark and export
./build/graphene_cli bench data/grid.bin 1000 42 data/bench.json

# Start API server
./build/graphene_api data/grid.bin
# Or on custom port: GRAPHENE_PORT=9000 ./build/graphene_api data/grid.bin

# Query via HTTP
curl -X POST http://localhost:8080/query \
  -H 'Content-Type: application/json' \
  -d '{"src":0,"dst":5000}'
```

### Key Numbers

| Fact | Value |
|---|---|
| DIMACS NY nodes | 264,346 |
| DIMACS NY edges | 733,846 |
| Dijkstra mean time | 8.83 ms |
| A* mean time | 6.07 ms |
| A* nodes expanded ratio vs Dijkstra | ~2.2× fewer |
| CSR arrays per Graph | 4 |
| `buildCSR()` passes | 3 |
| Algorithms implemented | 4 (BFS, Dijkstra, A*-zero, A*-Euclidean) |
| CLI commands | 6 |
| API endpoints | 3 |
| OOP patterns used | 8 |
| External dependencies | 2 (Drogon, nlohmann-json) |
