#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
using namespace std;

vector<double> gauss_sequential (vector<vector<double>>& a, vector<double>& b) {
    const int n = (int)a.size();

    // forward substitution
    for (int i = 0; i < n; i++) {

        // 1 Choose pivot row r with max |A[r][i]|, r >= i
        int pivot_row = i;
        for (int j = i + 1; j < n; j++) {
            pivot_row = (fabs(a[j][i]) > fabs(a[pivot_row][i])) ? j : pivot_row;
        }

        if (fabs(a[pivot_row][i]) < 1e-15) throw runtime_error("No Solution or Infinite Solutions");

        if (pivot_row != i) {
            swap(a[i], a[pivot_row]);
            swap(b[i], b[pivot_row]);
        }

        // 2 Normalize pivot row
        double pivot = a[i][i];
        for (int j = 0; j < n; j++) a[i][j] /= pivot;
        b[i] /= pivot;

        // 3 Eliminate below row
        for (int k = i + 1; k < n; k++) {
            double m = a[k][i];
            if (fabs(m) < 1e-19) continue;
            
            for (int j = i; j < n; j++) a[k][j] -= a[i][j] * m; 
            b[k] -= b[i] * m; 
        }
    }

    // backward substitution 
    vector<double> x(n);
    for (int i = n - 1; i >= 0; i--) {
        double sum = 0.0;
        for (int j = i + 1; j < n; j++) sum += a[i][j] * x[j];
        x[i] = b[i] - sum; // since already reduced form
    }

    return x;
}

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
    vector<vector<double>> a(n, vector<double>(n));
    vector<double> b(n);
    string s;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            file >> s;
            a[i][j] = parse_token(s);
        }
        file >> s;
        b[i] = parse_token(s);
    }

    // cout << n << "\n";
    // for (int i = 0; i < n; i++) {
    //     for (int j = 0; j < n - 1; j++) {
    //         cout << a[i][j] << " ";
    //     }
    //     cout << b[i] << "\n";
    // }

    vector<double> x = gauss_sequential(a, b);
    
    for (int i = 0; i < n; i++) {
        cout << x[i] << " ";
    }

    return 0;
}