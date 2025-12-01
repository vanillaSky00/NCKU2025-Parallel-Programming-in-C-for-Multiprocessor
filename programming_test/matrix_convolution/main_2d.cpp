#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mpi.h>
#include <iomanip>
#include <fstream>
#include <string>

using namespace std;

static constexpr int TAG_ROW = 42;
static constexpr int MOD = 998244353; 

vector<vector<int>> convolution(vector<vector<int>>& A, vector<vector<int>>& kernel, int n, int t, MPI_Comm comm) {
    int world_size, me;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &me);

    int rows_per = n / world_size;
    int start_row = me * rows_per + 1;
    int end_row = (me == world_size - 1) ? n : start_row + rows_per - 1;

    vector<vector<int>> tmp(n+2, vector<int>(n+2, 0));

    int up = (me == 0)                ? MPI_PROC_NULL : me - 1;
    int down = (me == world_size - 1) ? MPI_PROC_NULL : me + 1;

    while (t-- > 0) {
        for(int r = start_row; r <= end_row; r++) {
            for(int c = 1; c <= n; c++) {
                int val = 0;
                for(int x = -1; x <= 1; x++) {
                    for(int y = -1; y <= 1; y++) {
                        // new value += neighbor value * weight
                        val = (val + (A[r + x][c + y] % MOD * 1LL * kernel[x + 1][y + 1] % MOD + MOD) % MOD) % MOD;
                    }
                }
                tmp[r][c] = val;
            }
        }
        swap(A, tmp);

        MPI_Sendrecv(
            A[start_row].data() + 1, n, MPI_INT, up, TAG_ROW,
            A[end_row + 1].data() + 1, n, MPI_INT, down, TAG_ROW,
            comm, MPI_STATUS_IGNORE
        );

        MPI_Sendrecv(
            A[end_row].data() + 1, n, MPI_INT, down, TAG_ROW + 1,
            A[start_row - 1].data() + 1, n, MPI_INT, up, TAG_ROW + 1,
            comm, MPI_STATUS_IGNORE
        );
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

    // After done the convolution assembly those only strip-corrected result
    int rows_per = n / world_size;
    int start_row = world_rank * rows_per + 1;
    int end_row = (world_rank == world_size - 1) ? n : start_row + rows_per - 1;
    int my_count = end_row - start_row + 1;
    vector<int> flat_strip(my_count * n);

    int ptr = 0;
    for (int i = start_row; i <= end_row; i++) {
        for (int j = 1; j <= n; j++) {
            flat_strip[ptr++] = res[i][j];
        }
    }

    vector<int> flat_full_res;
    vector<int> recv_counts;
    vector<int> displs; // When getting the package from Process X,the reference to place in big arr

    if (world_rank == 0) {
        flat_full_res.resize(n * n);
        recv_counts.resize(world_size);
        displs.resize(world_size);

        for (int r = 0; r < world_size; r++) {
            // We are now in rank 0, so make sure to count for other processors
            // information, which they have may different elements counts
            int r_start = r * rows_per + 1;
            int r_end = (r == world_size - 1) ? n : r_start + rows_per - 1;
            recv_counts[r] = (r_end - r_start + 1) * n;
            displs[r] = (r == 0) ? 0 : displs[r-1] + recv_counts[r-1];
        }
    }

    MPI_Gatherv(
        flat_strip.data(), my_count * n, MPI_INT,                           // send my strip
        flat_full_res.data(), recv_counts.data(), displs.data(), MPI_INT,   // recv args
        0, MPI_COMM_WORLD                                                   // root is 0
    );

    if (world_rank == 0) {
        int idx = 0;
        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                cout << flat_full_res[idx++] << " ";
            }
            cout << "\n";
        }
    }

    MPI_Finalize();
    return 0;
}