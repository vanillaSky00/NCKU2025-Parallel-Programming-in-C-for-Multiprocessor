#include <iostream>
#include <fstream>
#include <mpi.h>
#include <cmath>

const long long MOD = 1000000007;
const long long MOD_INV2 = 500000004; // Math.pow(2, MOD - 2) % MOD

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    long long n = 0;

    if (world_rank == 0) {
        std::string file_name;
        std::cin >> file_name;
        std::ifstream file(file_name);
        file >> n;
    }

    // broadcast n from rank 0 to all the other processes
    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    // split to 4 cores, rank 0, 1, 2, 3
    int group = world_rank / 2;    // 0 for first half, 1 for second half
    int local_id = world_rank % 2; // 0 or 1 within each group

    long long sqrt_n = floor(sqrt( (long double) n )); // 1 -> sqrt_n
    long long local_sum = 0;

    // group 0: handle first loop sqrt(n) to 1/2 sqt(n) + 1/2 sqt(n)
    if (group == 0) {
        long long chunk = (sqrt_n + 1) / 2;
        int start = (local_id == 0) ? 1 : chunk + 1;
        int end = (local_id == 0) ? chunk : sqrt_n;

        for (long long i = start; i <= end; i++) {
            long long temp = (n / i) * i % MOD;
            local_sum = (local_sum + temp) % MOD;
        }
    }

    // group 1 : handle second loop
    if (group == 1) {
        long long last = n / sqrt_n;
        long long chunk = (last - 1) / 2;
        int start  = (local_id == 0) ? 1 : chunk + 1;
        int end = (local_id == 0) ? chunk : last - 1;

        for (long long i = start; i <= end; i++) {
            long long l = n / (i + 1) + 1;
            long long r = n / i;
            
            long long temp = (( (l + r) % MOD ) * ( (r - l + 1) % MOD )) % MOD;
            temp = (temp * MOD_INV2) % MOD;
            local_sum = (local_sum + (temp * i) % MOD) % MOD;
        }
    }

    long long* global_result = nullptr;
    if (world_rank == 0) global_result = new long long[world_size];

    MPI_Gather(&local_sum, 1, MPI_LONG_LONG, 
                global_result, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        long long total = 0;
        for (int i = 0; i < world_size; i++) {
            total = (total + (global_result[i] % MOD) ) % MOD;
        }
        std::cout << total << "\n"; 
        delete[] global_result;
    }

    MPI_Finalize();
}