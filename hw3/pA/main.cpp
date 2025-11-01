#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <mpi.h>
#include <iomanip>
#include <fstream>
#include <string>

using namespace std;

#define UNIQUE_SOLUTION 0
#define INFINITY_SOLUTION 1
#define NO_SOLUTION 2

static constexpr double EPS = 1e-12;

static long double parse_token(string tok);
static inline pair<int,double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int p, int me);
static inline void exchange_row(vector<vector<double>>& A, vector<double>& b, int r1, int r2);
static inline void copy_row(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf);
static inline void copy_back_row(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf);
static inline void copy_exchange_row(vector<vector<double>>& A, vector<double>& b, int r, int k, int n, vector<double>& buf);


vector<double> gauss_cyclic(vector<vector<double>>& A, vector<double>& b, int n, MPI_Comm comm, unsigned char& state) {
    int p, me;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &me);

    vector<double> x(n);
    vector<double> buf(n + 1);

    for (int k = 0; k < n; k++) {
        auto [loc_row, loc_val] = local_pivot_candidate(A, k, n, p, me);

        struct { double val; int row; } in, out;
        in.val = loc_val;
        in.row = (loc_row == -1 ? -1 : loc_row);
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm);

        double pivot_val = out.val;
        int pivot_row = out.row;
        int k_owner = k % p;

        if (pivot_val < EPS) {
            if (me == k_owner) {
                for (int j = k; j < n; j++) buf[j] = 0.0;
                buf[n] = 0.0;
            }
            // Because a_kk = 0 so broadcast all zero buffer, others won't be subtracted
            MPI_Bcast(buf.data() + k, n - k + 1, MPI_DOUBLE, k_owner, comm);
        } 
        else {
            int pivot_owner = pivot_row % p;

            if (pivot_owner == k_owner) {
                if (me == pivot_owner) {
                    if (pivot_row != k) exchange_row(A, b, pivot_row, k);
                    copy_row(A, b, k, n, buf);
                }
            }
            else {
                if (me == k_owner) {
                    copy_row(A, b, k, n, buf);
                    MPI_Send(buf.data() + k, n - k + 1, MPI_DOUBLE,
                             pivot_owner, 42, comm);
                }
                else if (me == pivot_owner) {
                    MPI_Status st;
                    MPI_Recv(buf.data() + k, n - k + 1, MPI_DOUBLE,
                             k_owner, 42, comm, &st);
                    copy_exchange_row(A, b, pivot_row, k, n, buf);
                }
            }
            MPI_Bcast(buf.data() + k, n - k + 1, MPI_DOUBLE, pivot_owner, comm);
        }

        if ((k % p != (pivot_val < EPS ? k_owner : pivot_row % p)) && (k % p == me)) {
            copy_back_row(A, b, k, n, buf);
        }

        int i_start = k + 1;
        while (i_start < n && (i_start % p) != me) i_start++;

        for (int i = i_start; i < n; i += p) {
            if (fabs(buf[k]) > EPS) {
                double l = A[i][k] / buf[k];
                A[i][k] = 0.0;
                for (int j = k + 1; j < n; j++) {
                    A[i][j] -= l * buf[j];
                }
                b[i] -= l * buf[n];
            } 
            else {
                A[i][k] = 0.0;
            }
        }
    }

    // For every global row i find who really owns it: owner = i % p
    // If I am the owner and I am not rank 0 → send the row to rank 0
    if (p > 1) {
        for (int i = 0; i < n; i++) {
            int owner = i % p;
            if (me == owner && me != 0) {
                vector<double> row_data(n + 1);
                for (int j = 0; j < n; j++) row_data[j] = A[i][j];
                row_data[n] = b[i];
                MPI_Send(row_data.data(), n + 1, MPI_DOUBLE, 0, i, comm);
            } 
            else if (me == 0 && me != owner) {
                vector<double> row_data(n + 1);
                MPI_Status st;
                MPI_Recv(row_data.data(), n + 1, MPI_DOUBLE, owner, i, comm, &st);
                for (int j = 0; j < n; j++) A[i][j] = row_data[j];
                b[i] = row_data[n];
            }
        }
    }

    state = UNIQUE_SOLUTION;
    if (me == 0) {
        int rankA = 0;
        int rankAb = 0;

        for (int i = 0; i < n; i++) {
            bool row_has_non_zero_in_A = false;
            for (int j = i; j < n; j++) {
                if (fabs(A[i][j]) > EPS) {
                    row_has_non_zero_in_A = true;
                    break;
                }
            }

            if (row_has_non_zero_in_A) {
                rankA++;
                rankAb++;
            } else {
                if (fabs(b[i]) > EPS) {
                    rankAb++;
                }
            }
        }

        if (rankA != rankAb)      state = NO_SOLUTION;
        else if (rankA < n)       state = INFINITY_SOLUTION;
        else                      state = UNIQUE_SOLUTION;
    }
    MPI_Bcast(&state, 1, MPI_UNSIGNED_CHAR, 0, comm);
    if (state != UNIQUE_SOLUTION) {
        return vector<double>(n, 0.0);
    }

    for (int k = n - 1; k >= 0; k--) {
        double xk = 0.0;
        int k_owner_backsub = k % p;
        if (k_owner_backsub == me) {
            double sum = 0.0;
            for (int j = k + 1; j < n; j++) sum += A[k][j] * x[j];
            xk = (fabs(A[k][k]) < EPS) ? 0.0 : (b[k] - sum) / A[k][k];
        }
        MPI_Bcast(&xk, 1, MPI_DOUBLE, k_owner_backsub, comm);
        x[k] = xk;
    }

    return x;
}

int main(int argc, char *argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
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
        ifstream fin(file_name);
        fin >> n;
        A.assign(n, vector<double>(n));
        b.resize(n);
        for (int i = 0; i < n; i++) {
            string s;
            for (int j = 0; j < n; j++) {
                fin >> s;
                A[i][j] = (double)parse_token(s);
            }
            fin >> s;
            b[i] = (double)parse_token(s);
        }
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (n == 0) {
        MPI_Finalize();
        return 0;
    }

    if (world_rank != 0) {
        A.assign(n, vector<double>(n));
        b.resize(n);
    }

    vector<double> flat_A(n * n);
    if (world_rank == 0) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                flat_A[i * n + j] = A[i][j];
    }
    MPI_Bcast(flat_A.data(), n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (world_rank != 0) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                A[i][j] = flat_A[i * n + j];
    }
    MPI_Bcast(b.data(), n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    unsigned char state_result;
    vector<double> x = gauss_cyclic(A, b, n, MPI_COMM_WORLD, state_result);

    if (world_rank == 0) {
        if (state_result == UNIQUE_SOLUTION) {
            cout.setf(ios::fixed);
            cout << setprecision(8);
            for (int i = 0; i < n; ++i) cout << x[i] << (i + 1 == n ? '\n' : ' ');
        } 
        else if (state_result == INFINITY_SOLUTION) cout << "Infinite Solutions\n";
        else cout << "No Solution\n";
    }

    MPI_Finalize();
    return 0;
}


static long double parse_token(string tok) {
    auto p = tok.find('/');
    if (p == std::string::npos) return stold(tok);
    long double num = stold(tok.substr(0, p));
    long double den = stold(tok.substr(p + 1));
    if (den == 0.0L) throw runtime_error("Division by zero");
    return num / den;
}

static inline pair<int,double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int p, int me) {
    
    int best_row = -1;
    double best_val = 0.0;
    for (int i = k; i < n; i++) {
        if (i % p != me) continue;
        double v = fabs(A[i][k]);
        if (v > best_val) {
            best_val = v;
            best_row = i;
        }
    }
    return {best_row, best_val};
}

static inline void exchange_row(vector<vector<double>>& A, vector<double>& b, int r1, int r2) {
    if (r1 == r2) return;
    swap(A[r1], A[r2]);
    swap(b[r1], b[r2]);
}

static inline void copy_row(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf){
    for (int j = k; j < n; j++) buf[j] = A[k][j];
    buf[n] = b[k];
}

static inline void copy_back_row(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf){
    for (int j = k; j < n; j++) A[k][j] = buf[j];
    b[k] = buf[n];
}

static inline void copy_exchange_row(vector<vector<double>>& A, vector<double>& b, int r, int k, int n, vector<double>& buf) {
    for (int j = k; j < n; j++) {
        double tmp = A[r][j];
        A[r][j] = buf[j];
        buf[j] = tmp;
    }
    double tb = b[r];
    b[r] = buf[n];
    buf[n] = tb;
}