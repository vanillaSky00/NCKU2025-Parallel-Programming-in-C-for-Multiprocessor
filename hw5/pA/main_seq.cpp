#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

vector<long long> solver(vector<vector<long long>>& input_A, vector<vector<long long>>& input_B) {
    int n = input_A.size();
    vector<vector<long long>> A(n, vector<long long>(n, 0));
    vector<vector<long long>> B(n, vector<long long>(n, 0));

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (j == 0) {
                A[i][0] = input_A[i][0];
                B[i][0] = input_B[i][0];
            }
            else {
                A[i][j] = (input_A[i][1] * A[i][j-1] + input_A[i][2]) % input_A[i][3];
                B[i][j] = (input_B[i][1] * B[i][j-1] + input_B[i][2]) % input_B[i][3];
            }
        }
    }
    
    vector<long long> res(n, 0);

    for (int i = 0; i < n; i++) {
        long long xor_sum = 0;
        for (int j = 0; j < n; j++) {
            long long sum = 0;
            for (int k = 0; k < n; k++) {
                sum += A[i][k] * B[k][j];
            }
            xor_sum ^= sum;
        }
        res[i] = xor_sum;
    }

    return res;

}

int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);

    int n;
 
    std::string file_name;
    std::cin >> file_name;
    std::ifstream file(file_name);
    
    file >> n;

    vector<vector<long long>> input_A(n, vector<long long>(4, 0));
    vector<vector<long long>> input_B(n, vector<long long>(4, 0));

    for (int i = 0; i < n; i++) {
        file >> input_A[i][0] >> input_A[i][1] >> input_A[i][2] >> input_A[i][3];
    }

    for (int i = 0; i < n; i++) {
        file >> input_B[i][0] >> input_B[i][1] >> input_B[i][2] >> input_B[i][3];
    }

    vector<long long> res = solver(input_A, input_B);
    
    for (long long r : res) {
        cout << r << "\n";
    }
    
    return 0;
}