## Overall
Below documents implementation notes and debugging summary.

This project computes the function

$$
S(n) = \sum_{k=1}^{n} k \cdot \left\lfloor \frac{n}{k} \right\rfloor
$$

efficiently in  $O(\sqrt{n})$ using the hyperbola method and parallelizes the computation across four MPI processes.

## Parallelization Structure
| MPI Rank | Responsibility           | Range                        |
| -------- | ------------------------ | ---------------------------- |
| 0        | First loop (lower half)  | $i \in [1, \lfloor \sqrt{n}/2 \rfloor]$          |
| 1        | First loop (upper half)  | $i \in [\lfloor \sqrt{n}/2 \rfloor + 1, \lfloor \sqrt{n} \rfloor]$   |
| 2        | Second loop (lower half) | $i \in [1, (\text{last} - 1)/2]$        |
| 3        | Second loop (upper half) | $i \in [(\text{last} - 1)/2 + 1, \text{last} - 1]$ |


## Modular Arithmetic
Addition and multiplication are compatible with modulo

| Operation      | Mod rule                                                   | Works under mod?           |
| -------------- | ---------------------------------------------------------- | -------------------------- |
| Addition       | $(a + b) \bmod m = ((a \bmod m) + (b \bmod m)) \bmod m$            | ✅                          |
| Subtraction    | $(a - b) \bmod m = ((a \bmod m) - (b \bmod m)) \bmod m$            | ✅                          |
| Multiplication | $(a \times b) \bmod m = ((a \bmod m) \times (b \bmod m)) \bmod m$ | ✅                          |
| Division       | ❌ Not directly valid                                       | ❌ must use modular inverse |

By extension, we have: <br>
$(a + b + c) \bmod m = (((a + b) \bmod m) + c) \bmod m$

### Division Under Modulo

Division under a modulus must be replaced by multiplication with the modular inverse.
The modular inverse $ b^{-1} $ is defined as:
$$
b \times b^{-1} \equiv 1 \pmod{m}
$$
Hence, division becomes:

$$
\frac{a}{b} \bmod m = (a \times b^{-1}) \bmod m
$$

If $ m $ is prime, we can compute $ b^{-1} $ using $\textbf{Fermat's Little Theorem}$:

$$b^{m-1} \equiv 1 \pmod{m} \quad \Rightarrow \quad b^{m-2} \equiv b^{-1} \pmod{m}$$


Thus,

$$\frac{a}{b} \bmod m = (a \times b^{m-2}) \bmod m$$


Example:

$$a = 8, \quad b = 3, \quad m = 5$$

$$3^{m-2} = 3^{3} = 27 \equiv 2 \pmod{5}$$

$$\frac{8}{3} \bmod 5 = (8 \times 2) \bmod 5 = 16 \bmod 5 = 1$$

The key problem turns to find $b^{-1}$. When $b$ and $m$ are not coprime, Fermat’s theorem no longer works, so we must use the $\textbf{Extended Euclidean Algorithm (EEA)}$ to compute the modular inverse.



## Numeric Range
`long long` is $9 * 10^{18}$ while input max is $10^{18}$

Also we do not need `modpow()` to caculate each time because this problem only requires division by 2 which make INV a constant. If we do the `modpow()` inside `modDivide()` would cost nlogn
```c++
long long modPow(long long base, long long exp, long long mod);
long long modDivide(long long a, long long b, long long m);

ans += (modDivide((l + r) * (r - l + 1), 2, MOD) * i) % MOD; // ❌ unsafe
```
## Common mistakes
### 1 Wrong `MOD` and `MOD_INV`
```c++
// wrong
const long long MOD = 500000004; 

// correct 
const long long MOD = 1000000007;
const long long MOD_INV2 = 500000004; // Math.pow(2, MOD - 2) % MOD
```
We don’t need to call `modPow()` every time; dividing by 2 is constant.

### 2 Double-counting around √n
After the first loop ended at i = floor(sqrt(n)),
the second loop starts again at the same point, causing overlap.
```c++
for (long long i = last - 1; i >= 1; i--) { ... }

// In parallel
for (long long i = 1; i <= last - 1; i++) { ... }
```

### 3 Modular multiplication safety
```c++
long long temp = (( (l + r) % MOD ) * ((r - l + 1) % MOD)) % MOD;
temp = (temp * INV2) % MOD;
```

### 4 MPI: `global_result` not allocated (using `MPI_Gather`)
```c++
// wrong
long long* global_result = nullptr;
MPI_Gather(&local_result, 1, MPI_LONG_LONG, global_result, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

// correct
long long* global_result = nullptr;
if (world_rank == 0) global_result = new long long[world_size];
MPI_Gather(&local_result, 1, MPI_LONG_LONG, global_result, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
...
if (world_rank == 0) delete[] global_result;
```

### 5 Overflow in `MPI_Reduce`
`MPI_SUM` uses normal integer addition.
If intermediate sums exceed `long long`, overflow occurs.

Here, we are safe because each `local_sum < MOD`.
Just ensure modular reduction afterward:
```c++
MPI_Reduce(&local_sum, &total, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
if (world_rank == 0) total %= MOD;
```

### 6 Missing broadcast of input
```c++
MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
```

### 7 Wrong loop boundary
```c++
//wrong
for (long long i = start; i < end; i++) { ... }

//correct
for (long long i = start; i <= end; i++) { ... }
```

### 8 Wrong index ranges in trapezoid loop
```c++
//wrong
long long l = n / (i + 1);
long long r = n / i;

//correct
long long l = n / (i + 1) + 1;
long long r = n / i;
```

### 9 Mixed integer types
Avoid mixing `int` and `long long` for large ranges.
```c++
long long start = ...;
long long end = ...;
```


## Checklist
| Check                              |       |
| ---------------------------------- | ------|
| One-core output matches MPI 4-core |       |
| Handles n ≤ 1e18 safely            |       |
| No double-counting or missed terms |       |
| Modular division correct           |       |
| No overflow during reduce          |       |
| Portable across Linux clusters     |       |