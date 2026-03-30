#ifndef ALGORITHMS_KMEDOIDS_H
#define ALGORITHMS_KMEDOIDS_H

#include "algorithms/clustering.h"

ClusterResult* kmedoids_cluster(const Graph *g, int k, int max_iter, int seed);

#endif // ALGORITHMS_KMEDOIDS_H