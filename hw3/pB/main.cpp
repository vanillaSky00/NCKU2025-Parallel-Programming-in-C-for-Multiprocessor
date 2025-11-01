#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mpi.h>
#include <iomanip>
#include <fstream>
#include <string>

using namespace std;

#define UPPER_BOUND 1e6

static constexpr int TAG_ROW = 42;

long long solver(vector<vector<int>>& prev, int n, int t, MPI_Comm comm) {
    int world_size, me;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &me);

    int rows_per = n / world_size;
    int start_row = me * rows_per + 1;
    int end_row = (me == world_size - 1) ? n : start_row + rows_per - 1;

    vector<vector<int>> curr(n+2, vector<int>(n+2, 0));

    int up = (me == 0)                ? MPI_PROC_NULL : me - 1;
    int down = (me == world_size - 1) ? MPI_PROC_NULL : me + 1;

    while(t-- > 0) {
        for (int i = start_row; i <= end_row; i++) {
            for (int j = 1; j <= n; j++) {
                int v = prev[i][j] / 2 + 
                        (prev[i+1][j] + prev[i-1][j] + prev[i][j+1] + prev[i][j-1]) / 4;
                if (v > UPPER_BOUND) v = UPPER_BOUND;
                curr[i][j] = v;
            }
        }
        swap(prev, curr);

        // Broadcast and receive
        // top boundary
        MPI_Sendrecv(
            prev[start_row].data() + 1, n, MPI_INT, up, TAG_ROW,
            prev[end_row + 1].data() + 1, n, MPI_INT, down, TAG_ROW,
            comm, MPI_STATUS_IGNORE);

        // bottom
        MPI_Sendrecv(
            prev[end_row].data() + 1, n, MPI_INT, down, TAG_ROW + 1,
            prev[start_row - 1].data() + 1, n, MPI_INT, up, TAG_ROW + 1,
            comm, MPI_STATUS_IGNORE);      
    }

    long long local = 0;
    for (int i = start_row; i <= end_row; i++) {
        for (int j = 1; j <= n; j++) {
            local += (long long)prev[i][j];
        }
    }

    long long total = 0;
    MPI_Reduce(&local, &total, 1, MPI_LONG_LONG, MPI_SUM, 0, comm);

    return total;
}

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n = 0, t = 0;
    vector<vector<int>> A;

    if (world_rank == 0) {
        string file_name;
        cin >> file_name;
        ifstream file(file_name);

        file >> n >> t;
        A.assign(n + 2, vector<int>(n + 2, 0));  

        for (int i = 1; i <= n; ++i) {
            for (int j = 1; j <= n; ++j) {
                file >> A[i][j];
            }
        }
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&t, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (n == 0) {
        MPI_Finalize();
        return 0; // Exit if file reading failed
    }

    if (world_rank != 0) {
        A.assign(n+2, vector<int>(n+2, 0));
    }

    vector<int> flat((n+2)*(n+2));
    if (world_rank == 0) {
        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                flat[i * n + j] = A[i][j];
            }
        }
    }
    MPI_Bcast(flat.data(), (n+2)*(n+2), MPI_INT, 0, MPI_COMM_WORLD);

    if (world_rank != 0) {
        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                A[i][j] = flat[i * n + j];
            }
        }
    }

    long long res = solver(A, n, t, MPI_COMM_WORLD);

    if (world_rank == 0) {
        cout << res << "\n"; 
    }

    MPI_Finalize();
    return 0;
}