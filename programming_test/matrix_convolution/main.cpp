#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mpi.h>
#include <iomanip>
#include <fstream>
#include <string>

using namespace std;

static constexpr int MOD = 998244353; 

vector<vector<int>> convolution(vector<vector<int>>& A, vector<vector<int>>& kernel, int n, int t, MPI_Comm comm) {
    int world_size, me;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &me);

    vector<vector<int>> tmp(n+2, vector<int>(n+2, 0));

    for(int i = 0; i < t; i++) {
        for(int j = 1; j <= n; j++) {
            for(int k = 1; k <= n; k++) {
                int val = 0;
                for(int x = -1; x <= 1; x++) {
                    for(int y = -1; y <= 1; y++) {
                        val = (val + (A[j + x][k + y] % MOD * 1LL * kernel[x + 1][y + 1] % MOD + MOD) % MOD) % MOD;
                        // calculate convolution with modulo MOD
                    }
                }
                tmp[j][k] = val;
            }
        }
        swap(A, tmp);
    }
    return A;
}

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n = 0, t = 0;
    vector<vector<int>> kernel;
    vector<vector<int>> A;

    if (world_rank == 0) {
        string file_name;
        cin >> file_name;
        ifstream file(file_name);

        file >> n >> t;
        kernel.assign(3, vector<int>(3, 0));
        A.assign(n+2, vector<int>(n+2, 0));
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                file >> kernel[i][j];
            }
        }

        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                file >> A[i][j];
            }
        }
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&t, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (n == 0) {
        MPI_Finalize();
        return 0; 
    }

    if (world_rank != 0) {
        kernel.assign(3, vector<int>(3, 0));
        A.assign(n+2, vector<int>(n+2, 0));
    }

    vector<int> flat_kernel(3 * 3);
    vector<int> flat_A((n+2)*(n+2));
    
    if (world_rank == 0) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                flat_kernel[i * 3 + j] = kernel[i][j];
            }
        }

        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                flat_A[i * (n+2) + j] = A[i][j];
            }
        }
    }
    MPI_Bcast(flat_kernel.data(), 3 * 3, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(flat_A.data(), (n+2)*(n+2), MPI_INT, 0, MPI_COMM_WORLD);

    if (world_rank != 0) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                kernel[i][j] = flat_kernel[i * 3 + j];
            }
        }

        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                A[i][j] = flat_A[i * (n+2) + j];
            }
        }
    }

    vector<vector<int>> res = convolution(A, kernel, n, t, MPI_COMM_WORLD);

    if (world_rank == 0) {
        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                cout << res[i][j] << " ";
            }
            cout << "\n";
        }
    }

    MPI_Finalize();
    return 0;
}