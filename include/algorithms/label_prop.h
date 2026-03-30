#ifndef ALGORITHMS_LABEL_PROP_H
#define ALGORITHMS_LABEL_PROP_H

#include "algorithms/clustering.h"

// igraph implementation
ClusterResult* label_prop_cluster_igraph(const Graph *g);

// Your implementation
ClusterResult* label_prop_cluster(const Graph *g, int max_iterations);

#endif // ALGORITHMS_LABEL_PROP_H