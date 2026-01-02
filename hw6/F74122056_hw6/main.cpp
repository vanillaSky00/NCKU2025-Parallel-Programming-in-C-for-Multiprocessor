#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

using namespace std;

// -------------------- Fast Scanner (FILE*) --------------------
struct FastScanner {
    static constexpr size_t BUFSZ = 1 << 20;
    FILE* f;
    char buf[BUFSZ];
    size_t idx = 0, size = 0;

    explicit FastScanner(FILE* file) : f(file) {}

    inline char readChar() {
        if (idx >= size) {
            size = fread(buf, 1, BUFSZ, f);
            idx = 0;
            if (size == 0) return 0;
        }
        return buf[idx++];
    }

    inline void skipSpaces() {
        char c;
        while ((c = readChar()) && isspace((unsigned char)c)) {}
        if (c) idx--;
    }

    // read token as string (for first token)
    string readToken() {
        skipSpaces();
        string s;
        char c;
        while ((c = readChar()) && !isspace((unsigned char)c)) s.push_back(c);
        return s;
    }

    // read signed int (here all inputs are non-negative, but keep generic)
    int readInt() {
        skipSpaces();
        char c = readChar();
        int sign = 1;
        if (c == '-') { sign = -1; c = readChar(); }
        int x = 0;
        while (c && isdigit((unsigned char)c)) {
            x = x * 10 + (c - '0');
            c = readChar();
        }
        return x * sign;
    }
};

// -------------------- Thread Context --------------------
struct ThreadCtx {
    int tid;
    int num_threads;
    int n;
    const int32_t* A;      // A is row-major, size n*n
    const int32_t* BT;     // BT is row-major where BT[j*n + k] == B[k*n + j]
    uint64_t thread_xor;
};

static void* worker(void* arg) {
    auto* ctx = static_cast<ThreadCtx*>(arg);
    const int n = ctx->n;
    const int32_t* A = ctx->A;
    const int32_t* BT = ctx->BT;

    // Give each thread a contiguous range of rows (better locality than stride)
    int rows_per = (n + ctx->num_threads - 1) / ctx->num_threads;
    int i0 = ctx->tid * rows_per;
    int i1 = min(n, i0 + rows_per);

    uint64_t local_x = 0;

    for (int i = i0; i < i1; i++) {
        const int32_t* arow = A + (int64_t)i * n;

        uint64_t row_x = 0;
        int j = 0;

        // unroll 4 columns
        for (; j + 3 < n; j += 4) {
            const int32_t* b0 = BT + (int64_t)(j + 0) * n;
            const int32_t* b1 = BT + (int64_t)(j + 1) * n;
            const int32_t* b2 = BT + (int64_t)(j + 2) * n;
            const int32_t* b3 = BT + (int64_t)(j + 3) * n;

            int64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;

            // inner dot
            for (int k = 0; k < n; k++) {
                int64_t a = arow[k];
                s0 += a * b0[k];
                s1 += a * b1[k];
                s2 += a * b2[k];
                s3 += a * b3[k];
            }

            // XOR of 4 results (equivalent to XOR each separately)
            row_x ^= (uint64_t)s0 ^ (uint64_t)s1 ^ (uint64_t)s2 ^ (uint64_t)s3;
        }

        // remaining columns
        for (; j < n; j++) {
            const int32_t* b = BT + (int64_t)j * n;
            int64_t s = 0;
            for (int k = 0; k < n; k++) {
                s += (int64_t)arow[k] * b[k];
            }
            row_x ^= (uint64_t)s;
        }

        local_x ^= row_x;
    }

    ctx->thread_xor = local_x;
    return nullptr;
}

static inline bool isNumberToken(const string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!isdigit((unsigned char)c)) return false;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num_threads>\n", argv[0]);
        return 1;
    }
    int T = atoi(argv[1]);
    if (T <= 0) T = 1;

    // Read first token from stdin: could be n OR a filename (for your test.py)
    FastScanner fs_stdin(stdin);
    string first = fs_stdin.readToken();
    if (first.empty()) return 0;

    FILE* in = stdin;
    FastScanner* fs = &fs_stdin;
    FastScanner* fs_file = nullptr;

    int n = 0;

    if (isNumberToken(first)) {
        // OJ mode: first token is n
        n = atoi(first.c_str());
    } else {
        // test.py mode: first token is a filename
        in = fopen(first.c_str(), "rb");
        if (!in) {
            fprintf(stderr, "cannot open input file: %s\n", first.c_str());
            return 1;
        }
        fs_file = new FastScanner(in);
        fs = fs_file;
        n = fs->readInt();
    }

    if (n <= 0) {
        fprintf(stderr, "invalid n\n");
        if (fs_file) { fclose(in); delete fs_file; }
        return 1;
    }

    // Read A and B matrices (n*n each)
    // A,B <= 1000, int32 is enough
    vector<int32_t> A((int64_t)n * n);
    vector<int32_t> B((int64_t)n * n);

    for (int64_t i = 0; i < (int64_t)n * n; i++) A[i] = (int32_t)fs->readInt();
    for (int64_t i = 0; i < (int64_t)n * n; i++) B[i] = (int32_t)fs->readInt();

    // Build BT (transpose of B) so BT[j*n + k] = B[k*n + j]
    vector<int32_t> BT((int64_t)n * n);
    for (int i = 0; i < n; i++) {
        const int32_t* brow = B.data() + (int64_t)i * n;
        for (int j = 0; j < n; j++) {
            BT[(int64_t)j * n + i] = brow[j];
        }
    }

    // Threads
    vector<pthread_t> th(T);
    vector<ThreadCtx> ctx(T);

    for (int t = 0; t < T; t++) {
        ctx[t] = ThreadCtx{t, T, n, A.data(), BT.data(), 0};
        int rc = pthread_create(&th[t], nullptr, worker, &ctx[t]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed\n");
            if (fs_file) { fclose(in); delete fs_file; }
            return 1;
        }
    }

    uint64_t ans = 0;
    for (int t = 0; t < T; t++) {
        pthread_join(th[t], nullptr);
        ans ^= ctx[t].thread_xor;
    }

    printf("%llu\n", (unsigned long long)ans);

    if (fs_file) {
        fclose(in);
        delete fs_file;
    }
    return 0;
}
