#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <climits>

using namespace std;


static void* worker(int n, vector<vector<pair<int, int>>>& adjList) {
    priority_queue<
        pair<int, int>,
        vector<pair<int, int>>,
        greater<pair<int, int>>
    > pq;

    vector<int> res(n, 0);

    for (int src = 0; src < n; src++) {

        vector<int> dist(n, INT_MAX);
        dist[src] = 0;
        pq.push({0, src});

        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            
            if (d > dist[u]) continue;

            for (const auto& [v, w] : adjList[u]) {
                if (d + w < dist[v]) {
                    dist[v] = d + w;
                    pq.push({dist[v], v});
                }
            }
        }
        
        int max_dist = INT_MIN;
        for (int i = 0; i < n; i++) {
            if (i == src) continue;
            int d = dist[i];
            if (d != INT_MAX && d > max_dist) max_dist = d;
        }
        res[src] = max_dist == INT_MIN ? -1 : max_dist;
    }

    for (int r : res) {
        cout << r << "\n";
    }

    return nullptr;
}


int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <num_threads>\n";
        return 1;
    }

    int NUM_THREADS = stoi(argv[1]);
    if (NUM_THREADS <= 0)
        throw runtime_error("num_threads must be positive");

    string file_name;
    cin >> file_name;
    ifstream file(file_name);
    if (!file)
        throw runtime_error("cannot open input file");

    int n, m;
    file >> n >> m;

    vector<vector<pair<int, int>>> adjList(n);

    int u, v, w;
    while (m-- > 0) {
        file >> u >> v >> w;
        u--; v--;
        adjList[u].push_back({v, w});
        adjList[v].push_back({u, w});
    }

    worker(n, adjList);
}