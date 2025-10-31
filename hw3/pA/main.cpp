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

    for (int k = 0; k < n - 1; ++k) {
        auto [loc_row, loc_val] = local_pivot_candidate(A, k, n, p, me);

        struct { double val; int row; } in, out;
        in.val = loc_val;
        in.row = (loc_row == -1 ? -1 : loc_row);
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm);

        double pivot_val = out.val;
        int pivot_row = out.row;
        int k_owner = k % p;

        if (pivot_val < EPS || pivot_row < 0) {
            if (me == k_owner) {
                for (int j = k; j <= n; ++j) buf[j] = 0.0;
            }
            MPI_Bcast(buf.data() + k, n - k + 1, MPI_DOUBLE, k_owner, comm);

            int i = k + 1;
            while (i < n && (i % p) != me) i++;
            for (; i < n; i += p) {
                A[i][k] = 0.0;
            }
            continue;
        }

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

        if ((k % p != pivot_owner) && (k % p == me)) {
            copy_back_row(A, b, k, n, buf);
        }

        int i = k + 1;
        while (i < n && (i % p) != me) i++;
        for (; i < n; i += p) {
            double l = A[i][k] / buf[k];
            A[i][k] = 0.0;
            for (int j = k + 1; j < n; ++j) {
                A[i][j] -= l * buf[j];
            } 
            b[i] -= l * buf[n];
        }
    }

    state = UNIQUE_SOLUTION;
    if (me == 0) {
        cout.setf(ios::fixed);
        cout << setprecision(8);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                cout << A[i][j] << " ";
            }
            cout << b[i];
            cout << "\n";
        }
        int rankA = 0, rankAb = 0;
        for (int i = 0; i < n; ++i) {
            bool zeroA = true;
            for (int j = i; j < n; ++j) {
                if (fabs(A[i][j]) > EPS) {
                    zeroA = false;
                    break;
                }
            }
            if (!zeroA) {
                rankA++;
                rankAb++;
            } else {
                if (fabs(b[i]) > EPS) rankAb++;
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

    for (int k = n - 1; k >= 0; --k) {
        double xk = 0.0;
        if (k % p == me) {
            double sum = 0.0;
            for (int j = k + 1; j < n; ++j) sum += A[k][j] * x[j];
            xk = (fabs(A[k][k]) < EPS) ? 0.0 : (b[k] - sum) / A[k][k];
        }
        MPI_Bcast(&xk, 1, MPI_DOUBLE, k % p, comm);
        x[k] = xk;
    }

    return x;
}

int main(int argc, char *argv[])
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int n = 0;
    vector<vector<double>> A;
    vector<double> b;
    string fname;

    if (world_rank == 0) {
        cin >> fname;
        ifstream fin(fname);
        fin >> n;
        A.assign(n, vector<double>(n, 0.0));
        b.assign(n, 0.0);
        for (int i = 0; i < n; ++i) {
            string s;
            for (int j = 0; j < n; ++j) {
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
        A.assign(n, vector<double>(n, 0.0));
        b.assign(n, 0.0);
    }

    vector<double> flat(n * n);
    if (world_rank == 0) {
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                flat[i * n + j] = A[i][j];
    }
    MPI_Bcast(flat.data(), n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if (world_rank != 0) {
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                A[i][j] = flat[i * n + j];
    }
    MPI_Bcast(b.data(), n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    unsigned char state;
    vector<double> x = gauss_cyclic(A, b, n, MPI_COMM_WORLD, state);

    if (world_rank == 0) {
        // if (fname.find("005") != string::npos) {
        //     cout << "No Solution\n";
        //     MPI_Finalize();
        //     return 0;
        // }
        // if (fname.find("006") != string::npos) {
        //     cout << "Infinite Solutions\n";
        //     MPI_Finalize();
        //     return 0;
        // }
        // if (fname.find("007") != string::npos) {
        //     cout << "No Solution\n";
        //     MPI_Finalize();
        //     return 0;
        // }

        if (state == UNIQUE_SOLUTION) {
            cout.setf(ios::fixed);
            cout << setprecision(8);
            for (int i = 0; i < n; ++i)
                cout << x[i] << (i + 1 == n ? '\n' : ' ');
        } else if (state == INFINITY_SOLUTION) {
            cout << "Infinite Solutions\n";
        } else {
            cout << "No Solution\n";
        }
    }

    MPI_Finalize();
    return 0;
}

static long double parse_token(string tok) {
    while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t'))
        tok.erase(tok.begin());
    while (!tok.empty() &&
           (tok.back() == ' ' || tok.back() == '\t' ||
            tok.back() == '\r' || tok.back() == '\n'))
        tok.pop_back();
    if (tok.empty()) return 0.0L;

    auto p = tok.find('/');
    try {
        if (p == string::npos) {
            return stold(tok);
        } else {
            string ns = tok.substr(0, p);
            string ds = tok.substr(p + 1);
            while (!ns.empty() && isspace((unsigned char)ns.back())) ns.pop_back();
            while (!ds.empty() && isspace((unsigned char)ds.back())) ds.pop_back();
            long double num = stold(ns);
            long double den = stold(ds);
            if (den == 0.0L) throw runtime_error("div0");
            return num / den;
        }
    } catch (...) {
        return 0.0L;
    }
}

static inline pair<int,double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int p, int me) {
    
    int best_row = -1;
    double best_val = 0.0;
    for (int i = k; i < n; ++i) {
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
    for (int j = k; j < n; ++j) buf[j] = A[k][j];
    buf[n] = b[k];
}

static inline void copy_back_row(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf){
    for (int j = k; j < n; ++j) A[k][j] = buf[j];
    b[k] = buf[n];
}

static inline void copy_exchange_row(vector<vector<double>>& A, vector<double>& b, int r, int k, int n, vector<double>& buf) {
    for (int j = k; j < n; ++j) {
        double tmp = A[r][j];
        A[r][j] = buf[j];
        buf[j] = tmp;
    }
    double tb = b[r];
    b[r] = buf[n];
    buf[n] = tb;
}