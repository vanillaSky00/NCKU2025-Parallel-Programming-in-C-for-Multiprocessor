## Overall
This program simulates a simple 2D stencil / diffusion update on an n × n grid for t time steps.
We parallelize it with MPI by splitting the grid horizontally (by rows) across processes and exchanging only the boundary (halo) rows between neighbors each iteration.<br>

Rank 0 reads the input, broadcasts the problem to everyone, each rank updates its own rows, and in the end we use MPI_Reduce to sum the result back to rank 0.

## Design

### Data distribution
only worder 0 read and then broadcast to all the other.
```
MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
MPI_Bcast(&t, 1, MPI_INT, 0, MPI_COMM_WORLD);
MPI_Bcast(flat.data(), (n+2)*(n+2), MPI_INT, 0, MPI_COMM_WORLD);
```

### Task policy
Split the grid by rows.<br>
If there are p processes, each process handles about `(n / p) × n` cells:<br>

```
rank 0 → rows 1 .. r
rank 1 → rows r+1 .. 2r
...
last rank → takes the remaining rows
```

Each rank updates only its rows every time step.

### Communication
On worker k:
- k sends top row to the up neighbor above, and receive its bottom row → this becomes k's top halo.
- k sends bottom row to the down neighbor, and receive its top row → this becomes k's bottom halo.

<br>

pseudo-diagram:
```
    +-----------+
    |           |
    +-----------+ (rank k-1: up neighbor)
    |           |
    +-----------+
        bottom
          | exchange
          |
         top           
    +-----------+        
    |           |          
    +-----------+ (me: rank k)  
    |           |  
    +-----------+   
        bottom      
          | exchange
          |
    +-----------+
    |           |
    +-----------+ (rank k: down neighbor)
    |           |
    +-----------+
```