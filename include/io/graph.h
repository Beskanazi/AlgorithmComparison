#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>
#include <stdbool.h>

typedef size_t NodeID;
typedef size_t EdgeID;

typedef struct {
    NodeID src;
    NodeID dst;
} Edge;

typedef struct {
    NodeID  n_nodes;
    EdgeID  n_edges;
    Edge   *edges;
    bool    directed;

    // CSR adjacency structure (built from edges)
    EdgeID *starts;      // [n_nodes + 1]
    NodeID *adjacency;   // [2 * n_edges] for undirected
} Graph;

// Main reader (auto-detects by extension: .mtx, .edges)
Graph* read_graph(const char *filename);

// Format-specific readers
Graph* read_graph_mtx(const char *filename);
Graph* read_graph_edge_list(const char *filename, bool directed);

// Adjacency iteration
EdgeID graph_begin_adjacent(const Graph *g, NodeID u);
EdgeID graph_end_adjacent  (const Graph *g, NodeID u);
NodeID graph_to            (const Graph *g, EdgeID e);
size_t graph_n_nodes       (const Graph *g);

// Cleanup
void free_graph(Graph *g);

#endif // GRAPH_H