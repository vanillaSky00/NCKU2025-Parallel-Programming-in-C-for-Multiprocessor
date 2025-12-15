#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <stdexcept>

struct ThreadCtx {
    const std::vector<std::array<long long, 4>>* A_param;
    const std::vector<std::array<long long, 4>>* B_param;
    std::vector<long long>* res;
    int tid;
    int num_threads;
};

static void* worker(void* arg) {
    auto* ctx = static_cast<ThreadCtx*>(arg);
    const auto& Ap = *ctx->A_param;
    const auto& Bp = *ctx->B_param;
    auto& res = *ctx->res;
    int n = (int)Ap.size();

    // Compute rows owned by this thread: i = tid, tid+T, tid+2T...
    for (int i = ctx->tid; i < n; i += ctx->num_threads) {

        // Generate row i of A
        std::vector<long long> Arow(n);
        Arow[0] = Ap[i][0];
        for (int j = 1; j < n; j++) {
            Arow[j] = (Ap[i][1] * Arow[j - 1] + Ap[i][2]) % Ap[i][3];
        }

        long long x = 0;

        // For each column j, compute C[i][j] = sum_k A[i][k] * B[k][j]
        // (This is still O(n^3), just showing pthread-correct structure)
        for (int j = 0; j < n; j++) {
            long long sum = 0;
            for (int k = 0; k < n; k++) {
                // Generate B[k][j] on the fly from B params (avoid full matrix B)
                long long bkj;
                if (j == 0) bkj = Bp[k][0];
                else {
                    long long prev = Bp[k][0];
                    for (int t = 1; t <= j; t++)
                        prev = (Bp[k][1] * prev + Bp[k][2]) % Bp[k][3];
                    bkj = prev;
                }

                sum += Arow[k] * bkj;
            }
            x ^= sum;
        }

        res[i] = x;
    }

    return nullptr;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <num_threads>\n";
        return 1;
    }
    int NUM_THREADS = std::stoi(argv[1]);
    if (NUM_THREADS <= 0) throw std::runtime_error("num_threads must be > 0");

    std::string file_name;
    std::cin >> file_name;
    std::ifstream file(file_name);
    if (!file) throw std::runtime_error("cannot open input file");

    int n;
    file >> n;

    std::vector<std::array<long long,4>> A_param(n), B_param(n);
    for (int i = 0; i < n; i++)
        file >> A_param[i][0] >> A_param[i][1] >> A_param[i][2] >> A_param[i][3];
    for (int i = 0; i < n; i++)
        file >> B_param[i][0] >> B_param[i][1] >> B_param[i][2] >> B_param[i][3];

    std::vector<long long> res(n, 0);

    std::vector<pthread_t> threads(NUM_THREADS);
    std::vector<ThreadCtx> ctx(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; t++) {
        ctx[t] = ThreadCtx{&A_param, &B_param, &res, t, NUM_THREADS};
        int rc = pthread_create(&threads[t], nullptr, worker, &ctx[t]);
        if (rc != 0) throw std::runtime_error("pthread_create failed");
    }

    for (int t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], nullptr);
    }

    for (auto v : res) std::cout << v << "\n";
    return 0;
}
