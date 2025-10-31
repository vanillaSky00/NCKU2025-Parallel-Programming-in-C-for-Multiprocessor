#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <mpi.h>
#include <iomanip>
#include <fstream>
#include <string>
#include <cctype>

using namespace std;

#define UNIQUE_SOLUTION 0
#define INFINITY_SOLUTION 1
#define NO_SOLUTION 2

static constexpr double EPS = 1e-12; 
static constexpr int TAG_PIVOT = 42;

static inline pair<int, double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me); 
static inline void swap_rows(vector<vector<double>>& A, vector<double>& b, int r1, int r2); 
static inline void pack_pivot_tail(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf); 
static inline void unpack_pivot_tail(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf); 
static long double parse_token(string tok);


vector<double> gauss_cyclic(vector<vector<double>>& A, vector<double>& b, int n, MPI_Comm comm, unsigned char& state) { 
    int world_size, me; 
    MPI_Comm_size(comm, &world_size); 
    MPI_Comm_rank(comm, &me); 
    
    // buf is for the pivot row tail (A[k..n-1], b[n]), tmp is for communication
    vector<double> x(n), buf(n+1), tmp(n+1); 

    // 1. Forward elimination 
    for (int k = 0; k < n - 1; k++) {
        
        auto [loc_piv, loc_val] = local_pivot_candidate(A, k, n, world_size, me); 
        
        // b. Global pivot selection 
        struct { double val; int idx; } in { loc_val, loc_piv } , out {}; 
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm); 
        
        const int pivot_row     = out.idx; 
        const bool singular_col = (out.val < EPS) || (pivot_row < 0); 
        const int pivot_owner   = pivot_row < 0 ? 0 : pivot_row % world_size; 
        const int k_owner = k % world_size;

        if (!singular_col) {
            // c. Pivot Row Exchange and Broadcast Preparation
            if (pivot_owner == me && k_owner == me) {
                //case 1: pivot and k on same rank → do local swap → copy to buffer
                if (pivot_row != k) swap_rows(A, b, pivot_row, k);
                pack_pivot_tail(A, b, k, n, buf); // Row k (the pivot row) goes into buf
            }
            else {
                // case 2a: I am k-owner → send my row k, receive pivot → overwrite my k
                if (k_owner == me) {
                    
                    pack_pivot_tail(A, b, k, n, tmp); // tmp contains row k data (to send)
                    
                    MPI_Sendrecv(tmp.data() + k, n - k + 1, MPI_DOUBLE, pivot_owner, TAG_PIVOT,
                                buf.data() + k, n - k + 1, MPI_DOUBLE, pivot_owner, TAG_PIVOT,
                                comm, MPI_STATUS_IGNORE);

                    // buf now holds the data for the new row k (the old pivot_row), ready for Bcast
                    unpack_pivot_tail(A, b, k, n, buf); 
                }
                else if (pivot_owner == me) {
                    // case 2b: I am pivot-owner → send pivot, receive row k → overwrite pivot-row
                    pack_pivot_tail(A, b, pivot_row, n, buf); // buf contains pivot_row data (to send)
                    
                    MPI_Sendrecv(buf.data() + k, n - k + 1, MPI_DOUBLE, k_owner, TAG_PIVOT,
                                tmp.data() + k, n - k + 1, MPI_DOUBLE, k_owner, TAG_PIVOT,
                                comm, MPI_STATUS_IGNORE);
                    
                    // Update local pivot_row with received row k (now in tmp)
                    // buf already holds the old pivot_row data, which is the new row k data, ready for Bcast
                    unpack_pivot_tail(A, b, pivot_row, n, tmp);

                }
            }
        } 
        else {
            // Singular column: root must zero the buf for consistency before Bcast
            if (me == (k_owner)) std::fill(buf.begin() + k, buf.end(), 0.0);
        }

        MPI_Bcast(buf.data() + k, n - k + 1, MPI_DOUBLE, pivot_owner, comm);

        if ((k_owner != pivot_owner) && (me == k_owner))
            unpack_pivot_tail(A, b, k, n, buf);

        // f. Elimination on my rows i = k+1, k+1+p, ... 
        int i = k + 1;
        while (i < n && (i % world_size) != me) i++;
        if (fabs(buf[k]) >= EPS) {
            for (; i < n; i+= world_size) {
                double l = A[i][k] / buf[k];
                A[i][k] = 0.0; // Implicitly zeroed
                for (int j = k + 1; j < n; ++j) A[i][j] -= l * buf[j];
                b[i] -= l * buf[n];
            }
        }
    } 
    
    // 2. Check for singularity/rank
    state = UNIQUE_SOLUTION; 
    if (me == 0) { 
        // Note: This rank check is simplified and only works if all processors have a full copy of A/b.
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
        if (k % world_size == me) { 
            double sum = 0.0; 
            for (int j = k + 1; j < n; j++) sum += A[k][j] * x[j]; 
            xk = (fabs(A[k][k]) < EPS) ? 0.0 : (b[k] - sum) / A[k][k]; 
        } 
        MPI_Bcast(&xk, 1, MPI_DOUBLE, k % world_size, comm); 
        x[k] = xk; 
    } 

    return x; 
}


static inline pair<int, double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me) {
    int best_row = -1;
    double best = -std::numeric_limits<double>::infinity();

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

static long double parse_token(std::string tok) {
    // 1) trim left
    while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t'))
        tok.erase(tok.begin());
    // 2) trim right (include \r from Windows files)
    while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t' || tok.back() == '\r' || tok.back() == '\n'))
        tok.pop_back();

    if (tok.empty()) return 0.0L;  // defensive

    auto p = tok.find('/');

    try {
        if (p == std::string::npos) {
            // plain number
            return std::stold(tok);
        } else {
            // fraction: a/b
            std::string num_s = tok.substr(0, p);
            std::string den_s = tok.substr(p + 1);

            // trim right on both parts just in case
            while (!num_s.empty() && (num_s.back() == ' ' || num_s.back() == '\t' || num_s.back() == '\r'))
                num_s.pop_back();
            while (!den_s.empty() && (den_s.back() == ' ' || den_s.back() == '\t' || den_s.back() == '\r'))
                den_s.pop_back();

            long double num = std::stold(num_s);
            long double den = std::stold(den_s);
            if (den == 0.0L) throw std::runtime_error("division by zero");
            return num / den;
        }
    } catch (const std::invalid_argument&) {
        // bad token → don't crash in judge
        // cerr << "bad token: '" << tok << "'\n";
        return 0.0L;
    } catch (const std::out_of_range&) {
        return 0.0L;
    }
}



int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    MPI_Init(&argc, &argv);
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n = 0;
    vector<vector<double>> A;
    vector<double> b;

    if (world_rank == 0) {
        string file_name;
        cin >> file_name;
        ifstream file(file_name);

        file >> n;
        A.assign(n, vector<double>(n, 0.0));
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


    // Broadcast n, matrix A (flated), and vector b
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // cout << "(rank,n)" << world_rank << "," << n << "\n";

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
            cout << setprecision(8);
            for (int i = 0; i < n; i++) cout << x[i] << (i+1 == n ? '\n' : ' ');
        }
        else if (state == INFINITY_SOLUTION) cout << "Infinite Solutions\n";
        else cout << "No Solution\n";
    }

    // cout << world_rank << "\n";
    // for (int i = 0; i < n; i++) {
    //     for (int j = 0; j < n; j++) {
    //         cout << A[i][j] << " ";
    //     }
    //     cout << "\n";
    // }

    MPI_Finalize();
    return 0;
}