#include <iostream>
#include <fstream>
#include <vector>
#include <cmath> 
#include <limits>
#include <random>
#include <algorithm>
#include <iomanip> 

using namespace std;

const int NUM_ANTS = 50;
const int NUM_ITERATIONS = 100;
const double ALPHA = 1.0;   
const double BETA = 5.0;    
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
    vector<vector<double>> distMatrix;
    vector<vector<double>> pheromones;

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

        distMatrix.resize(n, vector<double>(n));
        pheromones.resize(n, vector<double>(n, 0.1));

        for (int i = 0; i < NUM_ANTS; i++) {
            ants.emplace_back(n);
        }

        globalBestAnt.tourLength = numeric_limits<double>::max();
        calculateDistances();
    }

    void solve() {
        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
            // 1. Construct Solutions
            for (int antIdx = 0; antIdx < NUM_ANTS; antIdx++) {
                constructSolution(antIdx);
            }
            // 2. Global Update
            daemonActions();
            // 3. Pheromone Update
            pheromoneUpdate();
        }

        cout << fixed << setprecision(6) << globalBestAnt.tourLength << endl;
        for (size_t i = 0; i < globalBestAnt.path.size(); ++i) {
            cout << globalBestAnt.path[i] << (i == globalBestAnt.path.size() - 1 ? "" : " ");
        }
        cout << endl;
    }

private:
    void calculateDistances(){
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                double dx = cities[i].x - cities[j].x;
                double dy = cities[i].y - cities[j].y;
                distMatrix[i][j] = sqrt(dx * dx + dy * dy);
            }
        }
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
            ant.tourLength += distMatrix[currentCity][nextCity];
            ant.visited[nextCity] = true;
            currentCity = nextCity;
        }
        
        // Return to start
        ant.tourLength += distMatrix[currentCity][ant.path[0]];
    }

    int selectNextCity(int currentCity, const vector<bool>& visited) {
        vector<double> probs(n, 0.0);
        double sum = 0.0;

        for (int i = 0; i < n; i++) {
            if (!visited[i]) {
                double eta = 1.0 / (distMatrix[currentCity][i] + 1e-10);
                double tau = pheromones[currentCity][i];
                // FIXED FORMULA: Multiply, don't add
                probs[i] = pow(tau, ALPHA) * pow(eta, BETA);
                sum += probs[i];
            }            
        }

        // FIXED TYPE: Real distribution for doubles
        uniform_real_distribution<double> dist(0.0, sum);
        double r = dist(rng);
        double partialSum = 0.0;

        for (int i = 0; i < n; i++) {
            if (!visited[i]) {
                partialSum += probs[i];
                if (partialSum >= r) return i;
            }
        }

        // fallback
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
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                pheromones[i][j] *= (1.0 - EVAPORATION);
            }
        }

        for (const auto& ant : ants) {
            double contribution = Q / ant.tourLength;
            for (size_t i = 0; i < ant.path.size() - 1; i++) {
                int u = ant.path[i];
                int v = ant.path[i+1];
                pheromones[u][v] += contribution;
                pheromones[v][u] += contribution;
            }
            int u = ant.path.back();
            int v = ant.path[0];
            pheromones[u][v] += contribution;
            pheromones[v][u] += contribution;
        }        
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string filename;
    if (!(cin >> filename)) return 0;

    ifstream file(filename);
    if (!file) return 1;

    int n;
    file >> n;
    vector<pair<int, int>> cities(n);

    int i = 0;
    int count = n;
    while (count-- > 0) {
        file >> cities[i].first;
        file >> cities[i].second;
        i++;
    }

    AntColonyOptimization aco(cities);
    aco.solve();

    return 0;
}