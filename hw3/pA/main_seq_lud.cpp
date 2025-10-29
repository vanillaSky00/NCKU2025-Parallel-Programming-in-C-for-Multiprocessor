#include <iostream>
#include <fstream>
#include <vector>
using namespace std;

static const double EPS = 1e-12;   // 計算時判零門檻（double 建議 1e-12 ~ 1e-10）
static const double RHS_EPS = 1e-9; // 判不相容時對 RHS 的門檻略寬

static double parse_token(string tok) {
    auto p = tok.find('/');
    if (p == std::string::npos) return stold(tok);
    double num = stold(tok.substr(0, p));
    double den = stold(tok.substr(p + 1));
    if (den == 0.0L) throw runtime_error("division by zero");
    return num / den;
}

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    int n;
    
    string file_name;
    cin >> file_name;
    ifstream file(file_name);

    file >> n;
    vector<vector<double>> A(n, vector<double>(n));
    vector<double> b(n);
    string s;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            file >> s;
            A[i][j] = parse_token(s);
        }
        file >> s;
        b[i] = parse_token(s);
    }

    // 前向消去（部分選主元），形成行梯形；不強制每行歸一化
    int row = 0, col = 0;
    vector<int> piv_col; piv_col.reserve(n);

    while (row < n && col < n) {
        // 找當前欄 col 中（row..n-1）絕對值最大的主元列
        int sel = row;
        for (int i = row + 1; i < n; ++i)
            if (fabs(A[i][col]) > fabs(A[sel][col])) sel = i;

        if (fabs(A[sel][col]) < EPS) { // 這一欄沒有有效主元 → 自由變數，換下一欄
            ++col;
            continue;
        }

        if (sel != row) { swap(A[sel], A[row]); swap(b[sel], b[row]); }

        // 用主元消去下方各列
        double piv = A[row][col];
        for (int i = row + 1; i < n; ++i) {
            double m = (fabs(A[i][col]) < EPS) ? 0.0 : A[i][col] / piv;
            if (fabs(m) < EPS) continue;
            for (int j = col; j < n; ++j) A[i][j] -= m * A[row][j];
            b[i] -= m * b[row];
        }

        piv_col.push_back(col);
        ++row; ++col;
    }

    int rankA = (int)piv_col.size();

    // 檢查不相容：若某行係數全 ~0，但 RHS 非 ~0 → 無解
    for (int i = rankA; i < n; ++i) {
        bool allZero = true;
        for (int j = 0; j < n; ++j)
            if (fabs(A[i][j]) > EPS) { allZero = false; break; }
        if (allZero && fabs(b[i]) > RHS_EPS) {
            cout << "No Solution\n";
            return 0;
        }
    }

    // 若秩不足且相容 → 無限多解
    if (rankA < n) {
        cout << "Infinite Solutions\n";
        return 0;
    }

    // 唯一解：回代
    vector<double> x(n, 0.0);
    for (int i = rankA - 1; i >= 0; --i) {
        int c = piv_col[i];
        double sum = 0.0;
        for (int j = c + 1; j < n; ++j) sum += A[i][j] * x[j];
        double piv = A[i][c];
        if (fabs(piv) < EPS) { // 理論上不會發生，防守一下
            cout << "No Solution\n"; return 0;
        }
        x[c] = (b[i] - sum) / piv;
    }

    // 輸出唯一解（一行，空白分隔）
    cout.setf(ios::fixed);
    cout << setprecision(8);
    for (int i = 0; i < n; ++i) {
        if (i) cout << ' ';
        cout << x[i];
    }
    cout << '\n';
    return 0;
}