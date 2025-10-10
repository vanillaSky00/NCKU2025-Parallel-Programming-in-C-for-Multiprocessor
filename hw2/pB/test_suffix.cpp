#include <bits/stdc++.h>
#include <mpi.h>
using namespace std;

int main(int argc, char *argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank == 0) {
        string file_name;
        cin >> file_name;
        ifstream file(file_name);

        int n, m;
        file >> n >> m;

        vector<int> abilities(n), difficulties(m);
        for (int i = 0; i < n; i++) file >> abilities[i];
        for (int j = 0; j < m; j++) file >> difficulties[j];

        int myAbility = abilities[0];

=
        int maxA = *max_element(abilities.begin(), abilities.end());
        vector<int> freqA(maxA + 1, 0);     
        for (int a : abilities) freqA[a]++;

        // Suffix: sufA[v] = #players with ability >= v
        vector<int> sufA(maxA + 1, 0);
        for (int v = maxA; v >= 0; v--) sufA[v] = sufA[v + 1] + freqA[v];
    

        // c[i] annotated as lose_count[i] = number of players who beat Colten on problem i
        vector<int> lose_count(m, 0);
        for (int i = 0; i < m; ++i) {
            int d = difficulties[i];
            if (myAbility >= d) 
                lose_count[i] = 0;
            else if (d <= maxA) 
                lose_count[i] = sufA[d];
        }

        sort(lose_count.begin(), lose_count.end());

        // use greedy algorithm to select the problem
        // rank at a stage with problem x, y, z is max(c[x], c[y], c[z]) + 1, but we wish to have global minimum
        for (int k = 1; k <= m; ++k) {
            long long ans = 0;
            for (int j = k - 1; j < m; j += k) {
                ans += (long long)lose_count[j] + 1;
            }
            cout << ans << (k == m ? '\n' : ' '); // space format in this judge
        }
    }

    MPI_Finalize();
    return 0;
}
