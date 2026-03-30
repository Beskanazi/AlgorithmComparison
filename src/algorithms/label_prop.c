
#include "../../include/algorithms/label_prop.h"
#include "metrics/modularity.h"
#include <igraph/igraph.h>
#include <stdlib.h>
#include <time.h>

static igraph_t *graph_to_igraph(const Graph *g) {
  if (!g || g->n_nodes == 0)
    return NULL;

  // Count edges first
  size_t edge_count = 0;
  for (NodeID u = 0; u < g->n_nodes; u++) {
    for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
      NodeID v = graph_to(g, e);
      if (u < v) edge_count++;
    }
  }

  // Pre-allocate exact size
  igraph_vector_int_t edges;
  igraph_vector_int_init(&edges, edge_count * 2);

  size_t idx = 0;
  for (NodeID u = 0; u < g->n_nodes; u++) {
    for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
      NodeID v = graph_to(g, e);
      if (u < v) {
        VECTOR(edges)[idx++] = (igraph_integer_t)u;
        VECTOR(edges)[idx++] = (igraph_integer_t)v;
      }
    }
  }

  igraph_t *ig = malloc(sizeof(igraph_t));
  if (!ig) {
    igraph_vector_int_destroy(&edges);
    return NULL;
  }

  igraph_create(ig, &edges, (igraph_integer_t)g->n_nodes, IGRAPH_UNDIRECTED);
  igraph_vector_int_destroy(&edges);

  return ig;
}

ClusterResult *label_prop_cluster_igraph(const Graph *g) {
  if (!g || g->n_nodes == 0)
    return NULL;

  ClusterResult *result = malloc(sizeof(ClusterResult));
  if (!result)
    return NULL;

  result->labels = malloc(g->n_nodes * sizeof(int));
  if (!result->labels) {
    free(result);
    return NULL;
  }

  igraph_t *ig = graph_to_igraph(g);
  if (!ig) {
    free(result->labels);
    free(result);
    return NULL;
  }

  igraph_vector_int_t membership;
  igraph_vector_int_init(&membership, g->n_nodes);

  clock_t start = clock();
  igraph_error_t err = igraph_community_label_propagation(
      ig, &membership, IGRAPH_ALL, NULL, NULL, NULL, 0);
  clock_t end = clock();
  result->runtime_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

  if (err != IGRAPH_SUCCESS) {
    printf("Error: Label propagation failed with error code %d\n", err);
    igraph_vector_int_destroy(&membership);
    igraph_destroy(ig);
    free(ig);
    free(result->labels);
    free(result);
    return NULL;
  }

  long size = igraph_vector_int_size(&membership);
  if (size != g->n_nodes) {
    printf("Error: Membership vector size %ld does not match graph nodes %zu\n",
           size, g->n_nodes);
    igraph_vector_int_destroy(&membership);
    igraph_destroy(ig);
    free(ig);
    free(result->labels);
    free(result);
    return NULL;
  }

  int max_label = -1;
  for (NodeID i = 0; i < g->n_nodes; i++) {
    result->labels[i] = (int)VECTOR(membership)[i];
    if (result->labels[i] > max_label)
      max_label = result->labels[i];
  }
  result->num_clusters = max_label + 1;


  igraph_vector_int_destroy(&membership);
  igraph_destroy(ig);
  free(ig);

  return result;
}
