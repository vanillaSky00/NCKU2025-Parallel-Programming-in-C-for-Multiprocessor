

# Parallel TSP Solver (ACO + Hybrid Greedy)

This project implements a high-performance solver for the **Travelling Salesman Problem (TSP)** using a hybrid approach: **Ant Colony Optimization (ACO)** for high accuracy on smaller datasets and **Spatial Grid-Based Greedy Search** for speed on massive datasets ($N > 5000$).

## The Problem: TSP

**Goal:** Find the shortest possible route that visits each city exactly once and returns to the origin.

TSP is an **NP-hard** problem. As the number of cities ($N$) grows, the number of possible combinations explodes factorially:

$$\text{Combinations} = \frac{(n-1)!}{2}$$


For $N=20$, there are already $\approx 6 \times 10^{16}$ possible routes.

## Algorithm 1: Ant Colony Optimization (ACO)

Used for $N \le 5000$.
We simulate a colony of artificial ants that traverse the graph, leaving synthetic "pheromones" on good paths to guide future ants.

Key Parameters
- Ants (NUM_ANTS): The number of agents exploring the map per iteration (typically 20-50).
- Alpha ($\alpha$): Controls the importance of Pheromones (History). High $\alpha$ means ants just follow the crowd (exploitation).
- Beta ($\beta$): Controls the importance of Distance (Heuristic). High $\beta$ means ants greedily pick the closest city (exploration).**Distance** (Heuristic). High  means ants greedily pick the closest city (exploration).

### Pheromone Update Logic (The "Q" Factor)

After all ants finish a tour, they deposit pheromones based on the quality of their path.

$$\Delta \tau_{ij} = \frac{Q}{L_{tour}}$$

* **Short Path (Good):** Small   Large contribution.
* **Long Path (Bad):** Huge   Tiny contribution.

**Why Q?**
 is a scaling constant (e.g., 100.0). Without it, pheromone values might be too small (e.g., ) to effectively compete with evaporation or influence decision-making.

## Algorithm 2: Spatial Grid-Based Greedy

Used for $N > 5000$ (e.g., $N=10^6$).
Standard ACO is $O(N^2)$, which is too slow for millions of cities. We switch to a spatial hashing strategy:

1. **Grid Mapping:** Map all cities into a $1000 \times 1000$ coordinate grid.

2. **Local Search:** Instead of scanning all $N$ cities for the nearest neighbor, we only scan the local grid cell.
3. **Complexity:** Reduces from $O(N^2)$ to $O(N)$.
## Parallel Implementation

We use **Pthreads** to parallelize the ACO simulation.

### Thread Safety Strategy

Since we use a memory-optimized 1D vector for pheromones, we must prevent race conditions:

1. **Parallel Phase (Read-Only):** Threads run `selectNextCity` concurrently. Reading pheromone values is safe.
2. **Barrier:** We use `pthread_join` to ensure all ants finish their tours.
3. **Serial Phase (Write):** The `pheromoneUpdate` (evaporation & deposit) is performed serially by the main thread after the join. This ensures no two threads write to `pheromones[i]` simultaneously.

## Usage

**Compile:**

```bash
make compile

```

**Run:**

```bash
# Standard run (reads filename from stdin)
make run < data/input/001.in

# Or with custom parameters
# ./hw7 <ants> <iter> <alpha> <beta> <evap> <Q>
./hw7 30 200 1.0 2.0 0.5 100.0 < data/input/001.in
```

**Test:**
```bash
# generate random test
python3 gen_test.py

# test
python3 optimizer.py
```
## References
https://www.youtube.com/watch?v=u7bQomllcJw