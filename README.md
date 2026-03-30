# Graph Clustering Benchmark

Benchmarking framework for comparing graph clustering algorithms. 
Implements Louvain, Label Propagation, Infomap, K-Medoids, and XCut on real-world networks and evaluates results using 
modularity, silhouette score, normalized cut, and coverage. Written in 
C with a C++ wrapper for XCut.

## Build

```bash
cmake -S . -B build
cmake --build build

# Run all algorithms on all graphs in the data/ directory
./build/benchmark --all

# Run all algorithms on a single graph
./build/benchmark graph.mtx

# Run a single algorithm
./build/benchmark graph.mtx -a louvain
