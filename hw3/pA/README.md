## Overall
Use MPI to parallelize the forward elimination phase of Gaussian elimination, since this stage allows independent row updates that can be distributed across multiple processes.<br>

This implementation focuses on designing an efficient communication pattern to coordinate these distributed computations.


## Common pitfall
In MPI, each process is an independent program instance with its own memory space — there is no shared memory.<br>
Therefore, all data exchange must occur through explicit message passing rather than direct access.<br>

### Directly swap the pointer

| Global row | Owner |
| ---------- | ----- |
| 0          | P₀    |
| 1          | P₁    |
| 2          | P₀    |
| 3          | P₁    |

But sometimes pivot = row 2 (on P₀) and row k=1 (on P₁).<br>
Now they belong to different address spaces. P₀ cannot directly touch P₁’s row array in memory.


The memory space are different among MPI processes. And a process has its own memory space, they cannot swap pointers globally. <br>
And it also explains why we often see copy the value from the message we receive.

**Not threads!**
We are creating 4 completely separate processes, each with its own:

- stack
- heap
- global/static variables
- file descriptors
- memory address space

### Synchronized Bidirectional Row Exchange
When implementing a parallel swap between two specific, non-contiguous rows owned by different processors, always use `MPI_Sendrecv` instead of separate `MPI_Send` and `MPI_Recv` calls. This prevents deadlock (where both processors are waiting to receive before they can send) and ensures the exchange is complete and synchronized.


### Data Distribution 
Must be aware of correct data distribution, that is what do I have in this current worker (processor). And then do the communication (send and receive).
- Which worker owns the data I need.
- Which worker needs my data.

## How to solve a linear system 
After elimination, we get upper-triangular form matrix like:

<br>

```math
\begin{bmatrix}
    \begin{array}{cccc|c}
        * & * & * & * & * \\
        0 & * & * & * & * \\
        0 & 0 & 0 & * & * \\
    \end{array}
\end{bmatrix}
```

We define:


- $rank(A)$: number of non-zero rows in the upper-triangular form of A

- $rank([A | b])$: number of non-zero rows in the augmented matrix

<br>

| Case                   | Condition         | Meaning |                                              
| ---------------------- | ----------------- | ------- | 
| **Unique solution**    | rank(A) = rank([A \| b]) = n | exactly one (x)                              | 
| **Infinite solutions** | rank(A) = rank([A \| b]) < n | system underdetermined (some free variables) |      
| **No solution**        | rank(A) < rank([A \| b])     | inconsistent (a row like `0 0 0  \| c≠0`) |

<br>

```math
\begin{bmatrix}
    \begin{array}{ccc|c}
        0 & * & * & * \\
        0 & 0 & * & * \\
        0 & 0 & 0 & * 
    \end{array}
\end{bmatrix}
```

```math
\begin{bmatrix}
    \begin{array}{ccc|c}
        1 & 2 & 3 & 6\\
        0 & 0 & 0 & 0\\
        0 & -1 & -2 & -3\\
    \end{array}
\end{bmatrix}
```

```math
\begin{bmatrix}
    \begin{array}{ccc|c}
        1 & 2 & 3 & 6\\
        0 & 0 & 0 & 5
    \end{array}
\end{bmatrix}
```

## Pivoting
Why?
When solving a linear system $ Ax = y $ using Gaussain elimination, we need to divided by the diagonal element $ Ax = y $ during elimination. <br>
- $a^{(k)}_{kk} = 0$

- $a^{(k)}_{kk}$ is very small 

**To avoid this, the better strategy:<br>
Swap rows or columns so that we divide by a larger (better) number — this process is called pivoting.**

$a^{(k)}_{rk}$ where the pivot has the maximum absolute value in the row

```math
\begin{bmatrix}
0 & 2 & 3\\
1 & 1 & 1\\
2 & 4 & 6\\
\end{bmatrix}
```


swap $row_{0}$  with  $row_{2}$ 

```math
\begin{bmatrix}
2 & 4 & 6\\
1 & 1 & 1\\
0 & 2 & 3\\
\end{bmatrix}
```

pivot $a_{00} = 2$

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
**Instead of giving each worker a row, we assign them in cyclic pattern.**

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

## Implementation


**Forward elimination**

1. **Local pivot pick** <br>
   On step `k`, each rank scans only the rows it owns (those with `i % p == me`) and in column `k` finds its best candidate `(abs value, global row id)`.

2. **Global pivot pick** <br>
   All ranks run `MPI_Allreduce` with `MPI_MAXLOC` to pick the single global best pivot row.

3. **Pivot row placement** <br>
   If the pivot row lives on a different rank than row `k`, the two ranks exchange that row (send/recv a contiguous buffer of length `n+1`) so that the row `k` is correct on its owner.

4. **Pivot broadcast** <br>
   The rank that owns row `k` normalizes it (divide by the diagonal) and broadcasts the tail `pivot[k..n]` to all ranks.

5. **Local elimination** <br>
   Each rank goes through *its* rows `i > k` (same `i % p == me`) and uses the received pivot to zero `A[i][k]` and update `A[i][j], b[i]` for `j > k`. This part is fully parallel.

---

**Rebuild matrix on rank 0**

6. **Gather rows back** <br>
   After elimination, each nonzero rank sends its rows `(A[i][*], b[i])` to rank 0, because rows are cyclically distributed and rank 0 does not naturally hold all rows in order.

---

**Rank checking**

7. **Rank 0 determines type** <br>
   Counts `rank(A)` and `rank([A|b])`, after compariosn of two broadcast the result to all ranks.
---

**Backward substitution**

8. **Solve from bottom** <br>
   Starting from the last row, the rank that owns row `k` computes `x[k]`, broadcasts it, and all ranks use it to finish the solution vector.


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

## MPI Note
`MPI_Sendrecv` is a blocking MPI routine that combines a send and a receive operation into a single call. Main purpose: safely exchange data between two processes without the risk of **deadlock** that can occur when using separate `MPI_Send` and `MPI_Recv` calls in certain communication patterns.<br>

`MPI_Allreduce` finds the global pivot row and its value. 
```c++
struct { double val; int row; } in, out;
in.val = loc_val;
in.row = (loc_row == -1 ? -1 : loc_row);
MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm);

// 1 Each rank finds its local maximum pivot (val, row_index).
// 2 MPI_MAXLOC operator finds the global maximum value and keeps its corresponding index.
// 3 MPI_DOUBLE_INT = predefined MPI struct type {double, int} for pairwise reductions.
// 4 Result (out) is identical on all ranks.
```

| MPI Call                | Who Calls      | Who Waits | Blocking?   | Synchronization Scope |
| ----------------------- | -------------- | --------- | ----------- | --------------------- |
| `MPI_Allreduce`         | all ranks      | all ranks | Yes         | Global                |
| `MPI_Bcast`             | all ranks      | all ranks | Yes         | Global                |
| `MPI_Sendrecv`  | 2 ranks        | those 2   | Yes         | Pairwise              |
| `MPI_Send` / `MPI_Recv` | specific pairs | those 2   | Yes | Pairwise              |
| `MPI_Gatherv`           | all ranks      | all ranks | Yes         | Global                |

<br>

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
``