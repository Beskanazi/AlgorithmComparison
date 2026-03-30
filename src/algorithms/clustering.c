#include "algorithms/clustering.h"
#include <stdlib.h>

void free_cluster_result(ClusterResult *result) {
    if (!result) return;
    free(result->labels);
    free(result);
}