#include <bits/stdc++.h>
#include <mpi.h>
using namespace std;

#define UNIQUE_SOLUTION 0
#define INFINITY_SOLUTION 1
#define NO_SOLUTION 2

static constexpr double EPS = 1e-12; static inline pair<int, double> 

local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me); 
static inline void swap_rows(vector<vector<double>>& A, vector<double>& b, int r1, int r2); 
static inline void pack_pivot_tail(const vector<vector<double>>& A, const vector<double>& b, int k, int n, vector<double>& buf); 
static inline void unpack_pivot_tail(vector<vector<double>>& A, vector<double>& b, int k, int n, const vector<double>& buf); 

vector<double> gauss_cyclic(vector<vector<double>>& A, vector<double>& b, int n, MPI_Comm comm, unsigned char& state) { 
    int world_size, me; MPI_Comm_size(comm, &world_size); MPI_Comm_rank(comm, &me); vector<double> x(n), buf(n+1); 
    // Forward elimination 
    for (int k = 0; k < n - 1; k++) {//why n -1 
        // 1. Local pivot selection 
        auto [loc_piv, loc_val] = local_pivot_candidate(A, k, n, world_size, me); 
        
        // 2. Global pivot selection 
        struct { double val; int idx; } in { loc_val, loc_piv } , out {}; 
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm); 
        
        const int pivot_row = out.idx; 
        const bool singular_col = (out.val < EPS) || (pivot_row < 0); 
        const int pivot_owner = pivot_row < 0 ? 0 : pivot_row % world_size; 
        
        // 3. Pivot row exchange 
        if (!singular_col) swap_rows(A, b, pivot_row, k); 
        // 4. Pivot row (tail only) broadcast 
        if (!singular_col) {
            if (me == pivot_owner) pack_pivot_tail(A, b, k, n, buf); 
        } 
        else { 
            // column is (near) zero ⇒ broadcast zeros so everyone proceeds safely 
            if (me == pivot_owner) for (int j = k; j <= n; j++) buf[j] = 0.0; 
        } 
        MPI_Bcast(buf.data()+k, (n - k + 1), MPI_DOUBLE, pivot_owner, comm); 
        
        // 5. eliminate my rows i = k+1, k+1+p, ... 
        if (fabs(buf[k]) >= EPS) {
            for (int i = k + 1; i < n; ++i) {
                double l = A[i][k] / buf[k];
                A[i][k] = 0.0;
                for (int j = k + 1; j < n; ++j) A[i][j] -= l * buf[j];
                b[i] -= l * buf[n];
            }
        }
    } 
    
    // 6. After elimination, check the three cases with rank 
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
    
    // 7. backward substitution 
    for (int k = n - 1; k >= 0; k--) { 
        double xk = 0.0; 
        if (k % world_size == me) { 
            double sum = 0.0; 
            for (int j = k + 1; j < n; j++) sum += A[k][j] * x[j]; 
            xk = (fabs(A[k][k]) < EPS) ? 0.0 : (b[k] - sum) / A[k][k]; // handle singluar 
        } 
        MPI_Bcast(&xk, 1, MPI_DOUBLE, k % world_size, comm); 
        x[k] = xk; 
    } 

    return x; 
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
        std::string file_name;
        std::cin >> file_name;
        std::ifstream file(file_name);
        
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

    // Broadcast n, matrix A (flated), and vector b
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
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


static inline pair<int, double> local_pivot_candidate(const vector<vector<double>>& A, int k, int n, int world_size, int me) {
    int best_row = -1;
    double best = 0.0;

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