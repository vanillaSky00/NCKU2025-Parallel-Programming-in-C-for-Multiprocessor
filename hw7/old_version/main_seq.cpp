#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>
#include <iomanip> 

using namespace std;


const int NUM_ANTS = 20; 
const int NUM_ITERATIONS = 50; 
const double ALPHA = 1.0;
const double BETA = 2.0; 
const double EVAPORATION = 0.5;
const double Q = 100.0;

struct Point {
    int x, y;
};

struct Ant {
    vector<int> path;
    double tourLength;
    vector<bool> visited;

    Ant(int n) : tourLength(0.0), visited(n, false) {
        path.reserve(n);
    }

    void reset(int n) {
        path.clear();
        visited.assign(n, false);
        tourLength = 0.0;
    }
};

class AntColonyOptimization {
private:
    int n; 
    vector<Point> cities;
    
    // MEMORY FIX: Removed distMatrix (Too big for N=10^6). 
    // We calculate distance on the fly.
    
    // MEMORY FIX: Use 1D vector for pheromones and float to save RAM.
    // Mapping: pheromones[i * n + j]
    vector<float> pheromones; 

    vector<Ant> ants;
    Ant globalBestAnt;

    mt19937 rng; 

public:
    AntColonyOptimization(const vector<pair<int, int>>& inputCities)
        : n(inputCities.size()), 
          globalBestAnt(inputCities.size()), 
          rng(random_device{}()) {
        
        for (auto& pos : inputCities) {
            cities.push_back(Point{pos.first, pos.second});
        }

        // Initialize pheromones (1D array)
        // Note: For N=10^6, even this is too big. 
        try {
            pheromones.resize((size_t)n * n, 0.1f);
        } 
        catch (const std::bad_alloc& e) {
            cerr << "[Error] Memory allocation failed for Pheromones. N is too large." << endl;
            exit(1);
        }

        for (int i = 0; i < NUM_ANTS; i++) {
            ants.emplace_back(n);
        }

        globalBestAnt.tourLength = numeric_limits<double>::max();
    }

    void solve() {
        if (n <= 1) {
            cout << "0.000000" << endl;
            if (n == 1) cout << "0" << endl;
            return;
        }

        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
            for (int antIdx = 0; antIdx < NUM_ANTS; antIdx++) {
                constructSolution(antIdx);
            }
            daemonActions();
            pheromoneUpdate();
        }

        // --- OUTPUT FORMAT ---
        cout << fixed << setprecision(6) << globalBestAnt.tourLength << endl;
        for (size_t i = 0; i < globalBestAnt.path.size(); ++i) {
            cout << globalBestAnt.path[i] << (i == globalBestAnt.path.size() - 1 ? "" : " ");
        }
        cout << endl;
    }

private:
    // Helper to get distance on the fly (Saves N*N memory)
    inline double getDist(int i, int j) {
        double dx = cities[i].x - cities[j].x;
        double dy = cities[i].y - cities[j].y;
        return sqrt(dx*dx + dy*dy);
    }

    inline float& getPheromone(int i, int j) {
        return pheromones[i * n + j];
    }

    void constructSolution(int antId) {
        Ant& ant = ants[antId];
        ant.reset(n);

        uniform_int_distribution<int> dist(0, n-1);
        int currentCity = dist(rng);

        ant.path.push_back(currentCity);
        ant.visited[currentCity] = true;

        for (int step = 0; step < n - 1; step++) {
            int nextCity = selectNextCity(currentCity, ant.visited);
            ant.path.push_back(nextCity);
            ant.tourLength += getDist(currentCity, nextCity);
            ant.visited[nextCity] = true;
            currentCity = nextCity;
        }
        
        ant.tourLength += getDist(currentCity, ant.path[0]);
    }

    int selectNextCity(int currentCity, const vector<bool>& visited) {
        // Optimization: Don't allocate probs vector every time. 
        // Use a thread-local or member vector ideally. 
        // For now, keeping it simple but checking n size.
        
        // If N is huge, we cannot iterate all cities.
        // For this assignment, we use standard ACO logic but be aware it's O(N).
        
        vector<double> probs; 
        probs.reserve(n); // Reserve to avoid reallocs
        
        double sum = 0.0;
        
        // Only calculate probs for unvisited to save some time
        // But we need to maintain index 'i'.
        
        // Standard Roulette Wheel
        for (int i = 0; i < n; i++) {
            if (!visited[i]) {
                double tau = (double)getPheromone(currentCity, i);
                double dist = getDist(currentCity, i);
                double eta = 1.0 / (dist + 1e-10);
                
                double p = pow(tau, ALPHA) * pow(eta, BETA);
                probs.push_back(p);
                sum += p;
            } else {
                probs.push_back(0.0);
            }
        }

        uniform_real_distribution<double> dist(0.0, sum);
        double r = dist(rng);
        double partialSum = 0.0;

        for (int i = 0; i < n; i++) {
            if (!visited[i]) {
                partialSum += probs[i];
                if (partialSum >= r) return i;
            }
        }

        // Fallback
        for (int i = 0; i < n; i++) if (!visited[i]) return i;
        return -1;
    }

    void daemonActions() {
        for (const auto& ant : ants) {
            if (ant.tourLength < globalBestAnt.tourLength) {
                globalBestAnt = ant;
            }
        }
    }

    void pheromoneUpdate() {
        // 1. Evaporation
        // Flattened loop is faster
        for (size_t i = 0; i < pheromones.size(); ++i) {
            pheromones[i] *= (1.0f - (float)EVAPORATION);
        }

        // 2. Deposit
        for (const auto& ant : ants) {
            double contribution = Q / ant.tourLength;
            for (size_t i = 0; i < ant.path.size() - 1; i++) {
                int u = ant.path[i];
                int v = ant.path[i+1];
                getPheromone(u, v) += (float)contribution;
                getPheromone(v, u) += (float)contribution;
            }
            // Close loop
            int u = ant.path.back();
            int v = ant.path[0];
            getPheromone(u, v) += (float)contribution;
            getPheromone(v, u) += (float)contribution;
        }        
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string filename;
    if (!(cin >> filename)) return 0;

    ifstream file(filename);
    if (!file) {
        cerr << "[Error] opening file." << endl;
        return 1;
    }

    int n;
    if (!(file >> n)) return 0;
    
    vector<pair<int, int>> cities(n);

    int i = 0;
    // Fix loop to ensure we don't overflow if file is short
    while (i < n && file >> cities[i].first >> cities[i].second) {
        i++;
    }

    // Safety check if file had fewer lines than N
    if (i < n) {
        cities.resize(i);
        n = i;
    }

    AntColonyOptimization aco(cities);
    aco.solve();
    return 0;
}