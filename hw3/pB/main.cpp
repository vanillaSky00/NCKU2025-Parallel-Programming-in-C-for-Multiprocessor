// forward substitution
    for (int i = 0; i < n; i++) {
        int piviot_row = i;

        // 1 choose pivot row r with max |A[r][i]|, r >= i
        for (int j = i + 1; j < n; j++) {
            piviot_row = (fabs(a[j][i]) > fabs(a[piviot_row][i])) ? j : piviot_row;
        }

        if (fabs(a[piviot_row][i]) < 1e-12) throw runtime_error("Singular / nearly singular");

        if (piviot_row != i) {
            swap(a[i], a[piviot_row]);
            swap(b[i], b[piviot_row]);
        }

        // 2 normalize pivot row
        double pivot = a[i][i];
        for (int j = i; j < n; j++) a[i][j] /= pivot;
        b[i] /= pivot;
        
        
        // 3 eliminate below row
        for (int k = i + 1; k < n; k++) {
            double m = a[k][i];
            if (fabs(m) < 1e-19) continue;
            for (int j = i; j < n; j++) a[k][j] -= m * a[i][j];
            b[k] -= m * b[i];
        }
    }

    // backward substitution
    vector<double> x(n);
    for (int i = n - 1; i >= 0; i--) {
        double sum = 0.0;
        for (int j = i + 1; j < n; j++) sum += a[i][j] * x[j];
        x[i] = (b[i] - sum);
    }