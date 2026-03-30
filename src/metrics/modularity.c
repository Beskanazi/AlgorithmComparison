#include "metrics/modularity.h"

double compute_modularity_igraph(const Graph *g, const int *labels, double resolution) {
    if (!g || !labels || g->n_nodes == 0) return 0.0;

    // Build igraph edge list
    igraph_vector_int_t edges;
    igraph_vector_int_init(&edges, g->n_edges * 2);

    size_t idx = 0;
    for (NodeID u = 0; u < g->n_nodes; u++) {
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            NodeID v = graph_to(g, e);
            if (u < v) {  // Undirected: only add once
                VECTOR(edges)[idx++] = (igraph_integer_t)u;
                VECTOR(edges)[idx++] = (igraph_integer_t)v;
            }
        }
    }
    igraph_vector_int_resize(&edges, idx);

    // Create igraph
    igraph_t ig;
    igraph_create(&ig, &edges, (igraph_integer_t)g->n_nodes, IGRAPH_UNDIRECTED);
    igraph_vector_int_destroy(&edges);

    // Convert labels to membership vector
    igraph_vector_int_t membership;
    igraph_vector_int_init(&membership, (igraph_integer_t)g->n_nodes);
    for (NodeID i = 0; i < g->n_nodes; i++) {
        VECTOR(membership)[i] = labels[i];
    }

    // Compute modularity
    igraph_real_t mod;
    igraph_modularity(&ig, &membership, NULL, resolution, IGRAPH_UNDIRECTED, &mod);

    // Cleanup
    igraph_vector_int_destroy(&membership);
    igraph_destroy(&ig);

    return (double)mod;
}