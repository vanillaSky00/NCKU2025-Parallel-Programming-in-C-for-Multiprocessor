## How to solve a linear system 
1. Unique solution
2. Infinite solution
3. No solution


After elimination, we get upper-triangular form matrix like:

<br>

$
\begin{bmatrix}
    \begin{array}{cccc|c}
        * & * & * & * & * \\
        0 & * & * & * & * \\
        0 & 0 & 0 & * & * \\
    \end{array}
\end{bmatrix}
$

We define:


- $rank(A)$: number of non-zero rows in the upper-triangular form of A

- $rank(\begin{bmatrix}\begin{array}{c|c}
        A & b \\
    \end{array} \end{bmatrix})$: number of non-zero rows in the augmented matrix

<br>

| Case                   | Condition         | Meaning |                                              |       |
| ---------------------- | ----------------- | ------- | -------------------------------------------- | ----- |
| **Unique solution**    | rank(A) = rank([A \| b]) = n | exactly one (x)                              |       |
| **Infinite solutions** | rank(A) = rank([A \| b]) < n | system underdetermined (some free variables) |       |
| **No solution**        | rank(A) < rank([A \| b])     | inconsistent (a row like `0 0 0  \| c≠0`) |

<br>

$
\begin{bmatrix}
    \begin{array}{ccc|c}
        0 & * & * & * \\
        0 & 0 & * & * \\
        0 & 0 & 0 & * 
    \end{array}
\end{bmatrix}
$

$
\begin{bmatrix}
    \begin{array}{ccc|c}
        1 & 2 & 3 & 6\\
        0 & 0 & 0 & 0\\
        0 & -1 & -2 & -3\\
    \end{array}
\end{bmatrix}
$

$
\begin{bmatrix}
    \begin{array}{ccc|c}
        1 & 2 & 3 & 6\\
        0 & 0 & 0 & 5
    \end{array}
\end{bmatrix}
$

## Pivoting
Why?
When solving a linear system $ Ax = y $ using Gaussain elimination, we need to divided by the diagonal element $ Ax = y $ during elimination. <br>
- $ a^{(k)}_{kk} = 0 $

- $ a^{(k)}_{kk} $ is very small 

**To avoid this, the better strategy:<br>
Swap rows or columns so that we divide by a larger (better) number — this process is called pivoting.**

$ a^{(k)}_{rk} $ where the pivot has the maximum absolute value in the row

$\begin{bmatrix}
0 & 2 & 3\\
1 & 1 & 1\\
2 & 4 & 6\\
\end{bmatrix}$

swap $ row_{0} $  with  $ row_{2} $  

$\begin{bmatrix}
2 & 4 & 6\\
1 & 1 & 1\\
0 & 2 & 3\\
\end{bmatrix}$

pivot $ a_{00} = 2 $

<br>

| Type                | What it does                                                                              | Example                                                |
| ------------------- | ----------------------------------------------------------------------------------------- | ------------------------------------------------------ |
| **Row pivoting**    | Swap rows so the pivot (diagonal element) is the largest in that column                   | Swap row 1 with row 3 → pivot becomes 2                |
| **Column pivoting** | Swap columns so that the pivot is the largest in that row                                 | Less common — usually used if column structure matters |
| **Total pivoting**  | Swap both rows *and* columns to get the largest possible pivot in the remaining submatrix | Most accurate, but expensive                           |


<br>


## Parallel Row-Cycle
During the elimination, intuitively we can divid rows among processors, so they can work in parallel.
Yes, however, how to assign work to those workers (processors) ? Therefore, now the problem relates to<br>

**load balancing**

Suppose we have 4 rows and 2 processors:

| Row | Processor |
| --- | --------- |
| 1   | P₁        |
| 2   | P₁        |
| 3   | P₂        |
| 4   | P₂        |

During elimination:
- After the first step (using row 1 as pivot), all rows below must be updated.
- But when we reach later steps, some processors finish early and become idle (no more rows to process).
- This wastes time → poor parallel efficiency ⚠️

Here is how Row-Cycle distribution hit in:
** Instead of giving each worker a row, we assign them in cyclic pattern.

| Row | Processor |
| --- | --------- |
| 1   | P₁        |
| 2   | P₂        |
| 3   | P₁        |
| 4   | P₂        |
| 5   | P₁        |
| 6   | P₂        |
| ... | ...       |

we assign worker1 (P1) with 1,3,5,7,... worker2 with 2,4,6,8...
- Each processor owns every p-th row (like round-robin).
- When later elimination steps remove upper rows, the remaining active rows are still evenly spread.
- Load is balanced — no processor sits idle too early.

<br>

```
for (int k = 0; k < n - 1; k++) {
    // pivot selection
    // maybe swap
    // broadcast pivot row
    // elimination on my rows
}
```


```markdown
## Forward Elimination
    1. Local pivot selection
    Each processor checks its rows in column 𝑘
    k to find the largest value → its local pivot candidate.

    2. Global pivot selection
    All processors compare their local pivots to find the overall (global) best pivot → needs communication.

    3. Pivot row exchange
    If the global pivot is on another processor, they swap the rows (so everyone uses the correct pivot row).

    4. Pivot row broadcast
    The processor owning the pivot sends it to all others — everyone needs this row to eliminate below.

    5. Compute elimination factors
    Each processor updates its own rows below the pivot in parallel.

    6. Update local matrix
    Each processor uses the pivot to eliminate elements in its own rows → local work, no need to wait.

## Rank checking to determine solution type

## Backward elimination
```


## MPI Note

`MPI_Allreduce` finds the global pivot row and its value. The singular_col check is important for identifying if the system is singular or has infinite solutions.
```c++
struct { double val; int idx; } in { loc_val, loc_piv } , out {};
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm);

        const int pivot_row = out.idx;
        const bool singular_col = (out.val < EPS) || (pivot_row < 0);
        const int pivot_owner = pivot_row < 0 ? 0 : pivot_row % world_size;
        constexpr int TAG_PIVOT = 42;
```

## C++ Note
<br>

| Feature | `const` | `constexpr` |
|----------|----------|-------------|
| **Evaluation Time** | Primarily runtime | Primarily compile-time (can be runtime for functions) |
| **Initialization** | Can be runtime or compile-time | Must be compile-time |
| **Implicit `const`** | No | Yes |
| **Use in Compile-Time Contexts** | Limited (e.g., `const int` initialized with literal) | Yes (e.g., array dimensions, template arguments) |

example 1:
```c++
const int runtime_value = get_user_input(); // Initialized at runtime
const int compile_time_value = 10;          // Initialized at compile time

void print_value(const int& value) {
    // value cannot be modified inside this function
}
```

example 2:
```c++
constexpr int compile_time_array_size = 5;
int arr[compile_time_array_size]; // Valid because compile_time_array_size is constexpr

constexpr int square(int x) {
    return x * x;
}


constexpr int result_compile_time = square(3); // Evaluated at compile time
int runtime_input = 5;
int result_runtime = square(runtime_input);   // Evaluated at runtime
```

## The advantage of the LU factorization
Thanks to:
https://stackoverflow.com/questions/10363891/parallel-iterative-algorithms-for-solving-linear-system-of-equations


Parallel Programming for Multicore and Cluster Systems Ch7.1 p363:
>The advantage of the LU factorization over the elimination method is that the factorization into L and U is done only once but can be used to solve several linear systems with the same matrix A and different
right-hand side vectors b without repeating the elimination process.

<br>
Also if we want unique solution A should be invertible.

| Concept                              | Requires A invertible? | Reason                                              |
| ------------------------------------ | ---------------------- | --------------------------------------------------- |
| **Existence of LU (algebraic)**      | ❌ Not necessarily      | Can exist even if singular, though U may have zeros |
| **LU with partial pivoting (PA=LU)** | ❌ No                   | Always exists for any square A                      |
| **Using LU to solve Ax=b uniquely**  | ✅ Yes                  | Need A⁻¹ to exist for unique solution               |


<br>

## Common pitfall

### Directly swap the pointer

| Global row | Owner |
| ---------- | ----- |
| 0          | P₀    |
| 1          | P₁    |
| 2          | P₀    |
| 3          | P₁    |

But sometimes pivot = row 2 (on P₀)
and row k=1 (on P₁).
Now they belong to different address spaces.
P₀ cannot directly touch P₁’s row array in memory.

| Reason              | Explanation                                                                   |
| ------------------- | ----------------------------------------------------------------------------- |
| 💾 Memory isolation | Each MPI process has its own local memory; you can’t “swap pointers” globally |
| 🚚 Communication    | Row data must be physically sent via MPI                                      |
| 🔄 Uniform handling | `copy_*` functions abstract both local and remote swaps                       |
| 📦 Contiguity       | MPI needs contiguous buffers for broadcast/send                               |
| 🧱 Robustness       | Keeps logic simple even for cyclic distributions and partial ownership        |





## Synchronized Bidirectional Row Exchange
When implementing a parallel swap between two specific, non-contiguous rows owned by different processors, always use **$\text{MPI\_Sendrecv}$** instead of separate $\text{MPI\_Send}$ and $\text{MPI\_Recv}$ calls. This prevents deadlock (where both processors are waiting to receive before they can send) and ensures the exchange is complete and synchronized.


🛠️ Key Fix: Synchronized Bidirectional Row Exchange

The original code used an asynchronous `MPI_Send` on one processor and an `MPI_Recv` on the other to handle the data swap. This approach was incomplete and prone to deadlocks or data races, but the core issue was a **missing data transfer** and **incorrect buffer initialization** for the subsequent broadcast.

### 1. The Original Problem (Asynchronous and Incomplete Swap) ❌

The logical operation required a swap: $\text{Row } k$ moves to the $\text{pivot\_row}$'s spot ($\text{pivot\_row}$), and the $\text{pivot\_row}$ moves to $\text{row } k$'s spot.

* **Processor $k$ Owner (`k % world_size == me`):** It $\text{MPI\_Send}$ $\text{row } k$'s data to the $\text{pivot\_owner}$, but it **failed to $\text{MPI\_Recv}$** the old $\text{pivot\_row}$'s data (which should become the new $\text{row } k$).
* **Pivot Owner (`pivot_owner == me`):** It $\text{MPI\_Recv}$ $\text{row } k$'s data (and updated its $\text{pivot\_row}$). Crucially, it then initialized the broadcast buffer (`buf`) with the old $\text{pivot\_row}$'s data, but it **failed to $\text{MPI\_Send}$** this old $\text{pivot\_row}$ data back to the $\text{k\_owner}$.

Because the $\text{k\_owner}$ (which is the root for the broadcast) never received the correct data, it broadcasted an old, unswapped version of $\text{row } k$, leading to incorrect elimination results and a wrong final answer.

### 2. The Corrected Solution (MPI\_Sendrecv) ✅

The fix replaced the separate sends/receives with the **atomic $\text{MPI\_Sendrecv}$ function** to ensure a correct bidirectional exchange and proper buffer initialization.

| Processor Role | Action Performed by `MPI_Sendrecv` | Resulting State |
| :--- | :--- | :--- |
| **Processor $k$ Owner** | **SEND:** $\text{Row } k$ data (`tmp` buffer) to $\text{pivot\_owner}$. | The $\text{pivot\_row}$ is now correctly updated on $\text{pivot\_owner}$. |
| | **RECEIVE:** Old $\text{pivot\_row}$ data (into `buf`) from $\text{pivot\_owner}$. | The $\text{buf}$ now holds the correct data for the new $\text{row } k$, ready for $\text{Bcast}$. |
| **Pivot Owner** | **SEND:** Old $\text{pivot\_row}$ data (from `buf`) to $\text{k\_owner}$. | The $\text{k\_owner}$ can now update its new $\text{row } k$. |
| | **RECEIVE:** $\text{Row } k$ data (into `tmp`) from $\text{k\_owner}$. | The $\text{pivot\_row}$ is updated locally using $\text{tmp}$. |

By making this change:
1.  The $\text{k\_owner}$ uses `unpack_pivot_tail` to locally update its new $\text{row } k$ (with the received $\text{pivot\_row}$ data stored in `buf`).
2.  The $\text{k\_owner}$'s `buf` correctly contains the data for the new $\text{row } k$ (which is the old $\text{pivot\_row}$) just before the $\text{MPI\_Bcast}$.
3.  The $\text{pivot\_owner}$ correctly updates its local $\text{pivot\_row}$ (with the received $\text{row } k$ data stored in `tmp`).

This atomic and synchronized data movement resolves the inconsistency, allowing the subsequent $\text{MPI\_Bcast}$ to propagate the correct pivot row to all processors for the elimination step.
