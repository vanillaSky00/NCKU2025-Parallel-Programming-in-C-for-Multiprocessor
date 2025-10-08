#include <iostream>
#include <fstream>
#include <mpi.h>

const long long MOD = 1000000007;
const long long moduler_inserve2 = 500000004; // Math.pow(2, MOD - 2) % MOD

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    long long n;
    
    if (world_rank == 0) {
        std::string file_name;
        std::cin >> file_name;
        std::ifstream file(file_name);
        
        file >> n;

        long long ans = 0;
        long long last = 0;
        for (long long i = 1; i * i <= n; i++) {
            long long temp = (n / i) * i % MOD;
            ans = (ans + temp) % MOD;
            last = n / i;
        }

        for (long long i = last - 1; i >= 1; i--) {
            long long l = n / (i + 1) + 1;
            long long r = n / i;

            long long temp = (( (l + r) % MOD ) * ( (r - l + 1) % MOD )) % MOD;
            temp = (temp * moduler_inserve2) % MOD;
            ans = (ans + (temp * i) % MOD) % MOD;
        }
        
        std::cout << ans << "\n"; 
    }
    MPI_Finalize();
}
