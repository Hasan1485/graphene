cmake_minimum_required(VERSION 3.20)
project(graphene LANGUAGES CXX)

# --- Language standard -------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json so clangd / editors get accurate flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to an optimized build; real benchmark numbers require it.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Dependencies (provided by vcpkg) ---------------------------------------
find_package(Drogon CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# --- Core library: graph + algorithms + benchmark ---------------------------
# A single static library shared by the CLI, the API server, and the tests so
# every binary runs the exact same code paths.
add_library(graphene_core STATIC
  src/graph/builder.cpp
  src/graph/generator.cpp
  src/graph/dimacs.cpp
  src/graph/serialize.cpp
  src/algorithms/heuristic.cpp
  src/algorithms/bfs.cpp
  src/algorithms/dijkstra.cpp
  src/algorithms/astar.cpp
  src/algorithms/factory.cpp
  src/benchmark/benchmark.cpp
  src/benchmark/json_export.cpp
)
target_include_directories(graphene_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(graphene_core PUBLIC nlohmann_json::nlohmann_json)

# --- CLI driver (graph generation, conversion, queries, benchmarks) ---------
add_executable(graphene_cli src/cli/main.cpp)
target_link_libraries(graphene_cli PRIVATE graphene_core)

# --- HTTP API server --------------------------------------------------------
add_executable(graphene_api src/api/main.cpp)
target_link_libraries(graphene_api PRIVATE graphene_core Drogon::Drogon)

# --- Tests ------------------------------------------------------------------
enable_testing()
add_executable(test_graph tests/test_graph.cpp)
target_link_libraries(test_graph PRIVATE graphene_core)
add_test(NAME test_graph COMMAND test_graph)

# --- Formatting (clang-format; the Spotless analog) -------------------------
# Style lives in .clang-format. Two targets mirror spotless:apply / :check:
#   cmake --build build --target format         # rewrite files in place
#   cmake --build build --target format-check   # fail if anything is unformatted
file(GLOB_RECURSE GRAPHENE_FORMAT_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${GRAPHENE_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "clang-format: rewriting sources in place")
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${GRAPHENE_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "clang-format: checking formatting")
else()
  message(STATUS "clang-format not found; 'format' / 'format-check' targets unavailable")
endif()
