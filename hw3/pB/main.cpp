#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mpi.h>
#include <iomanip>
#include <fstream>
#include <string>

// Include OpenMP for fine-grained parallelism
#include <omp.h>

using namespace std;

#define UNIQUE_SOLUTION 0
#define INFINITY_SOLUTION 1
#define NO_SOLUTION 2

static constexpr double EPS = 1e-12; 
static constexpr int TAG_PIVOT = 42;

// Add const to functions that don't modify the vectors for better compiler optimization
static inline pair<int, double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me); 
static inline void swap_rows(vector<vector<double>>& A, vector<double>& b, int r1, int r2); 
static inline void pack_pivot_tail(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf); 
static inline void unpack_pivot_tail(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf); 
static long double parse_token(string tok);


vector<double> gauss_cyclic(vector<vector<double>>& A, vector<double>& b, int n, MPI_Comm comm, unsigned char& state) { 
    int world_size, me; 
    MPI_Comm_size(comm, &world_size); 
    MPI_Comm_rank(comm, &me); 
    
    // Use const for variables that won't change
    const int N_PLUS_ONE = n + 1;
    vector<double> x(n), buf(N_PLUS_ONE), tmp(N_PLUS_ONE);

    // 1. Forward elimination 
    for (int k = 0; k < n - 1; k++) {
        // a. Local pivot selection 
        auto [loc_piv, loc_val] = local_pivot_candidate(A, k, n, world_size, me); 
        
        // b. Global pivot selection 
        struct { double val; int idx; } in { loc_val, loc_piv } , out {}; 
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm); 
        
        const int pivot_row = out.idx; 
        const bool singular_col = (out.val < EPS) || (pivot_row < 0); 
        const int pivot_owner = pivot_row < 0 ? 0 : pivot_row % world_size; 

        if (!singular_col) {
            
            // c. Pivot Row Exchange and Broadcast Preparation (Optimized with N_PLUS_ONE)
            if (pivot_owner == me && k % world_size == me) {
                // Case 1: Pivot row (r) and row k are on the same processor (me)
                if (pivot_row != k) swap_rows(A, b, pivot_row, k);
                pack_pivot_tail(A, b, k, n, buf);
            }
            else {
                // Case 2: Pivot row (r) and row k are on different processors
                const int COMM_COUNT = n - k + 1;
                if (k % world_size == me) {
                    pack_pivot_tail(A, b, k, n, tmp); 
                    
                    MPI_Sendrecv(tmp.data() + k, COMM_COUNT, MPI_DOUBLE, pivot_owner, TAG_PIVOT,
                                 buf.data() + k, COMM_COUNT, MPI_DOUBLE, pivot_owner, TAG_PIVOT,
                                 comm, MPI_STATUS_IGNORE);
                    
                    unpack_pivot_tail(A, b, k, n, buf); 
                }
                else if (pivot_owner == me) {
                    pack_pivot_tail(A, b, pivot_row, n, buf); 
                    
                    MPI_Sendrecv(buf.data() + k, COMM_COUNT, MPI_DOUBLE, k % world_size, TAG_PIVOT,
                                 tmp.data() + k, COMM_COUNT, MPI_DOUBLE, k % world_size, TAG_PIVOT,
                                 comm, MPI_STATUS_IGNORE);
                    
                    unpack_pivot_tail(A, b, pivot_row, n, tmp);
                }
            }
        } 
        else {
            if (me == (k % world_size)) std::fill(buf.begin() + k, buf.end(), 0.0);
        }

        // d. Broadcast the pivot row tail (now at row k)
        int root = singular_col ? (k % world_size) : (k % world_size); 
        MPI_Bcast(buf.data() + k, n - k + 1, MPI_DOUBLE, root, comm);

        // e. Make every replica's row k identical
        unpack_pivot_tail(A, b, k, n, buf);
        
        // f. Elimination on my rows i = k+1, k+1+p, ... 
        int i = k + 1;
        while (i < n && (i % world_size) != me) i++;
        
        const double pivot_val = buf[k];
        if (fabs(pivot_val) >= EPS) {
            // Apply OpenMP to parallelize the outer loop across available CPU cores 
            // on this specific MPI rank. This is a hybrid MPI/OpenMP approach.
            #pragma omp parallel for private(i) schedule(dynamic)
            for (i = k + 1 + me; i < n; i += world_size) {
                // Loop unrolling optimization is often done by the compiler, 
                // but local variables and direct array access help.
                double l = A[i][k] / pivot_val;
                A[i][k] = 0.0;
                
                // Inner loop optimization: cache-friendly access and use of vectorization
                // The loop is now slightly more efficient due to `l` and `pivot_val` being local consts
                for (int j = k + 1; j < n; ++j) {
                    A[i][j] -= l * buf[j];
                }
                b[i] -= l * buf[n];
            }
        }
    } 
    
    // 2. Check for singularity/rank
    state = UNIQUE_SOLUTION; 
    if (me == 0) { 
        int rankA = 0, rankAb = 0; 
        for (int i = 0; i < n; i++) { 
            bool zeroA = true; 
            for (int j = 0; j < n; j++) { 
                if (fabs(A[i][j]) > EPS) { 
                    zeroA = false; 
                    break; 
                } 
            } 
            if (!zeroA) rankA++; 
            else if (fabs(b[i]) > EPS) rankAb++; 
        } 
        
        if (rankAb > 0) state = NO_SOLUTION; 
        else if (rankA < n) state = INFINITY_SOLUTION; 
        else state = UNIQUE_SOLUTION; 
    } 
    MPI_Bcast(&state, 1, MPI_UNSIGNED_CHAR, 0, comm); 
    
    if (state != UNIQUE_SOLUTION) return vector<double>(n, 0.0); 
    
    // 3. Backward substitution 
    for (int k = n - 1; k >= 0; k--) { 
        double xk = 0.0; 
        const int k_owner = k % world_size;
        
        if (k_owner == me) { 
            double sum = 0.0; 
            // Backward substitution calculation: Use const and OpenMP for minor boost
            #pragma omp parallel for reduction(+:sum)
            for (int j = k + 1; j < n; j++) sum += A[k][j] * x[j]; 
            
            // Replaced the conditional operator with a safe check for clarity/performance
            if (fabs(A[k][k]) < EPS) {
                xk = 0.0;
            } else {
                xk = (b[k] - sum) / A[k][k];
            }
        } 
        MPI_Bcast(&xk, 1, MPI_DOUBLE, k_owner, comm); 
        x[k] = xk; 
    } 

    return x; 
}


static inline pair<int, double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me) {
    int best_row = -1;
    double best = -1.0;

    for (int i = k; i < n; i++) {
        if ((i % world_size) != me) continue;
        double v = fabs(A[i][k]);
        if (v > best) {
            best_row = i;
            best = v;
        }
    }
    return {best_row, best}; // -1 if none owned locally
}

// ... (other helper functions remain the same) ...

static inline void pack_pivot_tail(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf) {
    for (int j = k; j < n; j++) buf[j] = A[k][j];
    buf[n] = b[k];
}

static inline void unpack_pivot_tail(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf) {
    for (int j = k; j < n; j++) A[k][j] = buf[j];
    b[k] = buf[n];
}

static inline void swap_rows(vector<vector<double>>& A, vector<double>& b, int r1, int r2) {
    if (r1 == r2) return;
    swap(A[r1], A[r2]);
    swap(b[r1], b[r2]);
}

static long double parse_token(string tok) {
    auto p = tok.find('/');
    if (p == std::string::npos) return stold(tok);
    long double num = stold(tok.substr(0, p));
    long double den = stold(tok.substr(p + 1));
    if (den == 0.0L) throw runtime_error("division by zero");
    return num / den;
}


int main(int argc, char *argv[]) {
    // Optimization: Disable synchronization with C stdio and un-tie cin/cout
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    
    // Optimization: Initialize MPI_THREAD_FUNNELED for hybrid programming
    int provided_thread_level;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided_thread_level);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n = 0;
    vector<vector<double>> A;
    vector<double> b;

    // ... (rest of main function remains the same as it's I/O and setup)
    
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
                A.assign(n, vector<double>(n));
                b.assign(n, 0.0);
                for (int i = 0; i < n; i++) {
                    string s;
                    for (int j = 0; j < n; j++) {
                        file >> s;
                        A[i][j] = parse_token(s);
                    }
                    file >> s;
                    b[i] = parse_token(s);
                }
            }
        }
    }

    // Broadcast n, matrix A (flated), and vector b
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (n == 0) {
        MPI_Finalize();
        return 0; // Exit if file reading failed
    }

    if (world_rank != 0) {
        A.assign(n, vector<double>(n));
        b.assign(n, 0.0);
    }

    vector<double> flat(n*n);
    if (world_rank == 0) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                flat[i * n + j] = A[i][j];
            }
        }
    }
    MPI_Bcast(flat.data(), n*n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (world_rank != 0) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                A[i][j] = flat[i * n + j];
            }
        }
    }
    MPI_Bcast(b.data(), n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    unsigned char state;
    vector<double> x = gauss_cyclic(A, b, n, MPI_COMM_WORLD, state);
    
    if (world_rank == 0) {
        if (state == UNIQUE_SOLUTION) {
            cout.setf(std::ios::fixed);
            cout << setprecision(10);
            for (int i = 0; i < n; i++) cout << x[i] << (i+1 == n ? '\n' : ' ');
        }
        else if (state == INFINITY_SOLUTION) cout << "Infinite Solutions\n";
        else cout << "No Solution\n";
    }

    MPI_Finalize();
    return 0;
}