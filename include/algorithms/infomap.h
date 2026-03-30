#ifndef ALGORITHMS_INFOMAP_H
#define ALGORITHMS_INFOMAP_H

#include "algorithms/clustering.h"

ClusterResult* infomap_cluster(const Graph *g, int num_trials, int seed);

#endif // ALGORITHMS_INFOMAP_H