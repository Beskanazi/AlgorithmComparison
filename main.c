#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "io/graph.h"
#include "algorithms/clustering.h"
#include "algorithms/louvain.h"
#include "algorithms/label_prop.h"
#include "algorithms/infomap.h"
#include "algorithms/kmedoids.h"
#include "algorithms/xcut.h"
#include "metrics/modularity.h"
#include "metrics/silhouette.h"
#include "metrics/ncut.h"
#include "metrics/extra_metrics.h"


typedef struct {
    char   algorithm[32];
    int    k;
    double modularity;
    double silhouette;
    double ncut;
    double coverage;
    double density;
    double runtime_ms;
    ClusterStats cluster_stats;
} AlgorithmResult;


typedef struct {
    const char *graph_file;
    const char *data_dir;
    const char *algorithm;
    const char *output_file;
    int         k;
    int         seed;
    int         trials;
    int         max_iter;
    double      resolution;
    bool        find_k;
    bool        run_all;
} Config;

static void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s <graph_file> [options]       # single graph\n", prog);
    printf("  %s --all [data_dir] [options]   # all graphs in data_dir (default: ./data)\n\n", prog);
    printf("Options:\n");
    printf("  -a <n>   Algorithm: all, louvain, label_prop, infomap, kmedoids, xcut (default: all)\n");
    printf("  -k <n>      Clusters for kmedoids/xcut (default: auto)\n");
    printf("  -s <n>      Random seed (default: 42)\n");
    printf("  -t <n>      Infomap trials (default: 10)\n");
    printf("  -i <n>      K-Medoids max iterations (default: 100)\n");
    printf("  -r <f>      Louvain resolution (default: 1.0)\n");
    printf("  -o <file>   Output CSV — single graph mode only\n");
    printf("  --find-k    Auto-search best k\n");
    printf("  -h          Show this help\n");
}

static Config parse_args(int argc, char *argv[]) {
    Config cfg = {
        .graph_file  = NULL,
        .data_dir    = NULL,
        .algorithm   = "all",
        .output_file = NULL,
        .k           = -1,
        .seed        = 42,
        .trials      = 10,
        .max_iter    = 100,
        .resolution  = 1.0,
        .find_k      = false,
        .run_all     = false,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) {
            cfg.run_all = true;
            if (i + 1 < argc && argv[i+1][0] != '-')
                cfg.data_dir = argv[++i];
            else
                cfg.data_dir = "data";
        }
        else if (argv[i][0] != '-') {
            if (!cfg.graph_file) cfg.graph_file = argv[i];
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            { print_usage(argv[0]); exit(0); }
        else if (strcmp(argv[i], "-a") == 0 && i+1 < argc) cfg.algorithm   = argv[++i];
        else if (strcmp(argv[i], "-k") == 0 && i+1 < argc) cfg.k           = atoi(argv[++i]);
        else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) cfg.seed        = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) cfg.trials      = atoi(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0 && i+1 < argc) cfg.max_iter    = atoi(argv[++i]);
        else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) cfg.resolution  = atof(argv[++i]);
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) cfg.output_file = argv[++i];
        else if (strcmp(argv[i], "--find-k") == 0)          cfg.find_k      = true;
        else { printf("Unknown option: %s\n", argv[i]); print_usage(argv[0]); exit(1); }
    }

    return cfg;
}

static bool should_run(const char *algo, const char *selected) {
    return strcmp(selected, "all") == 0 || strcmp(selected, algo) == 0;
}


typedef struct {
    char **paths;
    int    count;
    int    capacity;
} FileList;

static FileList filelist_create(void) {
    FileList fl = { .paths = malloc(64 * sizeof(char*)), .count = 0, .capacity = 64 };
    return fl;
}

static void filelist_add(FileList *fl, const char *path) {
    if (fl->count >= fl->capacity) {
        fl->capacity *= 2;
        fl->paths = realloc(fl->paths, fl->capacity * sizeof(char*));
    }
    fl->paths[fl->count++] = strdup(path);
}

static void filelist_free(FileList *fl) {
    for (int i = 0; i < fl->count; i++) free(fl->paths[i]);
    free(fl->paths);
}

static bool has_graph_extension(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return false;
    return strcmp(ext, ".mtx") == 0 || strcmp(ext, ".edges") == 0;
}

static void collect_graphs(const char *dir, FileList *fl) {
    DIR *d = opendir(dir);
    if (!d) { fprintf(stderr, "Warning: cannot open directory %s\n", dir); return; }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode))
            collect_graphs(path, fl);
        else if (S_ISREG(st.st_mode) && has_graph_extension(entry->d_name))
            filelist_add(fl, path);
    }

    closedir(d);
}


static void ensure_dir(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) mkdir(path, 0755);
}

static void separator(void) {
    printf("────────────────────────────────────────────────────────────────\n");
}

static AlgorithmResult failed_result(const char *name) {
    AlgorithmResult r = {0};
    strncpy(r.algorithm, name, sizeof(r.algorithm) - 1);
    r.modularity = r.silhouette = r.ncut = -999.0;
    return r;
}

static void fill_metrics(AlgorithmResult *r, const Graph *g,
                         ClusterResult *cr, double resolution) {
    r->k             = cr->num_clusters;
    r->runtime_ms    = cr->runtime_ms;
    r->modularity    = compute_modularity_igraph(g, cr->labels, resolution);
    r->silhouette    = evaluate_silhouette(g, cr->labels, cr->num_clusters);
    r->ncut          = compute_ncut(g, cr->labels);
    r->coverage      = compute_coverage(g, cr->labels);
    r->density       = compute_density(g, cr->labels);
    r->cluster_stats = compute_cluster_stats(cr->labels, g->n_nodes, cr->num_clusters);
}

static void save_partition(const char *graph_base, const char *algorithm,
                            const int *labels, size_t n_nodes) {
    char algo_slug[32];
    strncpy(algo_slug, algorithm, sizeof(algo_slug) - 1);
    for (int i = 0; algo_slug[i]; i++) {
        algo_slug[i] = (char)tolower((unsigned char)algo_slug[i]);
        if (algo_slug[i] == ' ') algo_slug[i] = '_';
    }

    char path[512];
    snprintf(path, sizeof(path), "results/partitions/%s_%s.txt", graph_base, algo_slug);

    FILE *f = fopen(path, "w");
    if (!f) return;
    for (size_t i = 0; i < n_nodes; i++)
        fprintf(f, "%zu %d\n", i + 1, labels[i]);
    fclose(f);
}


static int find_best_k(const Graph *g, int seed, bool use_xcut, int search_iters, int louvain_k) {
    // Use Louvain's k as anchor for search range
    int anchor = louvain_k > 0 ? louvain_k : (int)sqrt((double)g->n_nodes);
    int min_k  = anchor / 2 > 2 ? anchor / 2 : 2;
    int max_k  = anchor * 3 / 2;
    int step   = (max_k - min_k) > 10 ? (max_k - min_k) / 5 : 2;
    if (step < 1) step = 1;

    // Scale search iterations for K-Medoids
    if      (g->n_nodes > 10000) search_iters = 3;
    else if (g->n_nodes > 5000)  search_iters = 5;
    else if (g->n_nodes > 2000)  search_iters = 10;

    printf("  Searching best k (%s) in [%d, %d] step %d (anchor=%d)...\n",
           use_xcut ? "XCut" : "K-Medoids", min_k, max_k, step, anchor);

    int    best_k    = min_k;
    double best_score = use_xcut ? 1e9 : -1e9;

    for (int k = min_k; k <= max_k; k += step) {
        printf("    k=%d...\r", k);
        fflush(stdout);

        ClusterResult *cr = use_xcut
            ? xcut_cluster(g, k, seed)
            : kmedoids_cluster(g, k, search_iters, seed);
        if (!cr) continue;

        if (use_xcut) {
            double score = compute_ncut(g, cr->labels);
            if (score < best_score) { best_score = score; best_k = k; }
        } else {
            double score = evaluate_silhouette(g, cr->labels, k);
            if (score > best_score) { best_score = score; best_k = k; }
        }
        free_cluster_result(cr);
    }

    printf("  → Best k = %d (%s = %.4f)\n\n", best_k,
           use_xcut ? "ncut" : "silhouette", best_score);
    return best_k;
}



// ============================================================================
// Algorithm Runners
// ============================================================================

static AlgorithmResult run_louvain(const Graph *g, const char *base, double resolution) {
    printf("  [Louvain]      resolution=%.2f\n", resolution);
    AlgorithmResult r = {0};
    strcpy(r.algorithm, "Louvain");
    ClusterResult *cr = louvain_cluster_igraph(g, resolution);
    if (!cr) return failed_result("Louvain");
    fill_metrics(&r, g, cr, resolution);
    save_partition(base, "Louvain", cr->labels, g->n_nodes);
    free_cluster_result(cr);
    return r;
}

static AlgorithmResult run_label_prop(const Graph *g, const char *base) {
    printf("  [Label Prop]\n");
    AlgorithmResult r = {0};
    strcpy(r.algorithm, "Label Prop");
    ClusterResult *cr = label_prop_cluster_igraph(g);
    if (!cr) return failed_result("Label Prop");
    fill_metrics(&r, g, cr, 1.0);
    save_partition(base, "Label Prop", cr->labels, g->n_nodes);
    free_cluster_result(cr);
    return r;
}

static AlgorithmResult run_infomap(const Graph *g, const char *base, int trials, int seed) {
    printf("  [Infomap]      trials=%d seed=%d\n", trials, seed);
    AlgorithmResult r = {0};
    strcpy(r.algorithm, "Infomap");
    ClusterResult *cr = infomap_cluster(g, trials, seed);
    if (!cr) return failed_result("Infomap");
    fill_metrics(&r, g, cr, 1.0);
    save_partition(base, "Infomap", cr->labels, g->n_nodes);
    free_cluster_result(cr);
    return r;
}

static AlgorithmResult run_kmedoids(const Graph *g, const char *base,
                                    int k, int max_iter, int seed) {
    printf("  [K-Medoids]    k=%d max_iter=%d seed=%d\n", k, max_iter, seed);
    AlgorithmResult r = {0};
    strcpy(r.algorithm, "K-Medoids");
    ClusterResult *cr = kmedoids_cluster(g, k, max_iter, seed);
    if (!cr) return failed_result("K-Medoids");
    fill_metrics(&r, g, cr, 1.0);
    save_partition(base, "K-Medoids", cr->labels, g->n_nodes);
    free_cluster_result(cr);
    return r;
}

static AlgorithmResult run_xcut(const Graph *g, const char *base, int k, int seed) {
    printf("  [XCut]         k=%d seed=%d\n", k, seed);
    AlgorithmResult r = {0};
    strcpy(r.algorithm, "XCut");
    ClusterResult *cr = xcut_cluster(g, k, seed);
    if (!cr) return failed_result("XCut");
    fill_metrics(&r, g, cr, 1.0);
    save_partition(base, "XCut", cr->labels, g->n_nodes);
    free_cluster_result(cr);
    return r;
}

// ============================================================================
// Output
// ============================================================================

static void print_summary(AlgorithmResult *results, int count) {
    printf("\n");
    separator();
    printf("  %-12s │ %-5s │ %-7s │ %-7s │ %-7s │ %-6s │ %s\n",
           "Algorithm", "k", "Q", "Sil", "NCut", "Cov", "Runtime");
    separator();
    for (int i = 0; i < count; i++) {
        AlgorithmResult *r = &results[i];
        if (r->modularity <= -999.0) continue;
        printf("  %-12s │ %-5d │ %7.4f │ %7.4f │ %7.4f │ %6.3f │ %.2fms\n",
               r->algorithm, r->k,
               r->modularity, r->silhouette, r->ncut,
               r->coverage, r->runtime_ms);
    }
    separator();
}


static void save_csv(const char *filename, const char *graph_name,
                     size_t n_nodes, size_t n_edges,
                     AlgorithmResult *results, int count) {
    bool exists = (access(filename, F_OK) == 0);
    FILE *f = fopen(filename, exists ? "a" : "w");
    if (!f) { printf("Error: could not write %s\n", filename); return; }

    if (!exists)
        fprintf(f, "graph,n_nodes,n_edges,algorithm,k,modularity,silhouette,ncut,coverage,"
           "density,avg_size,min_size,max_size,size_stddev,singletons,runtime_ms\n");


    for (int i = 0; i < count; i++) {
        AlgorithmResult *r = &results[i];
        if (r->modularity <= -999.0) continue;
        fprintf(f, "%s,%zu,%zu,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.2f,%d,%d,%.2f,%d,%.2f\n",
        graph_name, n_nodes, n_edges, r->algorithm, r->k,
        r->modularity, r->silhouette, r->ncut,
        r->coverage, r->density,
        r->cluster_stats.avg_size, r->cluster_stats.min_size,
        r->cluster_stats.max_size, r->cluster_stats.stddev,
        r->cluster_stats.singletons, r->runtime_ms);

    }

    fclose(f);
}


static void run_graph(const char *graph_file, const char *csv_out, const Config *cfg) {
    const char *slash = strrchr(graph_file, '/');
    const char *fname = slash ? slash + 1 : graph_file;
    char base[256];
    strncpy(base, fname, sizeof(base) - 1);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    separator();
    printf("  Graph : %s\n", graph_file);

    Graph *g = read_graph(graph_file);
    if (!g) { printf("  Error: failed to load — skipping.\n\n"); return; }

    printf("  Nodes : %zu  |  Edges: %zu\n\n", g->n_nodes, g->n_edges);

    AlgorithmResult results[8];
    int count = 0;
    int search_iters = g->n_nodes > 10000 ? 3 :
                       g->n_nodes > 5000  ? 5 :
                       g->n_nodes > 2000  ? 10 : 30;

    if (should_run("louvain",    cfg->algorithm)) results[count++] = run_louvain(g, base, cfg->resolution);
    if (should_run("label_prop", cfg->algorithm)) results[count++] = run_label_prop(g, base);
    if (should_run("infomap",    cfg->algorithm)) results[count++] = run_infomap(g, base, cfg->trials, cfg->seed);

    int louvain_k = 0;
    for (int i = 0; i < count; i++)
        if (strcmp(results[i].algorithm, "Louvain") == 0)
            louvain_k = results[i].k;

    if (should_run("kmedoids", cfg->algorithm)) {
        int k = (cfg->k > 0 && !cfg->find_k)
            ? cfg->k
            : find_best_k(g, cfg->seed, false, search_iters, louvain_k);

        int kmedoids_iters = g->n_nodes > 10000 ? 20 :
                     g->n_nodes > 5000  ? 30 : cfg->max_iter;
        results[count++] = run_kmedoids(g, base, k, kmedoids_iters, cfg->seed);
    }

    if (should_run("xcut", cfg->algorithm)) {
        int k = (cfg->k > 0 && !cfg->find_k)
            ? cfg->k
            : find_best_k(g, cfg->seed, true, search_iters, louvain_k);
        results[count++] = run_xcut(g, base, k, cfg->seed);
    }


    print_summary(results, count);
    save_csv(csv_out, base, g->n_nodes, g->n_edges, results, count);
    printf("  Appended to: %s\n\n", csv_out);

    free_graph(g);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    Config cfg = parse_args(argc, argv);

    const char *valid[] = {"all","louvain","label_prop","infomap","kmedoids","xcut"};
    bool valid_algo = false;
    for (int i = 0; i < 6; i++)
        if (strcmp(cfg.algorithm, valid[i]) == 0) { valid_algo = true; break; }
    if (!valid_algo) { printf("Error: unknown algorithm '%s'\n", cfg.algorithm); return 1; }

    ensure_dir("results");
    ensure_dir("results/partitions");

    //Batch mode
    if (cfg.run_all) {
        FileList fl = filelist_create();
        collect_graphs(cfg.data_dir, &fl);

        if (fl.count == 0) {
            printf("No .mtx or .edges files found in '%s'\n", cfg.data_dir);
            filelist_free(&fl);
            return 1;
        }

        printf("\n");
        separator();
        printf("  BATCH MODE — %d graph(s) in %s\n", fl.count, cfg.data_dir);
        separator();
        for (int i = 0; i < fl.count; i++)
            printf("  [%d] %s\n", i + 1, fl.paths[i]);
        printf("\n");

        const char *csv_out = "results/all_results.csv";
        remove(csv_out);  // fresh run each time

        for (int i = 0; i < fl.count; i++)
            run_graph(fl.paths[i], csv_out, &cfg);

        filelist_free(&fl);
        printf("Done. Results: %s\n\n", csv_out);
        return 0;
    }

    //Single graph mode
    if (!cfg.graph_file) {
        printf("Error: no graph file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    const char *slash = strrchr(cfg.graph_file, '/');
    const char *fname = slash ? slash + 1 : cfg.graph_file;
    char base[256];
    strncpy(base, fname, sizeof(base) - 1);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    char csv_out[512];
    if (cfg.output_file)
        strncpy(csv_out, cfg.output_file, sizeof(csv_out) - 1);
    else
        snprintf(csv_out, sizeof(csv_out), "results/%s.csv", base);

    printf("\n");
    separator();
    printf("  GRAPH CLUSTERING BENCHMARK\n");
    separator();
    printf("  File      : %s\n", cfg.graph_file);
    printf("  Algorithm : %s  |  Seed: %d\n", cfg.algorithm, cfg.seed);
    printf("  Output    : %s\n", csv_out);
    separator();

    run_graph(cfg.graph_file, csv_out, &cfg);

    printf("Done.\n");
    return 0;
}