#ifndef EXTRA_METRICS_H
#define EXTRA_METRICS_H

#include "io/graph.h"


typedef struct {
    double avg_size;    // Average cluster size
    int min_size;       // Smallest cluster size
    int max_size;       // Largest cluster size
    double stddev;      // Standard deviation of sizes
    int singletons;     // Number of single-node clusters
} ClusterStats;


ClusterStats compute_cluster_stats(const int *labels, size_t n_nodes, int k);
double compute_coverage(const Graph *g, const int *labels);
double compute_density(const Graph *g, const int *labels);

#endif