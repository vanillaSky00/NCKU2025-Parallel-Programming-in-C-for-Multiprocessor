### `emplace_back` vs `push_back`
```c++
// construct an Ant directly inside the vector, using constructor arguments (n)
ants.emplace_back(n);

// constructs a temporary Ant
// moves it into the vector (or copies if no move)
ants.push_back(Ant(n)); 
```

1. Comparison: <br>
Without reserve(n):If $N=100$, the vector might resize at 1, 2, 4, 8, 16, 32, 64, 128. That is ~7 separate heap allocations and memory copies. <br> 
With `reserve(n)`: One single heap allocation for 100 integers.


2. Difference from `resize()` : <br>
It is important not to confuse reserve with `resize:path.resize(n)`: Changes size to $n$. It actually constructs $n$ "zero" integers immediately. If you did this, `path.push_back` would add elements starting at index $n$ (making the vector size $2n$). `path.reserve(n)`: Changes capacity to $n$ but keeps size at 0. This is perfect for the subsequent loop where you use push_back



https://blog.csdn.net/u013834525/article/details/104047635


###  `random_device`
```c++
// constructs a temporary random_device object
random_device{} 

// calls its operator() to produce a random seed
random_device{}() 
```
That seed is passed into the `mt19937` constructor So it’s like: 
“Create a random seed from the OS and seed my Mersenne Twister RNG with it."

### uniform_int_distribution
```
// Generates integers in a "closed interval [min, max]". Both min and max are inclusive.
uniform_int_distribution<int> dist(0, n - 1);

// map the random number rng to [0, n - 1]
dist(rng)
```
- `rng` is like engine and it output random number. <br>
- `dist` is like filter to accept whatever rng outputs. <br>
"Hey Distribution, here is the raw random engine (rng). Pull as many raw bits as you need from it and calculate a number that fits your specific range."



The reason for dividing by tourLength is to reward shorter (better) paths with more pheromone.Short Path (Good): If the distance is small, the denominator is small $\rightarrow$ Big Contribution.Long Path (Bad): If the distance is huge, the denominator is huge $\rightarrow$ Tiny Contribution.Concrete ExampleImagine $Q = 100$.Ant A (Smart): Finds a short path of length 10.Contribution = 1$100 / 10 = \mathbf{10.0}$ pheromone units.2Result: The trail shines brightly to attract future ants.Ant B (Dumb): Wanders around a path of length 500.Contribution = $100 / 500 = \mathbf{0.2}$ pheromone units.Result: The trail is barely visible; future ants will likely ignore it.What is Q?Q is just a scaling constant.Without $Q$, the values might be too small (e.g., $1/1000 = 0.001$) to compete with the initial pheromone levels or might vanish too fast due to evaporation. It tunes the "strength" of the pheromone signal relative to the rest of the mathematical system.