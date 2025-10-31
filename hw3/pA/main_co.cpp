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
static constexpr int TAG_PIVOT_DATA = 42; // Tag for sending/receiving pivot row data
static constexpr int TAG_ROW_K_DATA = 43; // Tag for sending/receiving row k data


static inline pair<int, double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me);
static inline void swap_rows(vector<vector<double>>& A, vector<double>& b, int r1, int r2);
static inline void pack_pivot_tail(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf);
static inline void unpack_pivot_tail(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf);
static long double parse_token(string tok);


vector<double> gauss_cyclic(vector<vector<double>>& A, vector<double>& b, int n, MPI_Comm comm, unsigned char& state) {
    int world_size, me;
    MPI_Comm_size(comm, &world_size);
    MPI_Comm_rank(comm, &me);

    vector<double> x(n);
    // Buffer to hold the pivot row for broadcast to all processes
    vector<double> pivot_row_buf(n + 1);

    // Forward elimination
    for (int k = 0; k < n - 1; k++) {

        auto [loc_piv, loc_val] = local_pivot_candidate(A, k, n, world_size, me);


        struct { double val; int idx; } in { loc_val, loc_piv } , out {};
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm);

        const int pivot_row     = out.idx; 
        const bool singular_col = (out.val < EPS) || (pivot_row < 0);         // Check for singular column
        const int pivot_owner   = pivot_row < 0 ? 0 : pivot_row % world_size; // Rank that owns the pivot row
        const int k_owner       = k % world_size;                             // Rank that owns the current working row 'k'

        if (!singular_col) {
            // This block handles the exchange and preparation of the pivot row for broadcast.
            // Only 'k_owner' and 'pivot_owner' need to actively participate in Sendrecv.
            // All other ranks will simply wait for the Bcast.

            if (k_owner == me) { 
                if (pivot_owner == me) { 
                    if (pivot_row != k) swap_rows(A, b, pivot_row, k);
                    // Row 'k' now contains the pivot row. Pack it into the buffer for broadcast.
                    pack_pivot_tail(A, b, k, n, pivot_row_buf);
                } else { // I own row 'k', but the pivot row is on a different process ('pivot_owner')
                    // Send my current row 'k' data to 'pivot_owner'.
                    // Receive the actual pivot row data from 'pivot_owner' into my 'pivot_row_buf'.
                    vector<double> my_k_row_data(n + 1);
                    pack_pivot_tail(A, b, k, n, my_k_row_data);

                    MPI_Sendrecv(my_k_row_data.data() + k, n - k + 1, MPI_DOUBLE, pivot_owner, TAG_ROW_K_DATA,
                                 pivot_row_buf.data() + k, n - k + 1, MPI_DOUBLE, pivot_owner, TAG_PIVOT_DATA,
                                 comm, MPI_STATUS_IGNORE);

                    // 'pivot_row_buf' now contains the actual pivot row. Update my local row 'k' with this data.
                    unpack_pivot_tail(A, b, k, n, pivot_row_buf);
                }
            } else if (pivot_owner == me) { // I own the pivot row, but not row 'k' (k_owner is different)
                // Pack my pivot row data into 'pivot_row_buf' (to send to k_owner).
                // Receive k_owner's original row 'k' data into a temporary buffer.
                vector<double> received_k_row_data(n + 1);
                pack_pivot_tail(A, b, pivot_row, n, pivot_row_buf);

                MPI_Sendrecv(pivot_row_buf.data() + k, n - k + 1, MPI_DOUBLE, k_owner, TAG_PIVOT_DATA,
                                 received_k_row_data.data() + k, n - k + 1, MPI_DOUBLE, k_owner, TAG_ROW_K_DATA,
                                 comm, MPI_STATUS_IGNORE);

                // Update my local 'pivot_row' (which was originally the pivot_row) with the data received from k_owner.
                unpack_pivot_tail(A, b, pivot_row, n, received_k_row_data);
                // 'pivot_row_buf' still holds the actual pivot row data from *before* the Sendrecv,
                // which is correct for the upcoming broadcast.
            }
            // Processes that are neither k_owner nor pivot_owner do nothing in this conditional block.
            // Their 'pivot_row_buf' will be filled by the Bcast.
        }
        else { // Singular column: The pivot element is essentially zero.
            // The process designated as the root for the upcoming Bcast (pivot_owner)
            // must ensure its 'pivot_row_buf' is zeroed out for consistency.
            if (me == pivot_owner) {
                std::fill(pivot_row_buf.begin() + k, pivot_row_buf.end(), 0.0);
            }
        }

        // c. Broadcast the pivot row to all processes.
        // All processes will receive the pivot row (or zeroed data if singular) into their 'pivot_row_buf'.
        MPI_Bcast(pivot_row_buf.data() + k, n - k + 1, MPI_DOUBLE, pivot_owner, comm);

        // f. Elimination step on current process's assigned rows.
        // Iterate over rows 'i' that this process owns, starting from 'k+1'.
        int i = k + 1;
        while (i < n && (i % world_size) != me) i++; 
        if (fabs(pivot_row_buf[k]) >= EPS) { 
            for (; i < n; i+= world_size) { // Loop through all rows this process owns
                double l = A[i][k] / pivot_row_buf[k]; 
                A[i][k] = 0.0; // The element A[i][k] is implicitly zeroed
                for (int j = k + 1; j < n; ++j) {
                    A[i][j] -= l * pivot_row_buf[j];
                }
                b[i] -= l * pivot_row_buf[n]; 
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
            if (!zeroA) rankA++;                 // Row is not all zeros, so it contributes to rankA
            else if (fabs(b[i]) > EPS) rankAb++; // Row is all zeros in A, but b is non-zero (inconsistent)
        }

        if (rankAb > 0) state = NO_SOLUTION;
        else if (rankA < n) state = INFINITY_SOLUTION;
        else state = UNIQUE_SOLUTION;
    }
    MPI_Bcast(&state, 1, MPI_UNSIGNED_CHAR, 0, comm); // Broadcast the system state

    if (state != UNIQUE_SOLUTION) {
        return vector<double>(n, 0.0); // If no unique solution, return a zero vector
    }

    // 3. Backward substitution
    // Processes solve for their owned components of x in reverse order.
    // Each solved x[k] is broadcast to all processes.
    for (int k = n - 1; k >= 0; k--) {
        double xk = 0.0;
        if (k % world_size == me) { // If I own this row 'k'
            double sum = 0.0;
            for (int j = k + 1; j < n; j++) {
                sum += A[k][j] * x[j]; 
            }
            xk = (fabs(A[k][k]) < EPS) ? 0.0 : (b[k] - sum) / A[k][k];
        }
        // Broadcast x[k] to all processes, so everyone has the updated solution vector
        MPI_Bcast(&xk, 1, MPI_DOUBLE, k % world_size, comm);
        x[k] = xk;
    }

    return x;
}


static inline pair<int, double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me) {
    int best_row = -1;
    double best = -std::numeric_limits<double>::infinity();

    for (int i = k; i < n; i++) {
        if ((i % world_size) != me) continue; // Only consider rows owned by 'me'
        double v = fabs(A[i][k]);
        if (v > best) {
            best_row = i;
            best = v;
        }
    }
    return {best_row, best}; // Returns -1 if no rows are owned locally from k to n-1
}


static inline void swap_rows(vector<vector<double>>& A, vector<double>& b, int r1, int r2) {
    if (r1 == r2) return;
    swap(A[r1], A[r2]);
    swap(b[r1], b[r2]);
}


static inline void pack_pivot_tail(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf) {
    for (int j = k; j < n; j++) buf[j] = A[k][j];
    buf[n] = b[k]; 
}


static inline void unpack_pivot_tail(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf) {
    for (int j = k; j < n; j++) A[k][j] = buf[j];
    b[k] = buf[n]; 
}

// Parses a string token, handling fractions and trimming whitespace.
static long double parse_token(std::string tok) {
    // 1) trim left
    while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t'))
        tok.erase(tok.begin());
    // 2) trim right (include \r from Windows files)
    while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t' || tok.back() == '\r' || tok.back() == '\n'))
        tok.pop_back();

    if (tok.empty()) return 0.0L;

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

        if (!file.is_open()) {
            cerr << "Error: Could not open file " << file_name << endl;
            n = 0; // Signal an error to other processes
        } else {
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
            file.close();
        }
    }


    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (n == 0) { 
        MPI_Finalize();
        return 0;
    }


    if (world_rank != 0) {
        A.assign(n, vector<double>(n, 0.0));
        b.assign(n, 0.0);
    }

    // Flatten matrix A for broadcasting
    vector<double> flat_A(n * n);
    if (world_rank == 0) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                flat_A[i * n + j] = A[i][j];
            }
        }
    }

    MPI_Bcast(flat_A.data(), n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // All processes (except 0) unflatten A
    if (world_rank != 0) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                A[i][j] = flat_A[i * n + j];
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

    MPI_Finalize();
    return 0;
}