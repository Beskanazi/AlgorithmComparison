#include "../../include/io/graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void log_info(const char *msg) {
    printf("[INFO] %s\n", msg);
}

static void log_error(const char *msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
}

static Graph* graph_create_from_edges(NodeID n, Edge *edges, EdgeID n_edges, bool directed) {
    Graph *g = calloc(1, sizeof(Graph));
    if (!g) return NULL;

    g->n_nodes  = n;
    g->n_edges  = n_edges;
    g->edges    = edges;
    g->directed = directed;

    size_t adj_size = directed ? n_edges : 2 * n_edges;

    g->starts    = calloc(n + 1, sizeof(EdgeID));
    g->adjacency = malloc(adj_size * sizeof(NodeID));

    if (!g->starts || !g->adjacency) {
        free_graph(g);
        return NULL;
    }

    // Count degrees
    for (EdgeID i = 0; i < n_edges; i++) {
        g->starts[edges[i].src + 1]++;
        if (!directed)
            g->starts[edges[i].dst + 1]++;
    }

    // Prefix sum
    for (NodeID i = 1; i <= n; i++)
        g->starts[i] += g->starts[i - 1];

    // Fill adjacency
    EdgeID *offset = calloc(n, sizeof(EdgeID));
    if (!offset) { free_graph(g); return NULL; }

    for (EdgeID i = 0; i < n_edges; i++) {
        NodeID u = edges[i].src;
        NodeID v = edges[i].dst;

        g->adjacency[g->starts[u] + offset[u]++] = v;
        if (!directed)
            g->adjacency[g->starts[v] + offset[v]++] = u;
    }

    free(offset);
    return g;
}


Graph* read_graph_mtx(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) { log_error("Failed to open file."); return NULL; }

    char line[1024];

    // Skip comment lines starting with %
    while (fgets(line, sizeof(line), file)) {
        if (line[0] != '%') break;
    }

    // Parse header: n1 n2 m
    NodeID n1, n2;
    EdgeID m;
    if (sscanf(line, "%zu %zu %zu", &n1, &n2, &m) != 3) {
        log_error("File is malformed.");
        fclose(file);
        return NULL;
    }

    if (n1 != n2) {
        log_error("Rectangular matrix. only square adjacency matrices supported.");
        fclose(file);
        return NULL;
    }

    size_t cap = m > 0 ? m : 1024;
    Edge *edges = malloc(cap * sizeof(Edge));
    EdgeID edge_count = 0;

    if (!edges) { log_error("Failed to allocate edges."); fclose(file); return NULL; }

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '%') continue;

        NodeID u, v;
        if (sscanf(line, "%zu %zu", &u, &v) < 2) {
            log_error("Malformed edge line.");
            free(edges); fclose(file);
            return NULL;
        }

        if (edge_count == cap) {
            cap *= 2;
            edges = realloc(edges, cap * sizeof(Edge));
            if (!edges) { fclose(file); return NULL; }
        }

        // 1-indexed to 0-indexed
        edges[edge_count].src = u - 1;
        edges[edge_count].dst = v - 1;
        edge_count++;
    }

    fclose(file);

    edges = realloc(edges, edge_count * sizeof(Edge));

    char msg[64];
    snprintf(msg, sizeof(msg), "Read %zu edges.", edge_count);
    log_info(msg);

    return graph_create_from_edges(n1, edges, edge_count, false);
}

Graph* read_graph_edge_list(const char *filename, bool directed) {
    FILE *file = fopen(filename, "r");
    if (!file) { log_error("Failed to open file."); return NULL; }

    char line[1024];
    size_t cap = 1024;
    Edge *edges = malloc(cap * sizeof(Edge));
    EdgeID edge_count = 0;
    NodeID max_node = 0;
    NodeID min_node = (NodeID)-1;

    if (!edges) { log_error("Failed to allocate edges."); fclose(file); return NULL; }

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '%' || line[0] == '\n') continue;
        if (!isdigit(line[0])) continue;

        // Support comma-separated
        for (char *p = line; *p; p++) {
            if (*p == ',') *p = ' ';
        }

        NodeID u, v;
        if (sscanf(line, "%zu %zu", &u, &v) < 2) {
            log_error("Malformed edge line.");
            free(edges); fclose(file);
            return NULL;
        }

        if (u < min_node) min_node = u;
        if (v < min_node) min_node = v;

        if (edge_count == cap) {
            cap *= 2;
            edges = realloc(edges, cap * sizeof(Edge));
            if (!edges) { fclose(file); return NULL; }
        }

        edges[edge_count].src = u;
        edges[edge_count].dst = v;
        if (u > max_node) max_node = u;
        if (v > max_node) max_node = v;
        edge_count++;
    }

    fclose(file);

    // Convert 1-indexed to 0-indexed if needed
    if (min_node == 1) {
        log_info("Detected 1-indexed graph, converting to 0-indexed.");
        for (EdgeID i = 0; i < edge_count; i++) {
            edges[i].src -= 1;
            edges[i].dst -= 1;
        }
        max_node -= 1;
    }

    edges = realloc(edges, edge_count * sizeof(Edge));

    char msg[64];
    snprintf(msg, sizeof(msg), "Read %zu edges.", edge_count);
    log_info(msg);

    return graph_create_from_edges(max_node + 1, edges, edge_count, directed);
}

Graph* read_graph(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) { log_error("File format not recognized."); return NULL; }

    if (strcmp(ext, ".mtx")   == 0) return read_graph_mtx(filename);
    if (strcmp(ext, ".edges") == 0) return read_graph_edge_list(filename, false);

    log_error("File format not recognized.");
    return NULL;
}

// ============================================================================
// Accessors
// ============================================================================

EdgeID   graph_begin_adjacent(const Graph *g, NodeID u) { return g->starts[u]; }
EdgeID   graph_end_adjacent  (const Graph *g, NodeID u) { return g->starts[u + 1]; }
NodeID   graph_to            (const Graph *g, EdgeID e) { return g->adjacency[e]; }
size_t   graph_n_nodes       (const Graph *g)           { return g->n_nodes; }

void free_graph(Graph *g) {
    if (!g) return;
    free(g->starts);
    free(g->adjacency);
    free(g->edges);
    free(g);
}