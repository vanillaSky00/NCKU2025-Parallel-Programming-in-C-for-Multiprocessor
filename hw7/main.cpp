#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>
#include <iomanip> 
#include <numeric>
#include <new> 

using namespace std;

// --- CONFIGURATION ---
// "MMAS" Standard constants
const int NUM_ANTS = 30;           
const int NUM_ITERATIONS = 200;    // More iterations possible due to Candidate List speedup
const double ALPHA = 1.0;
const double BETA = 2.0;           // Lower BETA because Candidate List does the greedy filtering
const double EVAPORATION = 0.2;    // Slower evaporation (MMAS style)
const double Q = 1.0;              // Q is less relevant in MMAS, but keep 1.0
const int NUM_THREADS = 4;

// --- CRITICAL THRESHOLDS ---
const int ACO_LIMIT = 5000;        // Use ACO for N <= 5000
const int CANDIDATE_K = 25;        // Check only 25 nearest neighbors
const int MAX_COORD = 1000;

// MMAS Bounds
double TAU_MAX = 1.0;
double TAU_MIN = 0.0001;

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

class AntColonyOptimization;

struct ThreadContext {
    int thread_id;
    int start_ant_idx;
    int end_ant_idx;
    AntColonyOptimization* aco_instance;
};

class AntColonyOptimization {
public: 
    int n; 
private:
    vector<Point> cities;
    
    // Flattened matrices
    vector<float> distMatrix;   // Size N*N
    vector<float> pheromones;   // Size N*N
    vector<int> candidates;     // Size N*K (Stores indices of K nearest neighbors)

    vector<Ant> ants;
    Ant globalBestAnt;

public:
    AntColonyOptimization(const vector<pair<int, int>>& inputCities)
        : n(inputCities.size()), 
          globalBestAnt(inputCities.size()) {
        
        for (auto& pos : inputCities) {
            cities.push_back(Point{pos.first, pos.second});
        }

        // --- 1. Memory Allocation (Only for ACO size) ---
        if (n <= ACO_LIMIT) {
            try {
                // Precompute Distance Matrix (O(1) lookup later)
                distMatrix.resize((size_t)n * n);
                for(int i=0; i<n; i++){
                    for(int j=0; j<n; j++){
                        long long dx = cities[i].x - cities[j].x;
                        long long dy = cities[i].y - cities[j].y;
                        distMatrix[i*n + j] = sqrt(dx*dx + dy*dy);
                    }
                }

                // Initialize Pheromones
                // Initial Tau = 1 / (rho * NearestNeighborTour) estimate
                // For simplicity, we start with a high value = TAU_MAX
                TAU_MAX = 1.0 / (EVAPORATION * n * 10.0); // Rough estimate
                TAU_MIN = TAU_MAX / (2.0 * n);
                pheromones.resize((size_t)n * n, (float)TAU_MAX);

                // Build Candidate Lists (The Speedup Trick)
                buildCandidateLists();

                for (int i = 0; i < NUM_ANTS; i++) {
                    ants.emplace_back(n);
                }
            } 
            catch (const std::bad_alloc& e) {
                cerr << "[Error] Memory allocation failed. N=" << n << endl;
                exit(1);
            }
        }
        
        globalBestAnt.tourLength = numeric_limits<double>::max();
    }

    void solve() {
        if (n <= 1) {
            cout << "0.000000" << endl;
            if (n == 1) cout << "0" << endl;
            return;
        }

        if (n > ACO_LIMIT) {
            solveGreedyLargeN();
        } else {
            solveACO();
        }

        // --- OUTPUT ---
        // 5. ROTATE PATH TO START AT 0 (Requirement)
        if (!globalBestAnt.path.empty()) {
            auto it = find(globalBestAnt.path.begin(), globalBestAnt.path.end(), 0);
            if (it != globalBestAnt.path.end()) {
                rotate(globalBestAnt.path.begin(), it, globalBestAnt.path.end());
            }
        }

        cout << fixed << setprecision(6) << globalBestAnt.tourLength << endl;
        if (!globalBestAnt.path.empty()) {
            cout << globalBestAnt.path[0];
            for (size_t i = 1; i < globalBestAnt.path.size(); ++i) {
                cout << " " << globalBestAnt.path[i];
            }
        }
        cout << endl;
    }

private:
    // --- OPTIMIZATION B: Candidate Lists ---
    void buildCandidateLists() {
        candidates.resize((size_t)n * CANDIDATE_K);
        vector<int> indices(n);
        iota(indices.begin(), indices.end(), 0);

        for (int i = 0; i < n; i++) {
            // Create pairs of (distance, index)
            vector<pair<float, int>> neighbors;
            neighbors.reserve(n);
            for (int j = 0; j < n; j++) {
                if (i != j) {
                    neighbors.push_back({distMatrix[i*n + j], j});
                }
            }
            
            // Partially sort to get top K nearest
            // nth_element is O(N), much faster than sort O(N log N)
            if (n > CANDIDATE_K) {
                nth_element(neighbors.begin(), neighbors.begin() + CANDIDATE_K, neighbors.end());
                // Sort the top K for even better ACO performance
                sort(neighbors.begin(), neighbors.begin() + CANDIDATE_K);
            } else {
                sort(neighbors.begin(), neighbors.end());
            }

            // Store in flat array
            int limit = min(n - 1, CANDIDATE_K);
            for (int k = 0; k < limit; k++) {
                candidates[i * CANDIDATE_K + k] = neighbors[k].second;
            }
        }
    }

    // --- STRATEGY 1: Optimized ACO ---
    void solveACO() {
        pthread_t threads[NUM_THREADS];
        ThreadContext contexts[NUM_THREADS];
        int ants_per_thread = NUM_ANTS / NUM_THREADS;

        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
            
            // 1. Move Ants
            for (int t = 0; t < NUM_THREADS; t++) {
                contexts[t].thread_id = t;
                contexts[t].start_ant_idx = t * ants_per_thread;
                contexts[t].end_ant_idx = (t == NUM_THREADS - 1) ? NUM_ANTS : (t + 1) * ants_per_thread;
                contexts[t].aco_instance = this;
                pthread_create(&threads[t], nullptr, AntColonyOptimization::threadEntry, &contexts[t]);
            }

            for (int t = 0; t < NUM_THREADS; t++) {
                pthread_join(threads[t], nullptr);
            }   

            // 2. Find Iteration Best
            Ant* iterBest = &ants[0];
            for (int i = 1; i < NUM_ANTS; i++) {
                if (ants[i].tourLength < iterBest->tourLength) {
                    iterBest = &ants[i];
                }
            }

            // --- OPTIMIZATION C: 2-Opt Local Search ---
            // Run local search ONLY on the best ant of the iteration (Fast & Effective)
            localSearch2Opt(*iterBest);

            // Update Global Best
            if (iterBest->tourLength < globalBestAnt.tourLength) {
                globalBestAnt = *iterBest;
                // Recalculate MMAS bounds based on new best
                TAU_MAX = 1.0 / (EVAPORATION * globalBestAnt.tourLength);
                TAU_MIN = TAU_MAX / (2.0 * n);
            }

            // --- OPTIMIZATION D: MMAS Pheromone Update ---
            pheromoneUpdateMMAS(iterBest);
        }
    }

    // A fast 2-opt implementation
    void localSearch2Opt(Ant& ant) {
        bool improvement = true;
        while (improvement) {
            improvement = false;
            for (int i = 0; i < n - 1; i++) {
                for (int j = i + 1; j < n; j++) {
                    int u = ant.path[i];
                    int v = ant.path[(i + 1) % n];
                    int x = ant.path[j];
                    int y = ant.path[(j + 1) % n];

                    // Current distance vs New distance (swap edges)
                    // (u,v) + (x,y)  vs  (u,x) + (v,y)
                    float currentDist = distMatrix[u*n + v] + distMatrix[x*n + y];
                    float newDist = distMatrix[u*n + x] + distMatrix[v*n + y];

                    if (newDist < currentDist) {
                        // Perform swap: reverse segment between i+1 and j
                        reverse(ant.path.begin() + i + 1, ant.path.begin() + j + 1);
                        ant.tourLength -= (currentDist - newDist);
                        improvement = true;
                    }
                }
            }
        }
    }

    void pheromoneUpdateMMAS(Ant* iterBest) {
        // 1. Evaporate everything
        for (auto& p : pheromones) {
            p *= (1.0f - (float)EVAPORATION);
            if (p < TAU_MIN) p = (float)TAU_MIN; // Clamp Min
        }

        // 2. Deposit ONLY on Iteration Best (MMAS rule)
        // Or Global Best (alternating helps sometimes, but Iteration Best is standard)
        double contribution = 1.0 / iterBest->tourLength;
        
        for (int i = 0; i < n; i++) {
            int u = iterBest->path[i];
            int v = iterBest->path[(i+1)%n];
            
            pheromones[u*n + v] += (float)contribution;
            pheromones[v*n + u] += (float)contribution;
        }

        // 3. Clamp Max
        for (auto& p : pheromones) {
            if (p > TAU_MAX) p = (float)TAU_MAX;
        }
    }

    // --- Helpers ---
    inline float getDist(int i, int j) const {
        return distMatrix[i * n + j];
    }
    
    // Large N fallback (standard euclidean)
    inline double getDistLarge(int i, int j) {
        long long dx = cities[i].x - cities[j].x;
        long long dy = cities[i].y - cities[j].y;
        return sqrt(dx*dx + dy*dy);
    }

    inline float& getPheromone(int i, int j) {
        return pheromones[i * n + j];
    }

public:
    void constructSolution(int antId, mt19937& local_rng, vector<double>& probsBuffer) {
        Ant& ant = ants[antId];
        ant.reset(n);

        // Random Start (Better exploration)
        uniform_int_distribution<int> dist(0, n-1);
        int currentCity = dist(local_rng);

        ant.path.push_back(currentCity);
        ant.visited[currentCity] = true;

        for (int step = 0; step < n - 1; step++) {
            int nextCity = selectNextCity(currentCity, ant.visited, local_rng, probsBuffer);
            ant.path.push_back(nextCity);
            ant.tourLength += getDist(currentCity, nextCity);
            ant.visited[nextCity] = true;
            currentCity = nextCity;
        }
        ant.tourLength += getDist(currentCity, ant.path[0]);
    }

private:
    int selectNextCity(int currentCity, const vector<bool>& visited, mt19937& local_rng, vector<double>& probs) {
        probs.clear();
        double sum = 0.0;
        
        // --- OPTIMIZATION B: Use Candidate List First ---
        // Only check the K nearest neighbors
        bool foundCandidate = false;
        int limit = min(n - 1, CANDIDATE_K);
        
        for (int k = 0; k < limit; k++) {
            int neighbor = candidates[currentCity * CANDIDATE_K + k];
            if (!visited[neighbor]) {
                double tau = (double)getPheromone(currentCity, neighbor);
                double dist = getDist(currentCity, neighbor);
                double eta = 1.0 / (dist + 1e-10);
                
                double p = pow(tau, ALPHA) * pow(eta, BETA);
                probs.push_back(p);
                sum += p;
                foundCandidate = true;
            } else {
                probs.push_back(0.0);
            }
        }

        // --- FALLBACK: If all candidates visited, scan everyone ---
        if (sum == 0.0) {
            probs.clear(); // Reset to size 0
            // We have to rebuild the probability vector for the remaining unvisited nodes
            // Note: Since 'probs' index must match 'candidates' index in the first loop,
            // but now we need to match 'global' index, we handle logic carefully.
            
            // Simplest correct way for fallback: Pure Greedy fallback or standard roulette
            // Let's do Standard Roulette over ALL unvisited
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
            
            // Selection logic for Fallback
            uniform_real_distribution<double> dist(0.0, sum);
            double r = dist(local_rng);
            double partialSum = 0.0;
            for (int i = 0; i < n; i++) {
                if (!visited[i]) {
                    partialSum += probs[i];
                    if (partialSum >= r) return i;
                }
            }
            // Absolute safety
            for (int i = 0; i < n; i++) if (!visited[i]) return i;
        } 
        else {
            // Selection logic for Candidate List
            uniform_real_distribution<double> dist(0.0, sum);
            double r = dist(local_rng);
            double partialSum = 0.0;
            for (int k = 0; k < limit; k++) {
                int neighbor = candidates[currentCity * CANDIDATE_K + k];
                if (!visited[neighbor]) {
                    partialSum += probs[k]; // probs[k] corresponds to neighbor
                    if (partialSum >= r) return neighbor;
                }
            }
        }
        
        return -1; // Should not reach here
    }

    // --- STRATEGY 2: Grid-Based Greedy (For Huge N) ---
    // Robust perimeter scan + bounds safety + nextIdx fallback.
    // Assumes coords are intended to be within [0..MAX_COORD], but will safely handle out-of-range by clamping.
    void solveGreedyLargeN() {
        static vector<int> grid[MAX_COORD + 1][MAX_COORD + 1];

        // Clear grid
        for (int i = 0; i <= MAX_COORD; ++i)
            for (int j = 0; j <= MAX_COORD; ++j)
                grid[i][j].clear();

        // Put points into grid (with safety clamp)
        for (int i = 0; i < n; ++i) {
            int x = cities[i].x;
            int y = cities[i].y;
            if (x < 0) x = 0; else if (x > MAX_COORD) x = MAX_COORD;
            if (y < 0) y = 0; else if (y > MAX_COORD) y = MAX_COORD;
            grid[x][y].push_back(i);
        }

        globalBestAnt.path.clear();
        globalBestAnt.path.reserve(n);
        globalBestAnt.tourLength = 0;

        // Start at node 0
        int currentIdx = 0;
        {
            int sx = cities[0].x, sy = cities[0].y;
            if (sx < 0) sx = 0; else if (sx > MAX_COORD) sx = MAX_COORD;
            if (sy < 0) sy = 0; else if (sy > MAX_COORD) sy = MAX_COORD;

            auto& startCell = grid[sx][sy];
            for (size_t k = 0; k < startCell.size(); ++k) {
                if (startCell[k] == 0) {
                    startCell[k] = startCell.back();
                    startCell.pop_back();
                    break;
                }
            }
        }
        globalBestAnt.path.push_back(0);

        // Helper lambda to test one cell
        auto tryCell = [&](int x, int y, int& nextIdx, double& minDist, bool& found) {
            auto& cell = grid[x][y];
            if (cell.empty()) return;
            for (int candidate : cell) {
                double d = getDistLarge(currentIdx, candidate);
                if (d < minDist) {
                    minDist = d;
                    nextIdx = candidate;
                    found = true;
                }
            }
        };

        // Helper lambda: fallback to find ANY remaining city (guarantees no crash)
        auto findAnyRemaining = [&]() -> int {
            for (int x = 0; x <= MAX_COORD; ++x) {
                for (int y = 0; y <= MAX_COORD; ++y) {
                    if (!grid[x][y].empty()) return grid[x][y].back();
                }
            }
            return -1; // should only happen if n == 0
        };

        for (int step = 1; step < n; ++step) {
            int cx = cities[currentIdx].x;
            int cy = cities[currentIdx].y;
            if (cx < 0) cx = 0; else if (cx > MAX_COORD) cx = MAX_COORD;
            if (cy < 0) cy = 0; else if (cy > MAX_COORD) cy = MAX_COORD;

            int nextIdx = -1;
            double minDist = numeric_limits<double>::max();
            bool found = false;

            // Search expanding square rings around (cx, cy)
            for (int r = 0; r <= MAX_COORD; ++r) {
                int xMin = max(0, cx - r);
                int xMax = min(MAX_COORD, cx + r);
                int yMin = max(0, cy - r);
                int yMax = min(MAX_COORD, cy + r);

                // Top & bottom edges
                for (int x = xMin; x <= xMax; ++x) {
                    tryCell(x, yMin, nextIdx, minDist, found);
                    if (found) break;
                    if (yMax != yMin) {
                        tryCell(x, yMax, nextIdx, minDist, found);
                        if (found) break;
                    }
                }
                if (found) break;

                // Left & right edges (excluding corners already checked)
                for (int y = yMin + 1; y <= yMax - 1; ++y) {
                    tryCell(xMin, y, nextIdx, minDist, found);
                    if (found) break;
                    if (xMax != xMin) {
                        tryCell(xMax, y, nextIdx, minDist, found);
                        if (found) break;
                    }
                }
                if (found) break;
            }

            // Absolute safety: if nothing found (shouldn’t happen), pick any remaining
            if (nextIdx == -1) {
                nextIdx = findAnyRemaining();
                if (nextIdx == -1) break; // nothing left (should not happen)
                minDist = getDistLarge(currentIdx, nextIdx);
            }

            // Remove nextIdx from its grid cell (swap&pop)
            int nx = cities[nextIdx].x;
            int ny = cities[nextIdx].y;
            if (nx < 0) nx = 0; else if (nx > MAX_COORD) nx = MAX_COORD;
            if (ny < 0) ny = 0; else if (ny > MAX_COORD) ny = MAX_COORD;

            auto& cell = grid[nx][ny];
            for (size_t k = 0; k < cell.size(); ++k) {
                if (cell[k] == nextIdx) {
                    cell[k] = cell.back();
                    cell.pop_back();
                    break;
                }
            }

            globalBestAnt.tourLength += minDist;
            globalBestAnt.path.push_back(nextIdx);
            currentIdx = nextIdx;
        }

        // Close loop
        globalBestAnt.tourLength += getDistLarge(currentIdx, globalBestAnt.path[0]);
    }

    static void* threadEntry(void* arg) {
        ThreadContext* ctx = static_cast<ThreadContext*>(arg);
        AntColonyOptimization* aco = ctx->aco_instance;
        mt19937 local_rng(ctx->thread_id + 5489u);
        vector<double> probsBuffer;
        if (aco->n <= ACO_LIMIT) probsBuffer.reserve(aco->n);

        for (int i = ctx->start_ant_idx; i < ctx->end_ant_idx; i++) {
            aco->constructSolution(i, local_rng, probsBuffer);
        }
        return nullptr;
    }
};

int main(int argc, char* argv[]) {
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
    while (i < n && file >> cities[i].first >> cities[i].second) i++;
    if (i < n) { cities.resize(i); n = i; }

    AntColonyOptimization aco(cities);
    aco.solve();
    return 0;
}