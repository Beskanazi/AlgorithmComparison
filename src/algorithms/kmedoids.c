#include "algorithms/kmedoids.h"
#include "metrics/modularity.h"
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    size_t *data;
    size_t front, rear, size, capacity;
} Queue;

static Queue* queue_create(size_t capacity) {
    Queue *q = malloc(sizeof(Queue));
    if (!q) return NULL;
    q->data = malloc(capacity * sizeof(size_t));
    if (!q->data) { free(q); return NULL; }
    q->front = q->rear = q->size = 0;
    q->capacity = capacity;
    return q;
}

static void queue_enqueue(Queue *q, size_t node) {
    if (q->size >= q->capacity) return;
    q->data[q->rear] = node;
    q->rear = (q->rear + 1) % q->capacity;
    q->size++;
}

static size_t queue_dequeue(Queue *q) {
    size_t node = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    return node;
}

static bool queue_empty(Queue *q) {
    return q->size == 0;
}

static void queue_free(Queue *q) {
    if (!q) return;
    free(q->data);
    free(q);
}

static void multi_source_bfs(const Graph *g, size_t *medoids, int k, int *labels) {
    for (NodeID i = 0; i < g->n_nodes; i++) {
        labels[i] = -1;
    }

    Queue *q = queue_create(g->n_nodes);
    if (!q) return;

    for (int c = 0; c < k; c++) {
        if (medoids[c] < g->n_nodes) {
            labels[medoids[c]] = c;
            queue_enqueue(q, medoids[c]);
        }
    }

    while (!queue_empty(q)) {
        size_t u = queue_dequeue(q);

        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            NodeID v = graph_to(g, e);
            if (labels[v] == -1) {
                labels[v] = labels[u];
                queue_enqueue(q, v);
            }
        }
    }

    queue_free(q);
}

static size_t find_cluster_centroid(const Graph *g, const int *labels, int target_cluster, size_t n) {
    // Collect cluster nodes
    size_t *cluster_nodes = malloc(n * sizeof(size_t));
    int cluster_size = 0;

    for (size_t i = 0; i < n; i++) {
        if (labels[i] == target_cluster) {
            cluster_nodes[cluster_size++] = i;
        }
    }

    if (cluster_size == 0) {
        free(cluster_nodes);
        return 0;
    }

    if (cluster_size == 1) {
        size_t result = cluster_nodes[0];
        free(cluster_nodes);
        return result;
    }

    // Build BFS tree within cluster
    int *parent = malloc(n * sizeof(int));
    int *subtree_size = malloc(n * sizeof(int));
    int *visit_order = malloc(cluster_size * sizeof(int));
    bool *in_cluster = calloc(n, sizeof(bool));

    for (size_t i = 0; i < n; i++) {
        parent[i] = -1;
        subtree_size[i] = 1;
    }

    for (int i = 0; i < cluster_size; i++) {
        in_cluster[cluster_nodes[i]] = true;
    }

    // Find highest-degree node in cluster as root
    size_t root = cluster_nodes[0];
    int max_degree = 0;

    for (int i = 0; i < cluster_size; i++) {
        size_t node = cluster_nodes[i];
        int degree = 0;
        for (EdgeID e = graph_begin_adjacent(g, node); e != graph_end_adjacent(g, node); e++) {
            NodeID neighbor = graph_to(g, e);
            if (in_cluster[neighbor]) degree++;
        }
        if (degree > max_degree) {
            max_degree = degree;
            root = node;
        }
    }

    // BFS within cluster
    Queue *q = queue_create(cluster_size);
    bool *visited = calloc(n, sizeof(bool));

    queue_enqueue(q, root);
    visited[root] = true;
    parent[root] = (int)root;

    int visit_count = 0;

    while (!queue_empty(q)) {
        size_t u = queue_dequeue(q);
        visit_order[visit_count++] = (int)u;

        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            NodeID v = graph_to(g, e);
            if (!visited[v] && in_cluster[v]) {
                visited[v] = true;
                parent[v] = (int)u;
                queue_enqueue(q, v);
            }
        }
    }

    queue_free(q);
    free(visited);

    // Compute subtree sizes (bottom-up)
    for (int i = visit_count - 1; i >= 0; i--) {
        int u = visit_order[i];
        int p = parent[u];
        if (p != u && p != -1) {
            subtree_size[p] += subtree_size[u];
        }
    }

    // Find centroid
    size_t current = root;
    size_t threshold = (cluster_size - 1) / 2;

    bool moved = true;
    int walk_steps = 0;

    while (moved) {
        moved = false;
        if (++walk_steps > cluster_size) {
            /*break infinite centroid walk on disconnected clusters */
            printf("WARNING: centroid walk exceeded cluster_size=%d for cluster %d\n", cluster_size, target_cluster);
            break;
        }

        for (EdgeID e = graph_begin_adjacent(g, current); e != graph_end_adjacent(g, current); e++) {
            NodeID neighbor = graph_to(g, e);

            if (parent[neighbor] == (int)current && in_cluster[neighbor]) {
                if ((size_t)subtree_size[neighbor] > threshold) {
                    current = neighbor;
                    moved = true;
                    break;
                }
            }
        }
    }

    free(cluster_nodes);
    free(parent);
    free(subtree_size);
    free(visit_order);
    free(in_cluster);

    return current;
}


static size_t* find_all_centroids(const Graph *g, const int *labels, int k) {
    size_t *medoids = malloc(k * sizeof(size_t));
    if (!medoids) return NULL;
    for (int c = 0; c < k; c++) {
        medoids[c] = find_cluster_centroid(g, labels, c, g->n_nodes);
    }
    return medoids;
}


static size_t* find_initial_medoids(const Graph *g, int k, int seed) {
    size_t n = g->n_nodes;
    srand(seed);

    int    *labels    = malloc(n * sizeof(int));
    size_t *seeds_arr = malloc(k * sizeof(size_t));

    if (!labels || !seeds_arr) {
        free(labels); free(seeds_arr);
        return NULL;
    }

    for (size_t i = 0; i < n; i++) labels[i] = -1;

    for (int c = 0; c < k; c++)
        seeds_arr[c] = rand() % n;

    Queue *q = queue_create(n);
    for (int c = 0; c < k; c++) {
        if (labels[seeds_arr[c]] == -1) {
            labels[seeds_arr[c]] = c;
            queue_enqueue(q, seeds_arr[c]);
        }
    }

    while (!queue_empty(q)) {
        size_t u = queue_dequeue(q);
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            NodeID v = graph_to(g, e);
            if (labels[v] == -1) {
                labels[v] = labels[u];
                queue_enqueue(q, v);
            }
        }
    }
    queue_free(q);

    size_t *medoids = find_all_centroids(g, labels, k);

    free(labels);
    free(seeds_arr);
    return medoids;
}


ClusterResult* kmedoids_cluster(const Graph *g, int k, int max_iter, int seed) {
    if (!g || k <= 0 || g->n_nodes == 0) return NULL;

    ClusterResult *result = malloc(sizeof(ClusterResult));
    if (!result) return NULL;

    result->labels = malloc(g->n_nodes * sizeof(int));
    if (!result->labels) {
        free(result);
        return NULL;
    }

    clock_t start = clock();

    // Step 1: Get initial medoids
    size_t *medoids = find_initial_medoids(g, k, seed);
    if (!medoids) {
        free(result->labels);
        free(result);
        return NULL;
    }

    // Step 2: Iterative refinement
    int actual_iters = 0;

    for (int iter = 0; iter < max_iter; iter++) {
        actual_iters++;

        multi_source_bfs(g, medoids, k, result->labels);

        size_t *new_medoids = find_all_centroids(g, result->labels, k);
        if (!new_medoids) break;

        bool changed = false;
        for (int c = 0; c < k; c++) {
            if (new_medoids[c] != medoids[c]) {
                changed    = true;
                medoids[c] = new_medoids[c];
            }
        }
        free(new_medoids);

        if (!changed) break;
    }

    printf("  K-Medoids: %d iters, %.2fms\n", actual_iters,
           (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC);

    // Final assignment with converged medoids
    multi_source_bfs(g, medoids, k, result->labels);

    free(medoids);

    // Count clusters
    int max_label = -1;
    for (NodeID i = 0; i < g->n_nodes; i++) {
        if (result->labels[i] > max_label) {
            max_label = result->labels[i];
        }
    }
    result->num_clusters = max_label + 1;

    clock_t end = clock();
    result->runtime_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;


    return result;
}
