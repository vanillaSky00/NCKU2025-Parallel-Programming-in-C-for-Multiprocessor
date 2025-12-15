#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <stdexcept>

using namespace std;

struct ThreadCtx {
    const vector<array<long long,4>>* A_param;
    const vector<vector<long long>>* BT;   // transposed B
    vector<long long>* res;
    int tid;
    int num_threads;
};

static void* worker(void* arg) {
    auto* ctx = static_cast<ThreadCtx*>(arg);
    const auto& Ap = *ctx->A_param;
    const auto& BT = *ctx->BT;
    auto& res = *ctx->res;

    int n = (int)Ap.size();
    const int BLOCK = 64;   // cache-friendly block size

    vector<long long> Arow(n);

    for (int i = ctx->tid; i < n; i += ctx->num_threads) {

        // Build A[i][*]
        Arow[0] = Ap[i][0];
        for (int k = 1; k < n; k++) {
            Arow[k] = (Ap[i][1] * Arow[k - 1] + Ap[i][2]) % Ap[i][3];
        }

        long long x = 0;

        // Compute S_i = XOR_j (sum_k A[i][k] * B[k][j])
        for (int j = 0; j < n; j++) {
            long long sum = 0;

            for (int kk = 0; kk < n; kk += BLOCK) {
                int kend = min(n, kk + BLOCK);
                for (int k = kk; k < kend; k++) {
                    sum += Arow[k] * BT[j][k];
                }
            }
            x ^= sum;
        }

        res[i] = x;
    }

    return nullptr;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <num_threads>\n";
        return 1;
    }

    int NUM_THREADS = stoi(argv[1]);
    if (NUM_THREADS <= 0)
        throw runtime_error("num_threads must be positive");

    string file_name;
    cin >> file_name;
    ifstream file(file_name);
    if (!file)
        throw runtime_error("cannot open input file");

    int n;
    file >> n;

    // Read parameters
    vector<array<long long,4>> A_param(n), B_param(n);
    for (int i = 0; i < n; i++)
        file >> A_param[i][0] >> A_param[i][1]
             >> A_param[i][2] >> A_param[i][3];

    for (int i = 0; i < n; i++)
        file >> B_param[i][0] >> B_param[i][1]
             >> B_param[i][2] >> B_param[i][3];

    // Precompute full B
    vector<vector<long long>> B(n, vector<long long>(n));
    for (int i = 0; i < n; i++) {
        B[i][0] = B_param[i][0];
        for (int j = 1; j < n; j++) {
            B[i][j] = (B_param[i][1] * B[i][j - 1] + B_param[i][2]) % B_param[i][3];
        }
    }

    // Transpose B -> BT
    vector<vector<long long>> BT(n, vector<long long>(n));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            BT[j][i] = B[i][j];

    vector<long long> res(n, 0);

    vector<pthread_t> threads(NUM_THREADS);
    vector<ThreadCtx> ctx(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; t++) {
        ctx[t] = ThreadCtx{&A_param, &BT, &res, t, NUM_THREADS};
        int rc = pthread_create(&threads[t], nullptr, worker, &ctx[t]);
        if (rc != 0)
            throw runtime_error("pthread_create failed");
    }

    for (int t = 0; t < NUM_THREADS; t++)
        pthread_join(threads[t], nullptr);

    for (auto v : res)
        cout << v << "\n";

    return 0;
}