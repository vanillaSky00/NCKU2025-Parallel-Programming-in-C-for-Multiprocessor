# Intro


# TSP problem
https://en.wikipedia.org/wiki/Travelling_salesman_problem
What is the shortest possible route that visits each city exactly once and returns to the origin city? It is an NP-hard problem.
```
combination number for 5 vertices:
(4 * 3 * 2 * 1) / 2

n vertices:
(n-1)! / 2
```

# Ant algorithm

how many ants are there?
what is alpha, beta doing




### Reward shorter path
- Short Path (Good): If the distance is small, the denominator is small $\rightarrow$ Big Contribution.
- Long Path (Bad): If the distance is huge, the denominator is huge $\rightarrow$ Tiny Contribution.

Concrete Example:
```c++
Q = 100

// Smart Ant finds a short path of len 10
contribution = 100 / 10

// Dumb Ant find a path of len 500
contribution = 100 / 500
```
The trail for second one is barely visible; future ants will likely ignore it.

What is $Q$ ? It is just a scaling constant. Without $Q$, the values might be too small (e.g., $1/1000 = 0.001$) to compete with the initial pheromone levels or might vanish too fast due to evaporation. It tunes the "strength" of the pheromone signal relative to the rest of the mathematical system.

# Parallel warning
Since we are now using the memory-optimized version (1D vector) but planning to add pthreads back: We must ensure that `getPheromone` is not called for writing (+=) by multiple threads at the same time.

- Reading (in `selectNextCity`): Safe to do in parallel.
- Writing (in `pheromoneUpdate`): Must be done serially (or with atomic locks), which your current structure (Serial update after pthread_join) correctly handles.