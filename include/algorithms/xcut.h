#ifndef ALGORITHMS_XCUT_H
#define ALGORITHMS_XCUT_H

#include "algorithms/clustering.h"

#ifdef __cplusplus
extern "C" {
#endif

    ClusterResult* xcut_cluster(const Graph *g, int num_clusters, int seed);

#ifdef __cplusplus
}
#endif

#endif // ALGORITHMS_XCUT_H