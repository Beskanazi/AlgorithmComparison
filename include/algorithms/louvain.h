#ifndef ALGORITHMS_LOUVAIN_H
#define ALGORITHMS_LOUVAIN_H

#include "algorithms/clustering.h"

ClusterResult* louvain_cluster_igraph(const Graph *g, double resolution);
ClusterResult* louvain_cluster(const Graph *g, double resolution);

#endif // ALGORITHMS_LOUVAIN_H