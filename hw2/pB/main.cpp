#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
using namespace std;

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);

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
    
    vector<int> abilityFreq(MAXV + 1, 0);
    for (int a : abilities) abilityFreq[a]++;

    vector<int> abilityPre(MAXV + 1, 0);
    for (int i = 1; i <= MAXV; i++) abilityPre[i] = abilityPre[i-1] + abilityFreq[i]; 

    // count at problem i how many people lose_count[i] annotated as c[i] the main player has lost to
    vector<int> lose_count(m, 0);
    for (int i = 0; i < m; i++) {
        int d = difficulties[i];
        if (myAbility >= d) lose_count[i] = 0;
        else lose_count[i] = abilityPre[MAXV] - abilityPre[d-1];
    }

    // use greedy algorithm to select the problem
    // rank at a stage with problem x, y, z is max(c[x], c[y], c[z]) + 1, but we wish to have global minimum
    sort(lose_count.begin(), lose_count.end());

    for (int k = 1; k <= m; k++) { // mlog(m)
        int ans = 0;
        for (int j = k - 1; j < m; j+=k) {
            //cout << "when k = " << k << " rank: " << lose_count[j] + 1 << "\n";
            ans += lose_count[j] + 1;
        }
        cout << ans << " ";
        //cout << "stage: " << ans << "\n";
    }
    cout << "\n";
    return 0;
}