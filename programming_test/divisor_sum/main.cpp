#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <mpi.h>
using namespace std;

int main(int argc, char* argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(0);
    
    MPI_Init(&argc, &argv);
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    
    int n = 0;

    if (world_rank == 0) {
        string filename;
        cin >> filename;

        ifstream file(filename);
    
        file >> n;

        file.close();
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    long long res = 0;
    int sqrt_n = floor(sqrt(n));

    for (int i = 1; i <= sqrt_n; i++) {
        res += n / i;
    }
    for (int i = sqrt_n - 1; i >= 1; i--) {
        int l = n / (i + 1);
        int r = n / i;
        res += i * (r - l);
    }
    
    if (world_rank == 0) {
        cout << res;
    }
    
    MPI_Finalize();
    return 0;
}
