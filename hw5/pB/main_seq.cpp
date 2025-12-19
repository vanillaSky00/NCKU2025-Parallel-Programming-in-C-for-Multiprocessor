#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <limits>

using namespace std;
using Edge = pair<int, int>;
using AdjList = vector<vector<Edge>>;
constexpr int INF = numeric_limits<int>::max();

static void* dijkstra_worker(int n, const AdjList& adjList) {
    priority_queue<
        Edge,
        vector<Edge>,
        greater<Edge>
    > pq;

    vector<int> dist(n);
    vector<int> res(n);
    
    for (int src = 0; src < n; src++) {

        fill(dist.begin(), dist.end(), INF);

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
        
        int max_dist = -1;
        for (int i = 0; i < n; i++) {
            if (i == src || dist[i] == INF) continue;
            max_dist = max(max_dist, dist[i]);
        }
        res[src] = max_dist;
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

    AdjList adjList(n);

    int u, v, w;
    while (m-- > 0) {
        file >> u >> v >> w;
        u--; v--;
        adjList[u].push_back({v, w});
        adjList[v].push_back({u, w});
    }

    dijkstra_worker(n, adjList);
}