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

void convolution(vector<int>& A, vector<int>& kernel, int n, int t, MPI_Comm comm) {
    int world_size, me;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &me);

    int rows_per  = n / world_size;
    int start_row = me * rows_per + 1;
    int end_row   = (me == world_size - 1) ? n : start_row + rows_per - 1;

    int sz = n + 2;
    vector<int> tmp(sz * sz, 0);

    int up   = (me == 0)                ? MPI_PROC_NULL : me - 1;
    int down = (me == world_size - 1)   ? MPI_PROC_NULL : me + 1;

    while (t-- > 0) {
        for(int r = start_row; r <= end_row; r++) {
            int row_idx = r * sz;
            for(int c = 1; c <= n; c++) {
                int idx = row_idx + c;
                long long val = 0;
                
                val += (long long)A[idx - sz - 1] * kernel[0];
                val += (long long)A[idx - sz]     * kernel[1];
                val += (long long)A[idx - sz + 1] * kernel[2];
                val += (long long)A[idx - 1]      * kernel[3];
                val += (long long)A[idx]          * kernel[4];
                val += (long long)A[idx + 1]      * kernel[5];
                val += (long long)A[idx + sz - 1] * kernel[6];
                val += (long long)A[idx + sz]     * kernel[7];
                val += (long long)A[idx + sz + 1] * kernel[8];

                tmp[idx] = (val % MOD + MOD) % MOD;
            }
        }
        swap(A, tmp);

        MPI_Sendrecv(
            &A[start_row * sz + 1], n, MPI_INT, up, TAG_ROW,
            &A[(end_row + 1) * sz + 1], n, MPI_INT, down, TAG_ROW,
            comm, MPI_STATUS_IGNORE
        );

        MPI_Sendrecv(
            &A[end_row * sz + 1], n, MPI_INT, down, TAG_ROW + 1,
            &A[(start_row - 1) * sz + 1], n, MPI_INT, up, TAG_ROW + 1,
            comm, MPI_STATUS_IGNORE
        );
    }
}

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n = 0, t = 0;
    vector<int> kernel;
    vector<int> A;

    if (world_rank == 0) {
        string file_name;
        cin >> file_name;
        ifstream file(file_name);

        file >> n >> t;
        kernel.resize(9);
        A.resize((n + 2) * (n + 2), 0);
        
        for (int i = 0; i < 9; i++) {
            file >> kernel[i];
        }

        int sz = n + 2;
        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                file >> A[i * sz + j];
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
        kernel.resize(9);
        A.resize((n + 2) * (n + 2), 0);
    }

    MPI_Bcast(kernel.data(), 9, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(A.data(), (n + 2) * (n + 2), MPI_INT, 0, MPI_COMM_WORLD);

    convolution(A, kernel, n, t, MPI_COMM_WORLD);

    int rows_per  = n / world_size;
    int start_row = world_rank * rows_per + 1;
    int end_row   = (world_rank == world_size - 1) ? n : start_row + rows_per - 1;
    int my_count  = end_row - start_row + 1;
    
    vector<int> flat_strip(my_count * n);
    int sz = n + 2;
    int ptr = 0;
    for (int i = start_row; i <= end_row; i++) {
        for (int j = 1; j <= n; j++) {
            flat_strip[ptr++] = A[i * sz + j];
        }
    }

    vector<int> flat_full_res;
    vector<int> recv_counts;
    vector<int> displs; 

    if (world_rank == 0) {
        flat_full_res.resize(n * n);
        recv_counts.resize(world_size);
        displs.resize(world_size);

        for (int r = 0; r < world_size; r++) {
            int r_start    = r * rows_per + 1;
            int r_end      = (r == world_size - 1) ? n : r_start + rows_per - 1;
            recv_counts[r] = (r_end - r_start + 1) * n;
            displs[r]      = (r == 0) ? 0 : displs[r-1] + recv_counts[r-1];
        }
    }

    MPI_Gatherv(
        flat_strip.data(), my_count * n, MPI_INT,                           
        flat_full_res.data(), recv_counts.data(), displs.data(), MPI_INT,   
        0, MPI_COMM_WORLD                                                   
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