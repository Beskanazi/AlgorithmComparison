#include "metrics/extra_metrics.h"
#include <stdlib.h>
#include <math.h>


ClusterStats compute_cluster_stats(const int *labels, size_t n_nodes, int k) {
    ClusterStats stats = {0};

    if (!labels || k <= 0 || n_nodes == 0) {
        return stats;
    }

    int *sizes = calloc(k, sizeof(int));
    if (!sizes) return stats;

    // Count nodes per cluster
    for (size_t i = 0; i < n_nodes; i++) {
        if (labels[i] >= 0 && labels[i] < k) {
            sizes[labels[i]]++;
        }
    }

    // Find min, max, sum, and count singletons
    stats.min_size = sizes[0];
    stats.max_size = sizes[0];
    stats.singletons = 0;
    double sum = 0;

    for (int i = 0; i < k; i++) {
        sum += sizes[i];
        if (sizes[i] < stats.min_size) stats.min_size = sizes[i];
        if (sizes[i] > stats.max_size) stats.max_size = sizes[i];
        if (sizes[i] == 1) stats.singletons++;
    }

    stats.avg_size = sum / k;

    // Compute standard deviation
    double variance = 0;
    for (int i = 0; i < k; i++) {
        double diff = sizes[i] - stats.avg_size;
        variance += diff * diff;
    }
    stats.stddev = sqrt(variance / k);

    free(sizes);
    return stats;
}


/*
 * Measures what fraction of edges fall within clusters rather than between them.
 * A coverage of 1.0 means all edges are internal; 0.0 means all edges cross
 * cluster boundaries.
 */
double compute_coverage(const Graph *g, const int *labels) {
    if (!g || !labels || g->n_nodes == 0) return 0.0;

    size_t internal_edges = 0;
    size_t total_edges = 0;

    for (NodeID u = 0; u < g->n_nodes; u++) {
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            NodeID v = graph_to(g, e);
            total_edges++;
            if (labels[u] == labels[v]) {
                internal_edges++;
            }
        }
    }

    if (total_edges == 0) return 0.0;
    return (double)internal_edges / (double)total_edges;
}


/*
 * Computes the average conductance across all clusters.
 * Conductance measures how well-separated a cluster is from the rest of the graph.
 * For each cluster: conductance = cut(C) / min(vol(C), vol(V-C))
 * Lower values indicate better-separated clusters.
 */
double compute_conductance(const Graph *g, const int *labels) {
    if (!g || !labels || g->n_nodes == 0) return 0.0;

    // Find number of clusters
    int max_label = -1;
    for (NodeID i = 0; i < g->n_nodes; i++) {
        if (labels[i] > max_label) max_label = labels[i];
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

    double total_vol = 0;

    // Compute cut and volume for each cluster
    for (NodeID u = 0; u < g->n_nodes; u++) {
        int cu = labels[u];
        if (cu < 0) continue;

        EdgeID degree = graph_end_adjacent(g, u) - graph_begin_adjacent(g, u);
        vol[cu] += (double)degree;
        total_vol += (double)degree;

        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            NodeID v = graph_to(g, e);
            if (labels[v] != cu) {
                cut[cu] += 1.0;
            }
        }
    }

    double total_conductance = 0.0;
    int valid_clusters = 0;

    for (size_t i = 0; i < k; i++) {
        if (vol[i] > 0) {
            double vol_complement = total_vol - vol[i];
            double min_vol = (vol[i] < vol_complement) ? vol[i] : vol_complement;
            if (min_vol > 0) {
                double c = cut[i] / min_vol;
                total_conductance += c;
                valid_clusters++;
            }
        }
    }

    free(cut);
    free(vol);

    if (valid_clusters == 0) return 0.0;
    return total_conductance / valid_clusters;
}


double compute_density(const Graph *g, const int *labels) {
    (void)labels;  // unused
    if (!g || g->n_nodes <= 1) return 0.0;
    return (2.0 * g->n_edges) / ((double)g->n_nodes * (g->n_nodes - 1));
}
