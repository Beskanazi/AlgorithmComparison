#ifndef METRICS_SILHOUETTE_H
#define METRICS_SILHOUETTE_H

#include "io/graph.h"

// Medoid finding
int*  find_medoids(const Graph *g, const int *labels, int k);

// Default silhouette used by benchmark (corrected method)
float evaluate_silhouette(const Graph *g, const int *labels, int k);

float evaluate_silhouette_standard_with_medoids(const Graph *g, const int *labels, const int *medoids, int k);
float evaluate_silhouette_fast_with_medoids(const Graph *g, const int *labels, const int *medoids, int k);
float evaluate_silhouette_corrected_with_medoids(const Graph *g, const int *labels, const int *medoids, int k);

#endif // METRICS_SILHOUETTE_H