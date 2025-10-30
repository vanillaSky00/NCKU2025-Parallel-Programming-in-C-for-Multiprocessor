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

static constexpr double EPS = 1e-12; 
static constexpr int TAG_PIVOT = 42;



int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n = 0, t = 0;
    vector<vector<double>> A;

    if (world_rank == 0) {
        std::string file_name;
        if (argc < 2) {
            cerr << "Usage: " << argv[0] << " <input_file>" << endl;
            n = 0; 
        } 
        else {
            file_name = argv[1];
            std::ifstream file(file_name);
            
            if (!file.is_open()) {
                cerr << "Error: Could not open file " << file_name << endl;
                n = 0;
            } 
            else {
                file >> n;
                file >> t;
                A.assign(n+2, vector<double>(n+2));
                for (int i = 1; i <= n; i++) {
                    for (int j = 1; j <= n; j++) {
                        file >> A[i][j];
                    }
                }
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
        A.assign(n+2, vector<double>(n+2));
    }

    vector<double> flat((n+2)*(n+2));
    if (world_rank == 0) {
        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                flat[i * n + j] = A[i][j];
            }
        }
    }
    MPI_Bcast(flat.data(), (n+2)*(n+2), MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (world_rank != 0) {
        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= n; j++) {
                A[i][j] = flat[i * n + j];
            }
        }
    }

    if (world_rank == 0) {
        // cout << n << " " << t << "\n";
        // for (int i = 0; i < n+2; i++) {
        //     for (int j = 0; j < n+2; j++) cout << A[i][j] << " ";
        //     cout << "\n";
        // }
    }

    MPI_Finalize();
    return 0;
}