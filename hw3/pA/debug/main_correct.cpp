#include <bits/stdc++.h>
#include <mpi.h>
using namespace std;

static constexpr double EPS = 1e-9;

// 解析字串數字，支援 "p/q" 分數
static double parseNumber(const string& s_) {
    string s = s_;
    // 去除前後空白
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
            // 去尾端空白
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

// 計算各 rank 的 row 區間起點（循環分配：前 extra 個 rank 多一列）
static inline int start_of_rank(int rank, int base, int extra) {
    return rank * base + (rank < extra ? rank : extra);
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    MPI_Init(&argc, &argv);
    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n = 0;
    vector<double> full; // root 端用的增廣矩陣 (n x (n+1)) 線性化

    if (rank == 0) {
        // 從 stdin 讀「資料檔路徑」
        string path;
        if (!(cin >> path)) MPI_Abort(MPI_COMM_WORLD, 1);

        ifstream fin(path);
        if (!fin || !(fin >> n)) MPI_Abort(MPI_COMM_WORLD, 1);

        full.resize((size_t)n * (n + 1));
        string tok;
        for (int i = 0; i < n * (n + 1); ++i) {
            if (!(fin >> tok)) MPI_Abort(MPI_COMM_WORLD, 1);
            full[i] = parseNumber(tok);
        }
    }

    // 廣播 n
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (n == 0) {
        MPI_Finalize();
        return 0;
    }

    // 工作切分
    int base = n / size;
    int extra = n % size;
    int my_rows = base + (rank < extra);
    int start = start_of_rank(rank, base, extra);

    // 建立 Scatterv 參數（root 端）
    vector<int> cnt, disp;
    if (rank == 0) {
        cnt.resize(size);
        disp.resize(size);
        int d = 0;
        for (int p = 0; p < size; ++p) {
            cnt[p] = (base + (p < extra)) * (n + 1);
            disp[p] = d;
            d += cnt[p];
        }
    }

    // 本地增廣矩陣 A_local：my_rows x (n+1)
    vector<double> A((size_t)my_rows * (n + 1), 0.0);

    // 分發行塊（按連續 row 區段，而非純 i%p）
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

    // pivot 緩衝
    vector<double> pivot(n + 1, 0.0);

    // 前向消去
    for (int k = 0; k < n; ++k) {
        // 找全域樞紐（最大 |A[i][k]|，i >= k）
        double maxv = 0.0;
        int piv = -1;
        for (int i = 0; i < my_rows; ++i) {
            int gi = start + i;
            if (gi >= k) {
                double v = fabs(A[(size_t)i * (n + 1) + k]);
                if (v > maxv) {
                    maxv = v;
                    piv = gi; // 記錄 global row
                }
            }
        }

        struct { double val; int idx; } in{maxv, piv}, out{};
        // 注意：MPI_DOUBLE_INT 假設 {double,int} 連續布局，多數實作可用
        MPI_Allreduce(&in, &out, 1, MPI_DOUBLE_INT, MPI_MAXLOC, MPI_COMM_WORLD);
        piv = out.idx;
        if (piv < 0 || out.val < EPS) {
            // 此欄無可用樞紐 → 略過（對秩判定會反映）
            continue;
        }

        // 計算 pivot row owner 與 k row owner
        int owner_p = 0, owner_k = 0, acc = 0;
        for (int p = 0; p < size; ++p) {
            int rows = base + (p < extra);
            if (piv >= acc && piv < acc + rows) owner_p = p;
            if (k   >= acc && k   < acc + rows) owner_k = p;
            acc += rows;
        }

        // 若兩列不同 owner，兩端各自取出自身那一列，用 Sendrecv_replace 互換
        if (owner_p != owner_k && (rank == owner_p || rank == owner_k)) {
            int rowLocal = (rank == owner_p) ? (piv - start) : (k - start);
            memcpy(pivot.data(), &A[(size_t)rowLocal * (n + 1)], (n + 1) * sizeof(double));

            int peer = (rank == owner_p) ? owner_k : owner_p;
            // tag 用 k 以免未來擴充時混淆
            MPI_Sendrecv_replace(pivot.data(), n + 1, MPI_DOUBLE, peer, 1000 + k,
                                 peer, 1000 + k, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            memcpy(&A[(size_t)rowLocal * (n + 1)], pivot.data(), (n + 1) * sizeof(double));
        }
        // 同 owner：在 owner_k 上就地交換
        else if (owner_p == owner_k && rank == owner_k && piv != k) {
            int ip = piv - start, ik = k - start;
            for (int j = 0; j <= n; ++j)
                swap(A[(size_t)ip * (n + 1) + j], A[(size_t)ik * (n + 1) + j]);
        }

        // owner_k 負責把第 k 列歸一化，並廣播 k..n
        if (rank == owner_k) {
            int ik = k - start;
            double diag = A[(size_t)ik * (n + 1) + k];
            // 若 diag 很小，仍嘗試；理想上這不會發生，因已選主元
            for (int j = k; j <= n; ++j)
                A[(size_t)ik * (n + 1) + j] /= diag;
            // 複製出 pivot 行
            memcpy(pivot.data(), &A[(size_t)ik * (n + 1)], (n + 1) * sizeof(double));
        }
        MPI_Bcast(pivot.data() + k, (n + 1) - k, MPI_DOUBLE, owner_k, MPI_COMM_WORLD);

        // 用 pivot 列消去「本地 >k 的各列」
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

    // 收回到 root 檢查秩與回代
    if (rank == 0) {
        cnt.assign(size, 0);
        disp.assign(size, 0);
        int d = 0;
        for (int p = 0; p < size; ++p) {
            cnt[p] = (base + (p < extra)) * (n + 1);
            disp[p] = d;
            d += cnt[p];
        }
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
        // 用 row-echelon 後的增廣矩陣判斷：
        // rC = rank([A|b]), rA = rank(A)（注意你原始程式的變數命名相反）
        int rAb = 0, rA = 0;
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
            // 唯一解：回代
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

    MPI_Finalize();
    return 0;
}