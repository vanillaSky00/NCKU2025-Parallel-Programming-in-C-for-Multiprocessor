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

        int maxA = *max_element(abilities.begin(), abilities.end());
        int lowBound = myAbility + 1;
        int span = max(0, maxA - lowBound + 1);

        vector<int> freqA(span, 0); // freq over [lowBound..maxA] mapped to [0..span-1]
        for (int a : abilities) if (a >= lowBound) freqA[a - lowBound]++; // compressed indices

        // suffix sum of players with ability >= v
        vector<int> sufA(span + 1, 0); 
        for (int i = span - 1; i >= 0; i--) sufA[i] = sufA[i + 1] + freqA[i];

        // count at problem i how many people lose_count[i] annotated as c[i] the main player has lost to
        vector<int> lose_count(m, 0);
        int maxLose = 0;
        for (int i = 0; i < m; ++i) {
            int d = difficulties[i];
            if (myAbility < d && d <= maxA) {
                int idx = d - lowBound;
                int v = sufA[idx];
                lose_count[i] = v;
                maxLose = max(maxLose, v);
            }  // else leave 0
        }

        // use greedy algorithm to select 
        //sort(lose_count.begin(), lose_count.end());
        // counting sort, build freq and in-place write back, O(m + maxLose) time and O(maxLose) extra space.
        vector<int> freqL(maxLose + 1, 0);
        for (int l : lose_count) freqL[l]++;

        int idx = 0;
        for (int l = 0; l <= maxLose; l++) {
            int cnt = freqL[l];
            while (cnt--) lose_count[idx++] = l;
        }

        // rank at a stage with problem x, y, z is max(c[x], c[y], c[z]) + 1, but we wish to have global minimum
        for (int k = 1; k <= m; k++) {
            long long ans = 0;                 
            for (int j = k - 1; j < m; j += k) {
                ans += (long long)lose_count[j] + 1;  
            }
            cout << ans << (k == m ? '\n' : ' '); // restrict space format for this judge
        }
    }
    MPI_Finalize();
    return 0;
}