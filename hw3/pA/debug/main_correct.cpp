#include <cstring>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <string>
#include <mpi.h>

using namespace std;

#define UNIQUE_SOLUTION 0
#define INFINITY_SOLUTION 1
#define NO_SOLUTION 2

static constexpr double EPS = 1e-9;

static double parse_token(const string& s_);
static inline int start_of_rank(int r, int base, int extra);
static void build_scatterv_params(int n, int size, int base, int extra, vector<int>& cnt, vector<int>& disp);
static void forward_elimination_block(vector<double>& A, int n,
                                      int rank, int size,
                                      int base, int extra, int start,
                                      vector<double>& pivot);
static void solve_and_print_root(const vector<double>& full, int n);

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    MPI_Init(&argc, &argv);

    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n = 0;
    vector<double> full;

    if (rank == 0) {
        string path;
        if (!(cin >> path)) MPI_Abort(MPI_COMM_WORLD, 1);

        ifstream fin(path);
        if (!fin || !(fin >> n)) MPI_Abort(MPI_COMM_WORLD, 1);

        full.resize((size_t)n * (n + 1));
        string tok;
        for (int i = 0; i < n * (n + 1); ++i) {
            if (!(fin >> tok)) MPI_Abort(MPI_COMM_WORLD, 1);
            full[i] = parse_token(tok);
        }
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (n == 0) {
        MPI_Finalize();
        return 0;
    }

    int base = n / size;
    int extra = n % size;
    int my_rows = base + (rank < extra);
    int start = start_of_rank(rank, base, extra);

    vector<int> cnt, disp;
    if (rank == 0) {
        build_scatterv_params(n, size, base, extra, cnt, disp);
    }

    vector<double> A((size_t)my_rows * (n + 1), 0.0);

    MPI_Scatterv(
        rank == 0 ? full.data() : nullptr,
        rank == 0 ? cnt.data() : nullptr,
        rank == 0 ? disp.data() : nullptr,
        MPI_DOUBLE,
        A.data(),
        my_rows * (n + 1),
        MPI_DOUBLE,
        0,
        MPI_COMM_WORLD
    );

    vector<double> pivot(n + 1, 0.0);
    forward_elimination_block(A, n, rank, size, base, extra, start, pivot);

    if (rank == 0) {
        build_scatterv_params(n, size, base, extra, cnt, disp);
        full.assign((size_t)n * (n + 1), 0.0);
    }

    MPI_Gatherv(
        A.data(), my_rows * (n + 1), MPI_DOUBLE,
        rank == 0 ? full.data() : nullptr,
        rank == 0 ? cnt.data() : nullptr,
        rank == 0 ? disp.data() : nullptr,
        MPI_DOUBLE, 0, MPI_COMM_WORLD
    );

    if (rank == 0) {
        solve_and_print_root(full, n);
    }

    MPI_Finalize();
    return 0;
}

static double parse_token(const string& s_) {
    string s = s_;
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    if (l == string::npos) return 0.0;
    s = s.substr(l, r - l + 1);

    size_t slash = s.find('/');
    try {
        if (slash == string::npos) {
            return stod(s);
        } else {
            string ns = s.substr(0, slash);
            string ds = s.substr(slash + 1);
            while (!ns.empty() && isspace((unsigned char)ns.back())) ns.pop_back();
            while (!ds.empty() && isspace((unsigned char)ds.back())) ds.pop_back();
            double num = stod(ns);
            double den = stod(ds);
            if (fabs(den) < 1e-18) throw runtime_error("div0");
            return num / den;
        }
    } catch (...) {
        return 0.0;
    }
}

static inline int start_of_rank(int r, int base, int extra) {
    return r * base + (r < extra ? r : extra);
}

static void build_scatterv_params(int n, int size, int base, int extra, vector<int>& cnt, vector<int>& disp) {
    cnt.resize(size);
    disp.resize(size);
    int d = 0;
    for (int p = 0; p < size; ++p) {
        cnt[p] = (base + (p < extra)) * (n + 1);
        disp[p] = d;
        d += cnt[p];
    }
}

static void forward_elimination_block(vector<double>& A, int n, int rank, int size, int base, int extra, int start, vector<double>& pivot) {
    for (int k = 0; k < n; ++k) {
        double maxv = 0.0;
        int piv = -1;
        for (int i = 0; i < (int)(A.size() / (n + 1)); ++i) {
            int gi = start + i;
            if (gi >= k) {
                double v = fabs(A[(size_t)i * (n + 1) + k]);
                if (v > maxv) {
                    maxv = v;
                    piv = gi;
                }
            }
        }

        struct { double val; int idx; } in{maxv, piv}, out{};
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, MPI_COMM_WORLD);
        piv = out.idx;
        if (piv < 0 || out.val < EPS) {
            continue;
        }

        int owner_p = 0, owner_k = 0, acc = 0;
        for (int p = 0; p < size; ++p) {
            int rows = base + (p < extra);
            if (piv >= acc && piv < acc + rows) owner_p = p;
            if (k   >= acc && k   < acc + rows) owner_k = p;
            acc += rows;
        }

        if (owner_p != owner_k && (rank == owner_p || rank == owner_k)) {
            int rowLocal = (rank == owner_p) ? (piv - start) : (k - start);
            memcpy(pivot.data(), &A[(size_t)rowLocal * (n + 1)], (n + 1) * sizeof(double));

            int peer = (rank == owner_p) ? owner_k : owner_p;
            MPI_Sendrecv_replace(pivot.data(), n + 1, MPI_DOUBLE, peer, 1000 + k,
                                 peer, 1000 + k, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            memcpy(&A[(size_t)rowLocal * (n + 1)], pivot.data(), (n + 1) * sizeof(double));
        }
        else if (owner_p == owner_k && rank == owner_k && piv != k) {
            int ip = piv - start, ik = k - start;
            for (int j = 0; j <= n; ++j)
                swap(A[(size_t)ip * (n + 1) + j], A[(size_t)ik * (n + 1) + j]);
        }

        if (rank == owner_k) {
            int ik = k - start;
            double diag = A[(size_t)ik * (n + 1) + k];
            for (int j = k; j <= n; ++j)
                A[(size_t)ik * (n + 1) + j] /= diag;
            memcpy(pivot.data(), &A[(size_t)ik * (n + 1)], (n + 1) * sizeof(double));
        }

        MPI_Bcast(pivot.data() + k, (n + 1) - k, MPI_DOUBLE, owner_k, MPI_COMM_WORLD);

        int my_rows = (int)(A.size() / (n + 1));
        for (int i = 0; i < my_rows; ++i) {
            int gi = start + i;
            if (gi <= k) continue;
            double a = A[(size_t)i * (n + 1) + k];
            if (fabs(a) < EPS) continue;
            for (int j = k; j <= n; ++j)
                A[(size_t)i * (n + 1) + j] -= a * pivot[j];
            A[(size_t)i * (n + 1) + k] = 0.0;
        }
    }
}

static void solve_and_print_root(const vector<double>& full, int n) {
    int rA = 0, rAb = 0;
    for (int i = 0; i < n; ++i) {
        bool all0A = true;
        for (int j = 0; j < n; ++j) {
            if (fabs(full[(size_t)i * (n + 1) + j]) > EPS) { all0A = false; break; }
        }
        if (!all0A) rA++;
        if (!all0A || fabs(full[(size_t)i * (n + 1) + n]) > EPS) rAb++;
    }

    if (rA < rAb) {
        cout << "No Solution\n";
    } else if (rA < n) {
        cout << "Infinite Solutions\n";
    } else {
        vector<double> x(n, 0.0);
        for (int i = n - 1; i >= 0; --i) {
            double sum = full[(size_t)i * (n + 1) + n];
            for (int j = i + 1; j < n; ++j)
                sum -= full[(size_t)i * (n + 1) + j] * x[j];
            x[i] = sum / full[(size_t)i * (n + 1) + i];
        }
        cout.setf(ios::fixed);
        cout << setprecision(8);
        for (int i = 0; i < n; ++i) {
            if (i) cout << ' ';
            cout << x[i];
        }
        cout << '\n';
    }
}
