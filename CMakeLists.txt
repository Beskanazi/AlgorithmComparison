cmake_minimum_required(VERSION 3.25)
project(tez5 C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# ── XCut ─────────────────────────────────────────────────────────────────────
FetchContent_Declare(
        xcut
        GIT_REPOSITORY https://gitlab.com/vietaa/xcut.git
        GIT_TAG        main
)
FetchContent_MakeAvailable(xcut)

if(TARGET xcut_lib)
    target_compile_options(xcut_lib PRIVATE -O3)
    target_compile_definitions(xcut_lib PRIVATE NDEBUG)
endif()

# ── igraph ────────────────────────────────────────────────────────────────────
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

find_package(igraph QUIET)

if(NOT igraph_FOUND)
    message(STATUS "igraph not found on system. Downloading via FetchContent...")
    FetchContent_Declare(
            igraph
            GIT_REPOSITORY https://github.com/igraph/igraph.git
            GIT_TAG        1.0.1
    )
    FetchContent_MakeAvailable(igraph)
else()
    message(STATUS "Found igraph on system: ${igraph_DIR}")
endif()

# ── Benchmark executable ──────────────────────────────────────────────────────
add_executable(benchmark
        main.c
        src/io/graph.c
        src/metrics/modularity.c
        src/metrics/silhouette.c
        src/metrics/ncut.c
        src/metrics/extra_metrics.c
        src/algorithms/louvain.c
        src/algorithms/clustering.c
        src/algorithms/label_prop.c
        src/algorithms/kmedoids.c
        src/algorithms/infomap.c
        src/algorithms/xcut_wrapper.cpp
        main.c
)

target_include_directories(benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${xcut_SOURCE_DIR}/include
)

target_link_libraries(benchmark PRIVATE
        igraph::igraph
        xcut_lib
)


