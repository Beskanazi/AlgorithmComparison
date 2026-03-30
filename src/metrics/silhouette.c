#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include "io/graph.h"


typedef struct {
    int *nodes;
    int *sources;
    int *distances;
    int front, rear, size, capacity;
} BFSQueue;

static BFSQueue* bfs_queue_create(int capacity) {
    BFSQueue *q = malloc(sizeof(BFSQueue));
    if (!q) return NULL;
    q->nodes     = malloc(capacity * sizeof(int));
    q->sources   = malloc(capacity * sizeof(int));
    q->distances = malloc(capacity * sizeof(int));
    if (!q->nodes || !q->sources || !q->distances) {
        free(q->nodes); free(q->sources); free(q->distances); free(q);
        return NULL;
    }
    q->front = q->rear = q->size = 0;
    q->capacity = capacity;
    return q;
}

static void bfs_queue_enqueue(BFSQueue *q, int node, int source, int distance) {
    if (q->size >= q->capacity) return;
    q->nodes[q->rear]     = node;
    q->sources[q->rear]   = source;
    q->distances[q->rear] = distance;
    q->rear = (q->rear + 1) % q->capacity;
    q->size++;
}

static void bfs_queue_dequeue(BFSQueue *q, int *node, int *source, int *distance) {
    *node     = q->nodes[q->front];
    *source   = q->sources[q->front];
    *distance = q->distances[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
}

static bool bfs_queue_empty(BFSQueue *q) { return q->size == 0; }

static void bfs_queue_free(BFSQueue *q) {
    if (!q) return;
    free(q->nodes); free(q->sources); free(q->distances); free(q);
}


typedef struct {
    int *data;
    int front, rear, size, capacity;
} SimpleQueue;

static SimpleQueue* simple_queue_create(int capacity) {
    SimpleQueue *q = malloc(sizeof(SimpleQueue));
    if (!q) return NULL;
    q->data = malloc(capacity * sizeof(int));
    if (!q->data) { free(q); return NULL; }
    q->front = q->rear = q->size = 0;
    q->capacity = capacity;
    return q;
}

static void simple_queue_enqueue(SimpleQueue *q, int node) {
    if (q->size >= q->capacity) return;
    q->data[q->rear] = node;
    q->rear = (q->rear + 1) % q->capacity;
    q->size++;
}

static int simple_queue_dequeue(SimpleQueue *q) {
    int node = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    return node;
}

static bool simple_queue_empty(SimpleQueue *q) { return q->size == 0; }

static void simple_queue_free(SimpleQueue *q) {
    if (!q) return;
    free(q->data); free(q);
}


static int find_medoid_for_cluster(const Graph *g, const int *labels, int cluster_id) {
    int n = (int)g->n_nodes;

    int *cluster_nodes = malloc(n * sizeof(int));
    int cluster_size = 0;
    for (int i = 0; i < n; i++)
        if (labels[i] == cluster_id)
            cluster_nodes[cluster_size++] = i;

    if (cluster_size == 0) { free(cluster_nodes); return -1; }
    if (cluster_size == 1) { int r = cluster_nodes[0]; free(cluster_nodes); return r; }

    // Root = node with highest intra-cluster degree
    int root = cluster_nodes[0], max_degree = -1;
    for (int i = 0; i < cluster_size; i++) {
        int node = cluster_nodes[i], degree = 0;
        for (EdgeID e = graph_begin_adjacent(g, node); e != graph_end_adjacent(g, node); e++)
            if (labels[graph_to(g, e)] == cluster_id) degree++;
        if (degree > max_degree) { max_degree = degree; root = node; }
    }

    int *parent       = malloc(n * sizeof(int));
    int *subtree_size = malloc(n * sizeof(int));
    int *visit_order  = malloc(cluster_size * sizeof(int));
    bool *visited     = calloc(n, sizeof(bool));
    for (int i = 0; i < n; i++) { parent[i] = -1; subtree_size[i] = 1; }

    SimpleQueue *q = simple_queue_create(cluster_size);
    simple_queue_enqueue(q, root);
    visited[root] = true;
    parent[root]  = root;

    int visit_count = 0;
    while (!simple_queue_empty(q)) {
        int u = simple_queue_dequeue(q);
        visit_order[visit_count++] = u;
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            int v = (int)graph_to(g, e);
            if (!visited[v] && labels[v] == cluster_id) {
                visited[v] = true; parent[v] = u;
                simple_queue_enqueue(q, v);
            }
        }
    }
    simple_queue_free(q);
    free(visited);

    // Subtree sizes bottom-up
    for (int i = visit_count - 1; i >= 0; i--) {
        int u = visit_order[i], p = parent[u];
        if (p != u && p != -1) subtree_size[p] += subtree_size[u];
    }

    // Walk toward largest subtree until balanced
    int current = root, threshold = (cluster_size - 1) / 2, steps = 0;
    bool moved = true;
    while (moved) {
        moved = false;
        if (++steps > cluster_size) break;
        for (EdgeID e = graph_begin_adjacent(g, current); e != graph_end_adjacent(g, current); e++) {
            int nb = (int)graph_to(g, e);
            if (parent[nb] == current && labels[nb] == cluster_id
                && subtree_size[nb] > threshold) {
                current = nb; moved = true; break;
            }
        }
    }

    free(cluster_nodes); free(parent); free(subtree_size); free(visit_order);
    return current;
}

/**
 * Find medoids for all k clusters via tree-centroid method.
 * Returns: array of k medoid node IDs (caller must free).
 */
int* find_medoids(const Graph *g, const int *labels, int k) {
    int *medoids = malloc(k * sizeof(int));
    if (!medoids) return NULL;
    for (int c = 0; c < k; c++)
        medoids[c] = find_medoid_for_cluster(g, labels, c);
    return medoids;
}

// ============================================================================
// PART 2: Standard Silhouette — k separate unrestricted BFS passes
// ============================================================================


static int* bfs_single(const Graph *g, int source) {
    int n = (int)g->n_nodes;
    int *dist  = malloc(n * sizeof(int));
    int *queue = malloc(n * sizeof(int));
    if (!dist || !queue) { free(dist); free(queue); return NULL; }
    for (int i = 0; i < n; i++) dist[i] = -1;
    int front = 0, rear = 0;
    dist[source] = 0;
    queue[rear++] = source;
    while (front < rear) {
        int u = queue[front++];
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            int v = (int)graph_to(g, e);
            if (dist[v] == -1) { dist[v] = dist[u] + 1; queue[rear++] = v; }
        }
    }
    free(queue);
    return dist;
}


static float compute_standard(const Graph *g, const int *labels,
                               const int *medoids, int k) {
    int n = (int)g->n_nodes;

    int **dist = malloc(k * sizeof(int *));
    if (!dist) return 0.0f;
    for (int c = 0; c < k; c++)
        dist[c] = (medoids[c] >= 0) ? bfs_single(g, medoids[c]) : NULL;

    float total = 0.0f;
    int   count = 0;

    for (int i = 0; i < n; i++) {
        int own_c = labels[i];
        if (own_c < 0 || own_c >= k || !dist[own_c]) continue;
        int a = dist[own_c][i];
        if (a < 0) continue;

        int b = INT_MAX;
        for (int c = 0; c < k; c++) {
            if (c == own_c || !dist[c]) continue;
            int d = dist[c][i];
            if (d >= 0 && d < b) b = d;
        }
        if (b == INT_MAX) continue;

        float fa = (float)a, fb = (float)b, max_ab = fmaxf(fa, fb);
        total += (max_ab == 0.0f) ? 0.0f : (fb - fa) / max_ab;
        count++;
    }

    for (int c = 0; c < k; c++) free(dist[c]);
    free(dist);
    return (count > 0) ? total / (float)count : 0.0f;
}

// ============================================================================
// PART 3: Old Fast Silhouette — multi-source BFS with own/foreign lanes
// ============================================================================


static float compute_fast(const Graph *g, const int *labels,
                           const int *medoids, int k) {
    int n = (int)g->n_nodes;

    int  *own_dist        = malloc(n * sizeof(int));
    int  *foreign_dist    = malloc(n * sizeof(int));
    bool *own_reached     = calloc(n, sizeof(bool));
    bool *foreign_reached = calloc(n, sizeof(bool));

    if (!own_dist || !foreign_dist || !own_reached || !foreign_reached) {
        free(own_dist); free(foreign_dist);
        free(own_reached); free(foreign_reached);
        return 0.0f;
    }

    for (int i = 0; i < n; i++) { own_dist[i] = -1; foreign_dist[i] = -1; }

    BFSQueue *q = bfs_queue_create(2 * n);
    if (!q) {
        free(own_dist); free(foreign_dist);
        free(own_reached); free(foreign_reached);
        return 0.0f;
    }

    for (int c = 0; c < k; c++) {
        int m = medoids[c];
        if (m < 0 || m >= n) continue;
        own_dist[m] = 0; own_reached[m] = true;
        bfs_queue_enqueue(q, m, c, 0);
    }

    while (!bfs_queue_empty(q)) {
        int u, source, dist;
        bfs_queue_dequeue(q, &u, &source, &dist);
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            int v = (int)graph_to(g, e), nd = dist + 1;
            if (source == labels[v]) {
                if (!own_reached[v]) {
                    own_dist[v] = nd; own_reached[v] = true;
                    bfs_queue_enqueue(q, v, source, nd);
                }
            } else {
                if (!foreign_reached[v]) {
                    foreign_dist[v] = nd; foreign_reached[v] = true;
                    bfs_queue_enqueue(q, v, source, nd);
                }
            }
        }
    }
    bfs_queue_free(q);

    float total = 0.0f;
    int   count = 0;
    for (int i = 0; i < n; i++) {
        if (own_dist[i] < 0 || foreign_dist[i] < 0) continue;
        float fa = (float)own_dist[i], fb = (float)foreign_dist[i];
        float max_ab = fmaxf(fa, fb);
        total += (max_ab == 0.0f) ? 0.0f : (fb - fa) / max_ab;
        count++;
    }

    free(own_dist); free(foreign_dist);
    free(own_reached); free(foreign_reached);
    return (count > 0) ? total / (float)count : 0.0f;
}

// ============================================================================
// PART 4: Corrected Silhouette — separate own distances + nearest-two BFS
// ============================================================================


static int* compute_own_distances(const Graph *g, const int *labels,
                                   const int *medoids, int k) {
    int n = (int)g->n_nodes;
    int *own_dist = malloc(n * sizeof(int));
    if (!own_dist) return NULL;
    for (int i = 0; i < n; i++) own_dist[i] = -1;

    for (int c = 0; c < k; c++) {
        if (medoids[c] < 0) continue;
        int *dist = bfs_single(g, medoids[c]);
        if (!dist) continue;
        for (int i = 0; i < n; i++)
            if (labels[i] == c) own_dist[i] = dist[i];
        free(dist);
    }
    return own_dist;
}


static float compute_corrected(const Graph *g, const int *labels,
                                const int *medoids, int k) {
    int n = (int)g->n_nodes;

    int *own_dist = compute_own_distances(g, labels, medoids, k);
    if (!own_dist) return 0.0f;

    int *first_src   = malloc(n * sizeof(int));
    int *first_dist  = malloc(n * sizeof(int));
    int *second_src  = malloc(n * sizeof(int));
    int *second_dist = malloc(n * sizeof(int));

    if (!first_src || !first_dist || !second_src || !second_dist) {
        free(own_dist);
        free(first_src); free(first_dist);
        free(second_src); free(second_dist);
        return 0.0f;
    }

    for (int i = 0; i < n; i++) {
        first_src[i]  = -1; first_dist[i]  = -1;
        second_src[i] = -1; second_dist[i] = -1;
    }

    BFSQueue *q = bfs_queue_create(2 * n);
    if (!q) {
        free(own_dist);
        free(first_src); free(first_dist);
        free(second_src); free(second_dist);
        return 0.0f;
    }

    for (int c = 0; c < k; c++) {
        int m = medoids[c];
        if (m < 0 || m >= n) continue;
        first_src[m] = c; first_dist[m] = 0;
        bfs_queue_enqueue(q, m, c, 0);
    }

    while (!bfs_queue_empty(q)) {
        int u, source, dist;
        bfs_queue_dequeue(q, &u, &source, &dist);
        for (EdgeID e = graph_begin_adjacent(g, u); e != graph_end_adjacent(g, u); e++) {
            int v = (int)graph_to(g, e), nd = dist + 1;
            if (first_src[v] == -1) {
                first_src[v] = source; first_dist[v] = nd;
                bfs_queue_enqueue(q, v, source, nd);
            } else if (second_src[v] == -1 && first_src[v] != source) {
                second_src[v] = source; second_dist[v] = nd;
                bfs_queue_enqueue(q, v, source, nd);
            }
        }
    }
    bfs_queue_free(q);

    float total = 0.0f;
    int   count = 0;

    for (int i = 0; i < n; i++) {
        int own_c = labels[i];
        if (own_c < 0 || own_c >= k) continue;
        int a = own_dist[i];
        if (a < 0) continue;

        int b = -1;
        if      (first_src[i]  != -1 && first_src[i]  != own_c) b = first_dist[i];
        else if (second_src[i] != -1 && second_src[i] != own_c) b = second_dist[i];
        if (b < 0) continue;

        float fa = (float)a, fb = (float)b, max_ab = fmaxf(fa, fb);
        total += (max_ab == 0.0f) ? 0.0f : (fb - fa) / max_ab;
        count++;
    }

    free(own_dist);
    free(first_src); free(first_dist);
    free(second_src); free(second_dist);
    return (count > 0) ? total / (float)count : 0.0f;
}


float evaluate_silhouette(const Graph *g, const int *labels, int k) {
    int *medoids = find_medoids(g, labels, k);
    if (!medoids) return 0.0f;
    float score = compute_corrected(g, labels, medoids, k);
    free(medoids);
    return score;
}

float evaluate_silhouette_standard_with_medoids(const Graph *g, const int *labels,
                                                 const int *medoids, int k) {
    return compute_standard(g, labels, medoids, k);
}

float evaluate_silhouette_fast_with_medoids(const Graph *g, const int *labels,
                                             const int *medoids, int k) {
    return compute_fast(g, labels, medoids, k);
}

float evaluate_silhouette_corrected_with_medoids(const Graph *g, const int *labels,
                                                  const int *medoids, int k) {
    return compute_corrected(g, labels, medoids, k);
}