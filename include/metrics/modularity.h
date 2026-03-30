#ifndef MODULARITY_H
#define MODULARITY_H

#include "io/graph.h"
#include <igraph/igraph.h>
#include <stdlib.h>

double compute_modularity_igraph(const Graph *g, const int *labels, double resolution);

#endif // MODULARITY_H