#include "algorithms/infomap.h"
#include "metrics/modularity.h"
#include <igraph/igraph.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


static igraph_t *graph_to_igraph(const Graph *g) {
  if (!g || g->n_nodes == 0)
    return NULL;

  igraph_vector_int_t edges;
  igraph_vector_int_init(&edges, 0); // ← Start empty

  for (NodeID u = 0; u < g->n_nodes; u++) {
    for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u);
         e++) {
      NodeID v = graph_to(g, e);
      if (u < v) {
        igraph_vector_int_push_back(&edges, (igraph_integer_t)u);
        igraph_vector_int_push_back(&edges, (igraph_integer_t)v);
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



ClusterResult *infomap_cluster(const Graph *g, int num_trials, int seed) {
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

  igraph_real_t codelength;
  igraph_rng_seed(igraph_rng_default(), seed);

  clock_t start = clock();
  int res = igraph_community_infomap(ig,
                                     NULL,       // edge weights
                                     NULL,       // vertex weights
                                     num_trials, // number of trials
                                     0,          // is_regularized
                                     0.0,        // regularization_strength
                                     &membership, &codelength);
  clock_t end = clock();
  result->runtime_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

  if (res != IGRAPH_SUCCESS) {
    printf("Error: Infomap failed.\n");
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