#ifndef ALGORITHMS_CLUSTERING_H
#define ALGORITHMS_CLUSTERING_H

#include "io/graph.h"

typedef struct {
    int *labels;
    int num_clusters;
    double modularity;
    double runtime_ms;
} ClusterResult;

void free_cluster_result(ClusterResult *result);

#endif // ALGORITHMS_CLUSTERING_H