## Overall
NCKU Parallel programming using `MPI`

## IO hanlde
Since the input size in parallel problems is usually large,
the program first reads the input file name from stdin,
then opens and reads the actual data from that file.
(Both Problem A and Problem B use this method.)

The output is printed directly to stdout.
```c++
int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n;
    
    if (world_rank == 0) {
        std::string file_name;
        std::cin >> file_name;
        std::ifstream file(file_name);
        
        file >> n;
    }
}
```

## Makefile parameters
```Makefile
// for c
CC := mpicc

// for c++
CC := mpic++
CC := mpicxx

// optimize parameter
CFLAGS := -std=c++14 -O3 -march=skylake-avx512 -mtune=skylake-avx512 \
             -funroll-loops -ffast-math -flto -fno-signed-zeros \
             -fno-trapping-math -fassociative-math -freciprocal-math \
             -mprefer-vector-width=512 \
             -DOMPI_SKIP_MPICXX -DMPICH_SKIP_MPICXX -Wall
```