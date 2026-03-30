#include "xcut/core/config.hpp"
#include "xcut/core/definitions.hpp"
#include "xcut/data_structures/graph.hpp"
#include "xcut/expanders/expander_hierarchy.hpp"
#include "xcut/expanders/refinement.hpp"
#include "xcut/expanders/sparsifier.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <vector>
#include <ctime>
#include <iostream>

/*
 * THis extern "C" prevents C++ name mangling, allowing these symbols to be
 * linked from C code. We declare the C types and functions we need to
 * access from this C++ file.
 */
extern "C" {

typedef struct Graph Graph;
typedef struct {
    int *labels;
    int num_clusters;
    double modularity;
    double runtime_ms;
} ClusterResult;

size_t graph_begin_adjacent(const Graph *g, size_t u);
size_t graph_end_adjacent(const Graph *g, size_t u);
size_t graph_to(const Graph *g, size_t e);
size_t graph_n_nodes(const Graph *g);
double compute_modularity(const Graph *g, const int *labels, double resolution);

}

extern "C" {

ClusterResult* xcut_cluster(const Graph *g, int num_clusters, int seed) {
    if (!g || num_clusters <= 0) return nullptr;

    size_t n_nodes = graph_n_nodes(g);
    if (n_nodes == 0) return nullptr;


    // Initialize spdlog logger. Xcut crashes without this
    if (!spdlog::get("xcut")) {
        auto console = spdlog::stdout_color_mt("xcut");
        console->set_level(spdlog::level::warn);
    }

    ClusterResult *result = static_cast<ClusterResult*>(malloc(sizeof(ClusterResult)));
    if (!result) return nullptr;

    result->labels = static_cast<int*>(malloc(n_nodes * sizeof(int)));
    if (!result->labels) {
        free(result);
        return nullptr;
    }

    /* Convert CSR adjacency to XCut's edge list format.*/
    try {
        std::vector<std::pair<NodeID, NodeID>> edges;

        for (size_t u = 0; u < n_nodes; u++) {
            for (size_t e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
                size_t v = graph_to(g, e);
                if (u < v) {
                    edges.emplace_back(
                        static_cast<NodeID>(u),
                        static_cast<NodeID>(v)
                    );
                }
            }
        }

        /* Construct XCut graph object. false flag indicates an unweighted graph. */
        ::Graph xcut_graph(static_cast<NodeID>(n_nodes), edges, false);

        if (xcut_graph.has_degree_zero()) {
            std::cerr << "Warning: Graph has isolated nodes" << std::endl;
        }

        Config config(num_clusters);
        config.m_verbose = false;
        config.m_debug = false;

        clock_t start = clock();

        auto sparsifier = expander_hierarchy(&xcut_graph, &config);
        normalized_cut(sparsifier, config.m_num_clusters);
        for (NodeID level = sparsifier.size(); level > 0; level--) {
            refine(sparsifier, level - 1, config);
        }
        auto clustering = sparsifier.clustering();

        clock_t end = clock();
        result->runtime_ms = static_cast<double>(end - start) * 1000.0 / CLOCKS_PER_SEC;

        if (clustering.size() != n_nodes) {
            std::cerr << "Error: Clustering size (" << clustering.size()
                      << ") != graph nodes (" << n_nodes << ")" << std::endl;
            free(result->labels);
            free(result);
            return nullptr;
        }

        int max_label = -1;
        for (size_t i = 0; i < n_nodes; i++) {
            result->labels[i] = static_cast<int>(clustering[i]);
            if (result->labels[i] > max_label) {
                max_label = result->labels[i];
            }
        }
        result->num_clusters = max_label + 1;

        return result;

    } catch (const std::exception& e) {
        std::cerr << "XCut error: " << e.what() << std::endl;
        free(result->labels);
        free(result);
        return nullptr;
    }
}

}