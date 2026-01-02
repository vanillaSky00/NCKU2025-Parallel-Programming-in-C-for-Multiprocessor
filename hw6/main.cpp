// hw6_fast.cpp
// g++ -O3 -march=native -mavx2 -pthread hw6_fast.cpp -o hw6
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>

#include <immintrin.h>

#ifdef __linux__
  #include <sched.h>
#endif

using namespace std;

// ===================== Fast Scanner =====================
// Fast text int parser, avoids isspace/isdigit overhead.
struct FastScanner {
    FILE* f;
    char* buf;
    size_t cap;
    size_t idx, size;

    explicit FastScanner(FILE* file, size_t buf_cap = (1u << 24)) // 16MB buffer
        : f(file), cap(buf_cap), idx(0), size(0) {
        buf = (char*)malloc(cap);
        if (!buf) { fprintf(stderr, "malloc failed\n"); exit(1); }
    }

    ~FastScanner() { free(buf); }

    inline char refill() {
        size = fread(buf, 1, cap, f);
        idx = 0;
        return size ? buf[idx++] : 0;
    }

    inline char nextChar() {
        if (idx >= size) return refill();
        return buf[idx++];
    }

    inline void skipSpaces() {
        char c;
        while ((c = nextChar())) {
            if ((unsigned char)c > ' ') { idx--; return; }
        }
    }

    // Read next token as string (used for "n or filename" detection)
    string readToken() {
        skipSpaces();
        string s;
        char c;
        while ((c = nextChar())) {
            if ((unsigned char)c <= ' ') break;
            s.push_back(c);
        }
        return s;
    }

    inline int readInt() {
        skipSpaces();
        char c = nextChar();
        int sign = 1;
        if (c == '-') { sign = -1; c = nextChar(); }

        int x = 0;
        while (c && (unsigned)(c - '0') <= 9u) {
            x = x * 10 + (c - '0');
            c = nextChar();
        }
        return x * sign;
    }
};

static inline bool isNumberToken(const string& s) {
    if (s.empty()) return false;
    for (char c : s) if ((unsigned)(c - '0') > 9u) return false;
    return true;
}

// ===================== Aligned allocation =====================
static inline int32_t* aligned_alloc_i32(size_t n) {
    void* p = nullptr;
#if defined(_MSC_VER)
    p = _aligned_malloc(n * sizeof(int32_t), 64);
    if (!p) return nullptr;
#else
    if (posix_memalign(&p, 64, n * sizeof(int32_t)) != 0) p = nullptr;
#endif
    return (int32_t*)p;
}

static inline void aligned_free(void* p) {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    free(p);
#endif
}

// ===================== AVX2 helpers =====================
// Horizontal sum of 4x int64 inside __m256i
static inline int64_t hsum256_epi64(__m256i v) {
    alignas(32) int64_t tmp[4];
    _mm256_store_si256((__m256i*)tmp, v);
    return tmp[0] + tmp[1] + tmp[2] + tmp[3];
}

// Compute 4 dot products (a · b0..b3), exact int64 sums.
// a, b* are int32 arrays length n.
// Uses AVX2: mul_epi32 for even lanes and shifted for odd lanes.
static inline void dot4_avx2_i32_i64(
    const int32_t* __restrict a,
    const int32_t* __restrict b0,
    const int32_t* __restrict b1,
    const int32_t* __restrict b2,
    const int32_t* __restrict b3,
    int n,
    int64_t& s0, int64_t& s1, int64_t& s2, int64_t& s3
) {
    __m256i acc0e = _mm256_setzero_si256(), acc0o = _mm256_setzero_si256();
    __m256i acc1e = _mm256_setzero_si256(), acc1o = _mm256_setzero_si256();
    __m256i acc2e = _mm256_setzero_si256(), acc2o = _mm256_setzero_si256();
    __m256i acc3e = _mm256_setzero_si256(), acc3o = _mm256_setzero_si256();

    int k = 0;
    for (; k + 8 <= n; k += 8) {
        __m256i va  = _mm256_loadu_si256((const __m256i*)(a  + k));
        __m256i vb0 = _mm256_loadu_si256((const __m256i*)(b0 + k));
        __m256i vb1 = _mm256_loadu_si256((const __m256i*)(b1 + k));
        __m256i vb2 = _mm256_loadu_si256((const __m256i*)(b2 + k));
        __m256i vb3 = _mm256_loadu_si256((const __m256i*)(b3 + k));

        // prefetch forward (small win, sometimes)
        __builtin_prefetch(a  + k + 64, 0, 1);
        __builtin_prefetch(b0 + k + 64, 0, 1);
        __builtin_prefetch(b1 + k + 64, 0, 1);
        __builtin_prefetch(b2 + k + 64, 0, 1);
        __builtin_prefetch(b3 + k + 64, 0, 1);

        // even lanes
        __m256i p0e = _mm256_mul_epi32(va, vb0);
        __m256i p1e = _mm256_mul_epi32(va, vb1);
        __m256i p2e = _mm256_mul_epi32(va, vb2);
        __m256i p3e = _mm256_mul_epi32(va, vb3);

        // odd lanes
        __m256i vao  = _mm256_srli_epi64(va, 32);
        __m256i vb0o = _mm256_srli_epi64(vb0, 32);
        __m256i vb1o = _mm256_srli_epi64(vb1, 32);
        __m256i vb2o = _mm256_srli_epi64(vb2, 32);
        __m256i vb3o = _mm256_srli_epi64(vb3, 32);

        __m256i p0o = _mm256_mul_epi32(vao, vb0o);
        __m256i p1o = _mm256_mul_epi32(vao, vb1o);
        __m256i p2o = _mm256_mul_epi32(vao, vb2o);
        __m256i p3o = _mm256_mul_epi32(vao, vb3o);

        acc0e = _mm256_add_epi64(acc0e, p0e); acc0o = _mm256_add_epi64(acc0o, p0o);
        acc1e = _mm256_add_epi64(acc1e, p1e); acc1o = _mm256_add_epi64(acc1o, p1o);
        acc2e = _mm256_add_epi64(acc2e, p2e); acc2o = _mm256_add_epi64(acc2o, p2o);
        acc3e = _mm256_add_epi64(acc3e, p3e); acc3o = _mm256_add_epi64(acc3o, p3o);
    }

    s0 = hsum256_epi64(_mm256_add_epi64(acc0e, acc0o));
    s1 = hsum256_epi64(_mm256_add_epi64(acc1e, acc1o));
    s2 = hsum256_epi64(_mm256_add_epi64(acc2e, acc2o));
    s3 = hsum256_epi64(_mm256_add_epi64(acc3e, acc3o));

    // tail
    for (; k < n; k++) {
        int64_t av = (int64_t)a[k];
        s0 += av * (int64_t)b0[k];
        s1 += av * (int64_t)b1[k];
        s2 += av * (int64_t)b2[k];
        s3 += av * (int64_t)b3[k];
    }
}

// ===================== Thread Context =====================
struct ThreadCtx {
    int tid;
    int num_threads;
    int n;
    const int32_t* A;   // row-major
    const int32_t* BT;  // row-major: BT[j*n + k] = B[k*n + j]
    atomic<int>* next_row;
    uint64_t thread_xor;
};

static void pin_thread_if_linux(int tid) {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(tid, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#else
    (void)tid;
#endif
}

static void* worker(void* arg) {
    auto* ctx = static_cast<ThreadCtx*>(arg);
    const int n = ctx->n;
    const int32_t* A  = ctx->A;
    const int32_t* BT = ctx->BT;

    // Optional: pin threads (helps on some Linux judges)
    pin_thread_if_linux(ctx->tid);

    uint64_t local_x = 0;

    while (true) {
        int i = ctx->next_row->fetch_add(1, memory_order_relaxed);
        if (i >= n) break;

        const int32_t* arow = A + (int64_t)i * n;
        uint64_t row_x = 0;

        int j = 0;
        for (; j + 3 < n; j += 4) {
            const int32_t* b0 = BT + (int64_t)(j + 0) * n;
            const int32_t* b1 = BT + (int64_t)(j + 1) * n;
            const int32_t* b2 = BT + (int64_t)(j + 2) * n;
            const int32_t* b3 = BT + (int64_t)(j + 3) * n;

            int64_t s0, s1, s2, s3;
            dot4_avx2_i32_i64(arow, b0, b1, b2, b3, n, s0, s1, s2, s3);

            row_x ^= (uint64_t)s0 ^ (uint64_t)s1 ^ (uint64_t)s2 ^ (uint64_t)s3;
        }

        // tail columns
        for (; j < n; j++) {
            const int32_t* b = BT + (int64_t)j * n;
            int64_t s = 0;

            // vectorize single dot? (less benefit). keep scalar tail.
            for (int k = 0; k < n; k++) s += (int64_t)arow[k] * (int64_t)b[k];
            row_x ^= (uint64_t)s;
        }

        local_x ^= row_x;
    }

    ctx->thread_xor = local_x;
    return nullptr;
}

// ===================== Main =====================
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num_threads>\n", argv[0]);
        return 1;
    }
    int T = atoi(argv[1]);
    if (T <= 0) T = 1;

    // stdin: either "n ..." or "filename"
    FastScanner fs_stdin(stdin);
    string first = fs_stdin.readToken();
    if (first.empty()) return 0;

    FILE* in = stdin;
    FastScanner* fs = &fs_stdin;
    FastScanner* fs_file = nullptr;

    int n = 0;
    if (isNumberToken(first)) {
        n = atoi(first.c_str());
    } else {
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

    const int64_t nn = (int64_t)n * n;

    // aligned A, B, BT
    int32_t* A  = aligned_alloc_i32((size_t)nn);
    int32_t* B  = aligned_alloc_i32((size_t)nn);
    int32_t* BT = aligned_alloc_i32((size_t)nn);
    if (!A || !B || !BT) {
        fprintf(stderr, "aligned_alloc failed\n");
        if (A) aligned_free(A);
        if (B) aligned_free(B);
        if (BT) aligned_free(BT);
        if (fs_file) { fclose(in); delete fs_file; }
        return 1;
    }

    // read A, B
    for (int64_t i = 0; i < nn; i++) A[i] = (int32_t)fs->readInt();
    for (int64_t i = 0; i < nn; i++) B[i] = (int32_t)fs->readInt();

    // transpose B into BT: BT[j*n + i] = B[i*n + j]
    for (int i = 0; i < n; i++) {
        const int32_t* brow = B + (int64_t)i * n;
        for (int j = 0; j < n; j++) {
            BT[(int64_t)j * n + i] = brow[j];
        }
    }

    // threads with dynamic row scheduling
    atomic<int> next_row(0);
    vector<pthread_t> th(T);
    vector<ThreadCtx> ctx(T);

    for (int t = 0; t < T; t++) {
        ctx[t] = ThreadCtx{t, T, n, A, BT, &next_row, 0};
        int rc = pthread_create(&th[t], nullptr, worker, &ctx[t]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed\n");
            // join already created threads
            for (int u = 0; u < t; u++) pthread_join(th[u], nullptr);
            aligned_free(A); aligned_free(B); aligned_free(BT);
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

    aligned_free(A);
    aligned_free(B);
    aligned_free(BT);

    if (fs_file) {
        fclose(in);
        delete fs_file;
    }
    return 0;
}
