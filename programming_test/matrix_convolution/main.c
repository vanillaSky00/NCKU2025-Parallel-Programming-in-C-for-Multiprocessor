#include <stdio.h>
#include <stdlib.h>

const int MOD = 998244353; // const make the modulo faster

int main() {
    char buffer[100];
    scanf("%99s", buffer);
    FILE *file = fopen(buffer, "r"); // read file path
    int n, t;
    fscanf(file, "%d %d", &n, &t);
    int kernel[3][3];
    for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
            fscanf(file, "%d", &kernel[i][j]);
        }
    }
    int **arr = (int **)malloc((n + 2) * sizeof(int *)), **tmp = (int **)malloc((n + 2) * sizeof(int *));
    for (int i = 0; i < n + 2; i++) {
        arr[i] = (int *)calloc(n + 2, sizeof(int)); // initialize with zeros
        tmp[i] = (int *)calloc(n + 2, sizeof(int)); // initialize with zeros
    }
    for(int i = 1; i <= n; i++) {
        for(int j = 1; j <= n; j++) {
            fscanf(file, "%d", &arr[i][j]);
        }
    }
    
    for(int i = 0; i < t; ++i) {
        for(int j = 1; j <= n; ++j) {
            for(int k = 1; k <= n; ++k) {
                int val = 0;
                for(int x = -1; x <= 1; ++x) {
                    for(int y = -1; y <= 1; ++y) {
                        val = (val + (arr[j + x][k + y] % MOD * 1LL * kernel[x + 1][y + 1] % MOD + MOD) % MOD) % MOD;
                        // calculate convolution with modulo MOD
                    }
                }
                tmp[j][k] = val;
            }
        }
        // swap arr and tmp pointers
        int **swap = arr;
        arr = tmp;
        tmp = swap;
    }
    for(int i = 1; i <= n; i++) {
        for(int j = 1; j <= n; j++) {
            printf("%d", arr[i][j]);
            if (j < n) printf(" "); // do not print space after last element
        }
        printf("\n");
    }
    return 0;
}