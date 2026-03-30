"""
visualize.py — Thesis plot generator for graph clustering benchmark results

Usage:
  python visualize.py bars      results/karate.csv
  python visualize.py heatmap   results/karate.csv results/dolphins.csv results/football.csv
  python visualize.py runtime   results/karate.csv results/dolphins.csv results/football.csv
  python visualize.py community data/real_small/karate.mtx results/karate_louvain_partition.txt

All figures are saved to results/figures/
"""

import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np
import networkx as nx

# ── Style ─────────────────────────────────────────────────────────────────────

COLORS = {
    "Louvain":    "#2C5F8A",
    "Label Prop": "#5C8A4A",
    "Infomap":    "#8A6A2C",
    "K-Medoids":  "#8A2C4A",
    "XCut":       "#4A4A7A",
}
DEFAULT_COLORS = list(COLORS.values())

plt.rcParams.update({
    "font.family":     "serif",
    "font.size":       11,
    "axes.titlesize":  12,
    "axes.labelsize":  11,
    "figure.dpi":      150,
})

METRICS = {
    "modularity":   ("Modularity (Q)",        True),   # (label, higher_is_better)
    "silhouette":   ("Silhouette Score",       True),
    "ncut":         ("Normalized Cut",         False),
    "coverage":     ("Coverage",               True),
}

OUTPUT_DIR = "results/figures"


def ensure_output_dir():
    os.makedirs(OUTPUT_DIR, exist_ok=True)


def load_csv(path):
    df = pd.read_csv(path)
    return df


def algo_colors(algorithms):
    return [COLORS.get(a, "#607D8B") for a in algorithms]


# ── 1. BAR CHARTS ─────────────────────────────────────────────────────────────

def plot_bars(csv_file):
    ensure_output_dir()
    df = load_csv(csv_file)

    for graph_name, group in df.groupby("graph"):
        group = group.reset_index(drop=True)

        fig, axes = plt.subplots(1, 5, figsize=(18, 5))

        for ax, (metric, (label, higher_better)) in zip(axes, METRICS.items()):
            colors = algo_colors(group["algorithm"])
            vals   = group[metric].values
            bars   = ax.bar(group["algorithm"], vals, color=colors,
                            edgecolor="white", linewidth=0.8, width=0.6)

            # Ensure y-axis includes 0 and any negative values
            ymin = min(0, np.nanmin(vals)) * 1.15
            ymax = np.nanmax(vals) * 1.15
            if ymax == 0:
                ymax = 0.1
            ax.set_ylim(ymin, ymax)
            ax.axhline(0, color="black", linewidth=0.6, linestyle="-")

            direction = "↑ higher is better" if higher_better else "↓ lower is better"
            ax.set_title(f"{label}\n{direction}", fontsize=10)
            ax.set_ylabel(label if ax == axes[0] else "")
            ax.set_xticks(range(len(group["algorithm"])))
            ax.set_xticklabels(group["algorithm"], rotation=30, ha="right", fontsize=9)
            ax.spines["top"].set_visible(False)
            ax.spines["right"].set_visible(False)

        n = int(group["n_nodes"].iloc[0])
        m = int(group["n_edges"].iloc[0])
        fig.suptitle(f"Algorithm Comparison — {graph_name}  (n={n:,}, m={m:,})",
             fontsize=14, fontweight="bold", y=1.02)
        plt.tight_layout()

        out = f"{OUTPUT_DIR}/{graph_name}_bars.pdf"
        plt.savefig(out, bbox_inches="tight")
        print(f"Saved: {out}")
        plt.close()

# ── 2. HEATMAP ────────────────────────────────────────────────────────────────

def plot_heatmap(csv_files):
    """
    One heatmap per metric.
    Rows = algorithms, columns = graphs.
    Green = best, red = worst (direction-aware).
    """
    ensure_output_dir()

    df = pd.concat([load_csv(f) for f in csv_files], ignore_index=True)

    for metric, (label, higher_better) in METRICS.items():
        pivot = df.pivot_table(index="algorithm", columns="graph",
                               values=metric, aggfunc="mean")

        fig, ax = plt.subplots(figsize=(max(5, len(pivot.columns) * 1.6), 4))

        # Flip colormap for metrics where lower is better
        cmap = "RdYlGn" if higher_better else "RdYlGn_r"
        im = ax.imshow(pivot.values, cmap=cmap, aspect="auto",
                       vmin=np.nanmin(pivot.values),
                       vmax=np.nanmax(pivot.values))

        ax.set_xticks(range(len(pivot.columns)))
        ax.set_yticks(range(len(pivot.index)))
        ax.set_xticklabels(pivot.columns, rotation=25, ha="right")
        ax.set_yticklabels(pivot.index)


        plt.colorbar(im, ax=ax, fraction=0.03, pad=0.04)
        ax.set_title(f"{label} — Algorithms × Graphs\n({'higher is better' if higher_better else 'lower is better'})",
                     fontsize=12)
        plt.tight_layout()

        out = f"{OUTPUT_DIR}/heatmap_{metric}.pdf"
        plt.savefig(out, bbox_inches="tight")
        print(f"Saved: {out}")
        plt.close()


# ── 3. RUNTIME ────────────────────────────────────────────────────────────────

def plot_runtime(csv_files):
    """
    Grouped bar chart: x-axis = graph, groups = algorithms.
    Log scale on y-axis to handle large runtime differences.
    """
    ensure_output_dir()

    df = pd.concat([load_csv(f) for f in csv_files], ignore_index=True)
    graphs     = df["graph"].unique()
    algorithms = df["algorithm"].unique()

    x     = np.arange(len(graphs))
    width = 0.15
    n     = len(algorithms)

    fig, ax = plt.subplots(figsize=(max(8, len(graphs) * 2.5), 5))

    for i, algo in enumerate(algorithms):
        vals   = [df[(df["graph"] == g) & (df["algorithm"] == algo)]["runtime_ms"].values
                  for g in graphs]
        vals   = [v[0] if len(v) > 0 else 0.0 for v in vals]
        offset = (i - n / 2 + 0.5) * width
        bars   = ax.bar(x + offset, vals, width,
                        label=algo, color=COLORS.get(algo, "#607D8B"),
                        edgecolor="white", linewidth=0.6)

    ax.set_yscale("log")
    ax.set_xticks(x)
    ax.set_xticklabels(graphs, rotation=20, ha="right")
    ax.set_ylabel("Runtime (ms) — log scale")
    ax.set_title("Runtime Comparison Across Graphs", fontsize=13, fontweight="bold")
    ax.legend(loc="upper left", framealpha=0.9)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    plt.tight_layout()

    out = f"{OUTPUT_DIR}/runtime_comparison.pdf"
    plt.savefig(out, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close()


# ── 4. COMMUNITY VISUALIZATION ────────────────────────────────────────────────

def plot_community(graph_file, partition_file):
    ensure_output_dir()

    G = nx.Graph()
    with open(graph_file) as f:
        for line in f:
            if line.startswith("%") or line.startswith("#"): continue
            # skip header lines like "node_1,node_2"
            parts = line.strip().replace(",", " ").split()
            if len(parts) >= 2:
                try:
                    u, v = int(parts[0]), int(parts[1])
                    if u != v:
                        G.add_edge(u, v)
                except ValueError:
                    continue

    if len(G.nodes()) == 0:
        print(f"Error: no nodes loaded from {graph_file}")
        return

    partition = {}
    with open(partition_file) as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) == 2:
                try:
                    node, comm = int(parts[0]), int(parts[1])
                    partition[node] = comm
                except ValueError:
                    continue

    algo_name = "Unknown"
    for algo in COLORS:
        if algo.lower().replace(" ", "_") in os.path.basename(partition_file).lower():
            algo_name = algo
            break

    graph_base = os.path.splitext(os.path.basename(graph_file))[0]
    n_comm     = len(set(partition.values()))

    # Fixed: use new colormap API
    cmap        = plt.colormaps.get_cmap("tab20")
    node_list   = list(G.nodes())
    node_colors = [cmap((partition.get(n, 0) % 20) / 20) for n in node_list]

    k_layout = 1.5 / (len(G.nodes()) ** 0.5) if len(G.nodes()) > 1 else 1.0
    pos = nx.spring_layout(G, seed=42, k=k_layout)

    fig, ax = plt.subplots(figsize=(9, 7))
    nx.draw_networkx_edges(G, pos, alpha=0.2, width=0.7, ax=ax)
    nx.draw_networkx_nodes(G, pos, nodelist=node_list,
                           node_color=node_colors, node_size=180,
                           alpha=0.95, ax=ax)
    nx.draw_networkx_labels(G, pos, font_size=6, ax=ax)

    ax.set_title(f"{algo_name} — {graph_base}  ({n_comm} communities)",
                 fontsize=13, fontweight="bold")
    ax.axis("off")
    plt.tight_layout()

    algo_slug = algo_name.lower().replace(" ", "_")
    out = f"{OUTPUT_DIR}/{graph_base}_{algo_slug}_communities.pdf"
    plt.savefig(out, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close()

def plot_algorithm_runtime(csv_file, algorithm):
    """Bar chart: runtime of one algorithm across all graphs."""
    ensure_output_dir()
    df = load_csv(csv_file)

    algo_df = df[df["algorithm"] == algorithm].copy()

    if algo_df.empty:
        print(f"No data found for algorithm '{algorithm}'")
        print(f"Available: {df['algorithm'].unique()}")
        return

    algo_df = algo_df.sort_values("runtime_ms")

    fig, ax = plt.subplots(figsize=(max(8, len(algo_df) * 1.2), 5))

    color = COLORS.get(algorithm, "#607D8B")
    bars  = ax.bar(algo_df["graph"], algo_df["runtime_ms"],
                   color=color, edgecolor="white", linewidth=0.8)

    ax.set_yscale("log")
    ax.set_ylabel("Runtime (ms) — log scale")

    for bar, val in zip(bars, algo_df["runtime_ms"]):
        ax.text(bar.get_x() + bar.get_width() / 2,
                bar.get_height() * 1.1,
                f"{val:.1f}ms", ha="center", va="bottom", fontsize=8)

    ax.set_ylabel("Runtime (ms)")
    ax.set_xlabel("Graph")
    ax.set_title(f"{algorithm} — Runtime Across Graphs", fontsize=13, fontweight="bold")
    ax.set_xticklabels(algo_df["graph"], rotation=30, ha="right")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    plt.tight_layout()

    slug = algorithm.lower().replace(" ", "_")
    out  = f"{OUTPUT_DIR}/{slug}_runtime.pdf"
    plt.savefig(out, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close()

def plot_graph_density(csv_file):
    """Bar chart comparing graph density across all graphs."""
    ensure_output_dir()
    df = load_csv(csv_file)

    graphs = df.drop_duplicates(subset="graph")[["graph", "n_nodes", "n_edges", "density"]].copy()
    graphs = graphs.sort_values("density")

    fig, ax = plt.subplots(figsize=(max(8, len(graphs) * 1.2), 5))

    bars = ax.bar(graphs["graph"], graphs["density"],
                  color="#2C5F8A", edgecolor="white", linewidth=0.8)

    for bar, val, n, m in zip(bars, graphs["density"], graphs["n_nodes"], graphs["n_edges"]):
        ax.text(bar.get_x() + bar.get_width() / 2,
                bar.get_height() * 1.3,
                f"n={int(n):,}\nm={int(m):,}\nd={val:.6f}",
                ha="center", va="bottom", fontsize=7)

    ax.set_yscale("log")
    ax.set_ylabel("Graph Density (log scale)")
    ax.set_title("Graph Density Comparison", fontsize=13, fontweight="bold")
    ax.set_xticklabels(graphs["graph"], rotation=30, ha="right")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    plt.tight_layout()

    out = f"{OUTPUT_DIR}/graph_density.pdf"
    plt.savefig(out, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close()

def plot_runtime_stacked(csv_files):
    """
    Horizontal 100% stacked bar chart — one bar per graph, segments = algorithms.
    Shows runtime share per algorithm. Graphs sorted by total runtime.
    """
    ensure_output_dir()

    df = pd.concat([load_csv(f) for f in csv_files], ignore_index=True)

    ALGOS = ["Louvain", "Label Prop", "Infomap", "K-Medoids", "XCut"]

    totals = df.groupby("graph")["runtime_ms"].sum().sort_values()
    totals = totals[totals >= 10]
    graphs = totals.index.tolist()

    fig, ax = plt.subplots(figsize=(12, max(6, len(graphs) * 0.5)))

    lefts = np.zeros(len(graphs))

    for algo in ALGOS:
        widths = []
        for g in graphs:
            row = df[(df["graph"] == g) & (df["algorithm"] == algo)]["runtime_ms"]
            val = row.values[0] if len(row) > 0 else 0
            widths.append(val / totals[g] * 100)

        bars = ax.barh(graphs, widths, left=lefts,
                       label=algo,
                       color=COLORS.get(algo, "#607D8B"),
                       edgecolor="white", linewidth=0.4)

        # Label segments >= 5%
        for bar, w, l in zip(bars, widths, lefts):
            if w >= 5:
                ax.text(l + w / 2, bar.get_y() + bar.get_height() / 2,
                        f"{w:.0f}%",
                        ha="center", va="center",
                        fontsize=7, color="white", fontweight="bold")

        lefts += widths

    ax.set_xlim(0, 100)
    ax.set_xlabel("Share of total runtime (%)")
    ax.set_title("Runtime Distribution per Graph", fontsize=12, fontweight="bold")
    ax.legend(loc="lower right", framealpha=0.9, fontsize=9)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    plt.tight_layout()

    out = f"{OUTPUT_DIR}/runtime_stacked.pdf"
    plt.savefig(out, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close()




# ── 6. GROUPED LOG-SCALE RUNTIME ─────────────────────────────────────────────

def plot_runtime_grouped_log(csv_files):
    """
    Grouped bar chart (one group per graph, one bar per algorithm).
    Y-axis is log scale so small and large graphs are both legible.
    Graphs sorted left-to-right by total runtime.
    """
    ensure_output_dir()
    import matplotlib.ticker as mticker

    df = pd.concat([load_csv(f) for f in csv_files], ignore_index=True)

    ALGOS = ["Louvain", "Label Prop", "Infomap", "K-Medoids", "XCut"]

    # Sort graphs by total runtime ascending
    totals = df.groupby("graph")["runtime_ms"].sum().sort_values()
    totals = totals[totals >= 10]
    graphs = totals.index.tolist()


    n_graphs = len(graphs)
    n_algos  = len(ALGOS)
    group_w  = 0.8                          # total width reserved per graph
    bar_w    = group_w / n_algos
    x        = np.arange(n_graphs)

    fig, ax = plt.subplots(figsize=(max(14, n_graphs * 1.1), 6))

    for i, algo in enumerate(ALGOS):
        vals   = []
        for g in graphs:
            row = df[(df["graph"] == g) & (df["algorithm"] == algo)]["runtime_ms"]
            vals.append(row.values[0] if len(row) > 0 else np.nan)

        offsets = x + (i - n_algos / 2 + 0.5) * bar_w
        bars    = ax.bar(offsets, vals, bar_w,
                         label=algo,
                         color=COLORS.get(algo, "#607D8B"),
                         edgecolor="white", linewidth=0.4,
                         zorder=3)

        # Value labels on top of each bar (only for bars tall enough to matter)
        for bar, val, g in zip(bars, vals, graphs):
            if val is None or np.isnan(val):
                continue
            pct = val / totals[g] * 100
            ms_label = f"{val:.0f}" if val >= 1 else f"{val:.2f}"
            label = f"{ms_label}ms\n{pct:.1f}%"
            ax.text(bar.get_x() + bar.get_width() / 2,
                    val * 1.15,
                    label,
                    ha="center", va="bottom",
                    fontsize=6, color="#444444", rotation=90)


# Dividers between graph groups
    for xi in x[:-1]:
        ax.axvline(xi + 0.5, color="#dddddd", linewidth=0.6, linestyle="--", zorder=2)

    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(
        lambda v, _: f"{v:.0f}ms" if v >= 1 else f"{v:.2f}ms"
    ))
    ax.set_xticks(x)
    ax.set_xticklabels(graphs, rotation=25, ha="right", fontsize=9)
    ax.set_ylabel("Runtime (ms) — log scale")
    ax.set_title("Algorithm Runtime per Graph  (log scale, sorted by total runtime)",
                 fontsize=12, fontweight="bold")
    ax.legend(loc="upper left", framealpha=0.9, fontsize=9)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(axis="y", which="both", color="#eeeeee", linewidth=0.5, zorder=1)
    plt.tight_layout()

    out = f"{OUTPUT_DIR}/runtime_grouped_log.pdf"
    plt.savefig(out, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close()

def plot_metric_bars(csv_files):
    """
    One grouped bar chart per metric.
    X-axis = graphs, bars = algorithms, y-axis = metric value.
    """
    ensure_output_dir()

    df = pd.concat([load_csv(f) for f in csv_files], ignore_index=True)

    ALGOS = ["Louvain", "Label Prop", "Infomap", "K-Medoids", "XCut"]

    # Sort graphs by number of nodes
    graph_order = df.drop_duplicates("graph").sort_values("n_nodes")["graph"].tolist()

    n_graphs = len(graph_order)
    n_algos = len(ALGOS)
    group_w = 0.8
    bar_w = group_w / n_algos
    x = np.arange(n_graphs)

    for metric, (label, higher_better) in METRICS.items():
        fig, ax = plt.subplots(figsize=(max(14, n_graphs * 1.1), 6))

        for i, algo in enumerate(ALGOS):
            vals = []
            for g in graph_order:
                row = df[(df["graph"] == g) & (df["algorithm"] == algo)][metric]
                vals.append(row.values[0] if len(row) > 0 else np.nan)

            offsets = x + (i - n_algos / 2 + 0.5) * bar_w
            bars = ax.bar(offsets, vals, bar_w,
                          label=algo,
                          color=COLORS.get(algo, "#607D8B"),
                          edgecolor="white", linewidth=0.4,
                          zorder=3)

            for bar, val in zip(bars, vals):
                if val is None or np.isnan(val):
                    continue
                ax.text(bar.get_x() + bar.get_width() / 2,
                        val + (abs(val) * 0.05 if val >= 0 else -abs(val) * 0.05),
                        f"{val:.3f}",
                        ha="center", va="bottom" if val >= 0 else "top",
                        fontsize=5, color="#444444", rotation=90)

        direction = "↑ higher is better" if higher_better else "↓ lower is better"
        ax.set_xticks(x)
        ax.set_xticklabels(graph_order, rotation=25, ha="right", fontsize=9)
        ax.set_ylabel(label)
        ax.set_title(f"{label} per Graph  ({direction})",
                     fontsize=12, fontweight="bold")
        ax.legend(loc="best", framealpha=0.9, fontsize=9)
        ax.axhline(0, color="black", linewidth=0.5)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.grid(axis="y", color="#eeeeee", linewidth=0.5, zorder=1)
        plt.tight_layout()

        out = f"{OUTPUT_DIR}/metric_{metric}.pdf"
        plt.savefig(out, bbox_inches="tight")
        print(f"Saved: {out}")
        plt.close()



# ── Entry point ───────────────────────────────────────────────────────────────

USAGE = """
Usage:
  python visualize.py bars        results/karate.csv
  python visualize.py heatmap     results/karate.csv results/dolphins.csv ...
  python visualize.py runtime     results/karate.csv results/dolphins.csv ...
  python visualize.py runtimelog  results/all_results.csv
  python visualize.py community   data/real_small/karate.mtx results/karate_louvain_partition.txt
  python visualize.py algotime    results/all_results.csv Louvain
"""

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(USAGE)
        sys.exit(1)

    mode = sys.argv[1]


    if mode == "bars":
        for csv in sys.argv[2:]:
            plot_bars(csv)

    elif mode == "heatmap":
        plot_heatmap(sys.argv[2:])

    elif mode == "runtime":
        plot_runtime(sys.argv[2:])

    elif mode == "runtimelog":
        plot_runtime_grouped_log(sys.argv[2:])

    elif mode == "community":
        if len(sys.argv) < 4:
            print("community mode needs: <graph_file> <partition_file>")
            sys.exit(1)
        plot_community(sys.argv[2], sys.argv[3])

    elif mode == "algotime":
        if len(sys.argv) < 4:
            print("Usage: python visualize.py algotime results/all_results.csv <AlgorithmName>")
            sys.exit(1)
        plot_algorithm_runtime(sys.argv[2], sys.argv[3])

    elif mode == "density":
        plot_graph_density(sys.argv[2])

    elif mode == "stacked":
        plot_runtime_stacked(sys.argv[2:])

    elif mode == "metrics":
        plot_metric_bars(sys.argv[2:])



    else:
        print(f"Unknown mode: {mode}")
        print(USAGE)
        sys.exit(1)