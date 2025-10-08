#include <iostream>
#include <fstream>
#include <cmath>

const long long MOD = 1000000007;
const long long moduler_inserve2 = 500000004; // Math.pow(2, MOD - 2) % MOD

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);


    long long n;

        n = 1000;

        long long ans = 0;
        long long last = 0;
        int tmp = 0;;
        for (long long i = 1; i * i <= n; i++) {
            long long temp = (n / i) * i % MOD;
            ans = (ans + temp) % MOD;
            last = n / i;
            tmp = i;
        }

        // make sure the boundary
        long long sqrt_n = floor(sqrt((long double) n));
        std::cout << "true last:" << last << " second:" << n / sqrt_n << "true stop i:" << tmp << " tmp:" << sqrt_n << "\n"; 

        for (long long i = last - 1; i >= 1; i--) {
            long long l = n / (i + 1) + 1;
            long long r = n / i;

            long long temp = (( (l + r) % MOD ) * ( (r - l + 1) % MOD )) % MOD;
            temp = (temp * moduler_inserve2) % MOD;
            ans = (ans + (temp * i) % MOD) % MOD;
        }
        
        std::cout << ans << "\n"; 
    
}