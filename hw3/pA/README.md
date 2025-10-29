https://stackoverflow.com/questions/10363891/parallel-iterative-algorithms-for-solving-linear-system-of-equations



## How to solve a linear system 
1. unique solution
2. infinite solution
3. no solution


## Pivoting
Why?
When solving a linear system $ Ax = y $ using Gaussain elimination, we need to divided by the diagonal element $ Ax = y $ during elimination. <br>
- $ a^{(k)}_{kk} = 0 $
- $ a^{(k)}_{kk} $ is very small 

To avoid this, the better strategy:<br>
Swap rows or columns so that we divide by a larger (better) number — this process is called pivoting.

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


### The advantage of the LU factorization
>The advantage of the LU factorization over the elimination method is that the factorization into L and U is done only once but can be used to solve several linear systems with the same matrix A and different
right-hand side vectors b without repeating the elimination process.

Parallel Programming for Multicore and Cluster Systems Ch7.1 p363