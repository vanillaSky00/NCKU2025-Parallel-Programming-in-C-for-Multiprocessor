https://stackoverflow.com/questions/10363891/parallel-iterative-algorithms-for-solving-linear-system-of-equations



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

```
1. Local pivot selection
Each processor checks its rows in column 
𝑘
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
```

## The advantage of the LU factorization
>The advantage of the LU factorization over the elimination method is that the factorization into L and U is done only once but can be used to solve several linear systems with the same matrix A and different
right-hand side vectors b without repeating the elimination process.

Parallel Programming for Multicore and Cluster Systems Ch7.1 p363