#include "metrics/ncut.h"
#include "io/graph.h"
#include <stdlib.h>

double compute_ncut(const Graph *g, const int *labels) {
    if (!g || !labels || g->n_nodes == 0) return 0.0;

    // Find number of communities
    int max_label = -1;
    for (NodeID i = 0; i < g->n_nodes; i++) {
        EdgeID degree = graph_end_adjacent(g, i) - graph_begin_adjacent(g, i);
        if (degree > 0 && labels[i] > max_label) {
            max_label = labels[i];
        }
    }
    if (max_label < 0) return 0.0;

    size_t k = (size_t)(max_label + 1);

    double *cut = calloc(k, sizeof(double));
    double *vol = calloc(k, sizeof(double));
    if (!cut || !vol) {
        free(cut);
        free(vol);
        return 0.0;
    }

    for (NodeID u = 0; u < g->n_nodes; u++) {
        EdgeID degree = graph_end_adjacent(g, u) - graph_begin_adjacent(g, u);
        if (degree == 0) continue;

        int cu = labels[u];
        if (cu < 0) continue;

        // Volume: total degree of nodes in community
        vol[cu] += (double)degree;

        // Count edges going outside the community
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            NodeID v = graph_to(g, e);
            int cv = labels[v];

            if (cu != cv) {
                cut[cu] += 1.0;
            }
        }
    }

    double ncut = 0.0;
    for (size_t i = 0; i < k; i++) {
        if (vol[i] > 0.0) {
            ncut += cut[i] / vol[i];
        }
    }

    free(cut);
    free(vol);
    return ncut / (double)k;
}