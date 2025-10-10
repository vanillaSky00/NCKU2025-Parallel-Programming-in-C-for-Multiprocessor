#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <mpi.h>
using namespace std;

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);

    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank == 0) {

        std::string file_name;
        std::cin >> file_name;
        std::ifstream file(file_name);
        
        int n; // player length
        int m; // quesiton length

        file >> n >> m;

        vector<int> abilities(n);
        vector<int> difficulties(m);
        for (int i = 0; i < n; i++) file >> abilities[i];
        for (int j = 0; j < m; j++) file >> difficulties[j];

        int myAbility = abilities[0]; // main player
        const int MAXV = 1000;        // max abilities and difficulties bound
        
        // Sort abilities once so we can count >= d via lower_bound
        sort(abilities.begin(), abilities.end());

        // Build c[i] = #players who beat Colten on problem i
        // (= #abilities >= difficulties[i] if myAbility < difficulties[i], else 0)
        vector<int> lose_count(m, 0);
        for (int i = 0; i < m; ++i) {
            int d = difficulties[i];
            if (myAbility >= d) {
                lose_count[i] = 0;
            } else {
                // players with ability >= d, binary search here
                auto it = lower_bound(abilities.begin(), abilities.end(), d);
                lose_count[i] = (int)(abilities.end() - it);
            }
        }

        sort(lose_count.begin(), lose_count.end());

        for (int k = 1; k <= m; ++k) {
            long long ans = 0;
            for (int j = k - 1; j < m; j += k) {
                ans += (long long)lose_count[j] + 1;  // rank = max(c) + 1
            }
            cout << ans << (k == m ? '\n' : ' ');
        }
    }
    
    MPI_Finalize();    
    return 0;
}