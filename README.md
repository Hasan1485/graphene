# Graphene

A fast, demonstrably-correct **shortest-path routing engine** in modern C++20,
built over real road networks. It ships a CSR graph core, three classic search
algorithms (BFS, Dijkstra, A\*), a benchmark harness with JSON export, and a
small HTTP API.

---

## Highlights

- **CSR (Compressed Sparse Row) graph** — the cache-friendly layout real routing
  engines use. Three flat arrays, trivially serializable.
- **Three algorithms behind one interface** — BFS (hop baseline), Dijkstra
  (binary heap, lazy deletion), and A\* with a pluggable heuristic.
- **A self-calibrating, provably-admissible A\* heuristic** — great-circle
  distance scaled by the graph's fastest edge, so it works on both real road
  networks and the synthetic generators without manual tuning.
- **Instant startup** — first DIMACS load is cached to a binary format; restarts
  are a single bulk read.
- **Benchmark harness** — runs every algorithm over the same seeded query set and
  exports per-query rows plus aggregate stats (mean/median/p95 time, mean nodes
  expanded) as JSON.
- **Drogon HTTP API** — three endpoints for graph info, single queries, and
  benchmarks.

---

## Project layout

```
src/
  graph/        CSR graph, edge-list builder, DIMACS loader, generator, serializer
  algorithms/   Algorithm interface, BFS, Dijkstra, A*, heuristics, factory
  benchmark/    query generation, batch runner, metrics, JSON export
  api/          Drogon HTTP server
  cli/          command-line driver
tests/          correctness tests (ctest)
```

---

## Building

Dependencies (`drogon`, `nlohmann-json`) are managed by **vcpkg** via the
manifest in [vcpkg.json](vcpkg.json). Requires CMake ≥ 3.20 and a C++20 compiler.

```bash
# 1. Get vcpkg (once)
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

# 2. Configure (vcpkg installs deps automatically) and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build

# 3. Run the tests
ctest --test-dir build --output-on-failure
```

Three binaries are produced in `build/`: `graphene_cli`, `graphene_api`, and
`test_graph`.

---

## Quick start (no download needed)

The synthetic generator lets you exercise everything offline:

```bash
# Generate a 1000x1000 grid (1M nodes), cache it as binary
./build/graphene_cli gen-grid 1000 1000 data/grid.bin 10

# Inspect it
./build/graphene_cli info data/grid.bin

# Run a single query across all algorithms
./build/graphene_cli query data/grid.bin 0 12345

# Benchmark 1000 random queries and write JSON
./build/graphene_cli bench data/grid.bin 1000 42 data/bench.json
```

---

## Using real road data (DIMACS)

Graphene uses the **9th DIMACS Implementation Challenge** USA road networks,
which ship real edge weights *and* coordinates (SNAP road graphs have neither).
The `data/` directory is gitignored — everything below stays local.

**Step 1 — download the New York network (~264K nodes): distance graph + coords.**
(The DIMACS host is slow; the two files are ~3.5 MB and ~2 MB compressed.)

```bash
mkdir -p data && cd data
curl -O http://www.diag.uniroma1.it/~challenge9/data/USA-road-d/USA-road-d.NY.gr.gz
curl -O http://www.diag.uniroma1.it/~challenge9/data/USA-road-d/USA-road-d.NY.co.gz
cd ..
```

**Step 2 — decompress.**

```bash
gunzip -f data/USA-road-d.NY.gr.gz data/USA-road-d.NY.co.gz
```

**Step 3 — convert to the fast binary format** (parses `.gr` + `.co`, builds the
CSR graph, writes `NY.bin`). After this the engine only needs `NY.bin`.

```bash
./build/graphene_cli convert data/USA-road-d.NY.gr data/USA-road-d.NY.co data/NY.bin
./build/graphene_cli info data/NY.bin     # sanity check: 264346 nodes, coords: yes
```

**Step 4 (optional) — reclaim disk** by deleting the raw text/archive files; only
`NY.bin` is needed from here on (re-run steps 1–3 to recreate it):

```bash
rm -f data/USA-road-d.NY.gr data/USA-road-d.NY.co \
      data/USA-road-d.NY.gr.gz data/USA-road-d.NY.co.gz
```

**Step 5 — benchmark all algorithms on 1000 random queries.**

```bash
./build/graphene_cli bench data/NY.bin 1000 42 data/NY-bench.json
```

For the headline numbers, repeat with a larger network (e.g. `CAL` ~1.9M nodes
or `USA` ~24M nodes) from the same download page — swap `USA-road-d.NY` for
`USA-road-d.CAL`, etc.

---

## Benchmark results

1000 random queries on the DIMACS **New York** distance network (264,346 nodes,
733,846 edges), Apple Silicon, Release build:

| algorithm         | mean (ms) | median (ms) | p95 (ms) | mean nodes expanded |
|-------------------|-----------|-------------|----------|---------------------|
| bfs               | 2.41      | 2.50        | 4.55     | 134,746             |
| dijkstra          | 8.83      | 9.03        | 16.48    | 134,708             |
| astar-zero        | 10.59     | 10.71       | 19.90    | 134,708             |
| **astar-euclidean** | **6.07**  | **4.91**    | 15.91    | **62,345**          |

Reading the numbers:

- **`astar-zero` expands the exact same node count as `dijkstra`** (134,708) —
  the correctness gate: A\* with a zero heuristic *is* Dijkstra.
- **`astar-euclidean` expands ~2.2× fewer nodes** and is the fastest weighted
  search, because the heuristic pulls the frontier toward the goal.
- `bfs` is fastest in wall-clock time but solves a *different* problem (fewest
  hops, ignoring weights), so its path cost is not comparable.

Regenerate with: `./build/graphene_cli bench data/NY.bin 1000 42 data/NY-bench.json`

## CLI reference

```
graphene_cli gen-grid   <rows> <cols> <out.bin> [maxWeight]
graphene_cli gen-random <n> <avgDeg> <seed> <out.bin>
graphene_cli convert    <file.gr> <file.co|-> <out.bin>
graphene_cli info       <graph>
graphene_cli query      <graph> <src> <dst> [algos...]
graphene_cli bench      <graph> <numQueries> <seed> <out.json> [algos...]
```

Algorithm names: `bfs`, `dijkstra`, `astar` (= `astar-euclidean`), `astar-zero`.
`<graph>` may be a `.bin` cache or a DIMACS `.gr` file.

---

## HTTP API

Start the server with a graph (binary cache or DIMACS `.gr`; DIMACS is cached to
`.bin` automatically on first load):

```bash
./build/graphene_api data/NY.bin
# or: GRAPHENE_PORT=9000 ./build/graphene_api data/NY.bin
# with no argument it generates a demo grid so the server always runs
```

### `GET /graph/info`

```bash
curl http://localhost:8080/graph/info
# {"nodes":264346,"edges":733846,"avgDegree":2.77...}
```

### `POST /query`

```bash
curl -s -X POST http://localhost:8080/query \
  -H 'Content-Type: application/json' \
  -d '{"src": 0, "dst": 100000, "algorithms": ["dijkstra", "astar-euclidean"]}'
# -> array of results: [{algorithm, path, cost, nodesExpanded, timeMs, reachable}, ...]
```

### `POST /benchmark`

```bash
curl -s -X POST http://localhost:8080/benchmark \
  -H 'Content-Type: application/json' \
  -d '{"numQueries": 200, "seed": 42, "algorithms": ["dijkstra", "astar-euclidean"]}'
# -> { config, summary:[...per-algorithm aggregates...], perQuery:[...] }
```

Invalid input (out-of-range `src`/`dst`, unknown algorithm name, malformed JSON)
returns HTTP 400 with `{"error": "..."}`.

---

## How the A\* heuristic stays admissible

A\* needs a heuristic that never *overestimates* the remaining cost. Graphene
projects every node's lat/lon onto a local plane in metres (an equirectangular
projection about the graph's mean latitude — within a fraction of a percent of
true great-circle distance at city/state scale, but trig-free in the hot loop),
then auto-calibrates a scale factor

```
s = min over all edges ( weight / planarDistance(u, v) )
```

— the smallest cost-per-metre anywhere in the graph (i.e. the fastest road).
Then `h(n) = s · planarDistance(n, dst)` is guaranteed to be a lower bound, and
because Euclidean distance in the projected plane obeys the triangle inequality
the heuristic is also **consistent**, so A\* can stop the first time it pops the
destination. The projection and scale are computed once per graph and cached, so
the cost is amortized across every query. The same formula works whether weights
are travel time, distance, or synthetic grid costs.

The correctness gate (enforced in the tests): **A\*-zero must equal Dijkstra**,
and on geographic data **A\*-Euclidean expands fewer nodes** than Dijkstra.
