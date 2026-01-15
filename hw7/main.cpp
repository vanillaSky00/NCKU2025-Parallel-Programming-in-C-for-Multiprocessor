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
#include <chrono>
#include <new>

using namespace std;

// --- CONFIGURATION ---
const int NUM_ANTS = 30;
const int NUM_ITERATIONS = 200;
const double ALPHA = 1.0;
const double BETA = 2.0;
const double EVAPORATION = 0.2;
const double Q = 1.0;
const int NUM_THREADS = 4;

// --- CRITICAL THRESHOLDS ---
const int ACO_LIMIT = 5000;
const int CANDIDATE_K = 25;
const int MAX_COORD = 1000;
const double TIME_LIMIT = 9.5;

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

// ---- Thread contexts (keep your style) ----
struct RangeContext {
    int thread_id;
    int start;
    int end;
    AntColonyOptimization* aco_instance;
    int iter; // used by ant-worker RNG
    Ant* iterBest; // optional pointer usage (not needed)
};

class AntColonyOptimization {
public:
    int n;
private:
    vector<Point> cities;

    vector<float> distMatrix;   // N*N for N<=5000
    vector<int> candidates;     // N*K
    vector<float> pheromones;   // N*K (SPARSE)

    vector<Ant> ants;
    Ant globalBestAnt;

    chrono::steady_clock::time_point startTime;

public:
    AntColonyOptimization(const vector<pair<int, int>>& inputCities)
        : n((int)inputCities.size()),
          globalBestAnt((int)inputCities.size()) {

        startTime = chrono::steady_clock::now();

        cities.reserve(n);
        for (auto& pos : inputCities) {
            cities.push_back(Point{pos.first, pos.second});
        }

        if (n <= ACO_LIMIT) {
            try {
                // Precompute distMatrix (parallel)
                distMatrix.resize((size_t)n * n);

                pthread_t threads[NUM_THREADS];
                RangeContext ctx[NUM_THREADS];

                int chunk = (n + NUM_THREADS - 1) / NUM_THREADS;
                for (int t = 0; t < NUM_THREADS; ++t) {
                    ctx[t].thread_id = t;
                    ctx[t].start = t * chunk;
                    ctx[t].end = min(n, (t + 1) * chunk);
                    ctx[t].aco_instance = this;
                    pthread_create(&threads[t], nullptr, &AntColonyOptimization::threadDistEntry, &ctx[t]);
                }
                for (int t = 0; t < NUM_THREADS; ++t) pthread_join(threads[t], nullptr);

                // Initial pheromone estimate (rough)
                TAU_MAX = 1.0 / (EVAPORATION * n * 10.0);
                TAU_MIN = TAU_MAX / (2.0 * n);

                buildCandidateLists(); // parallel inside

                pheromones.resize((size_t)n * CANDIDATE_K, (float)TAU_MAX);

                for (int i = 0; i < NUM_ANTS; i++) {
                    ants.emplace_back(n);
                }
            }
            catch (const std::bad_alloc&) {
                cerr << "[Error] Memory allocation failed." << endl;
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

        // ROTATE TO START AT 0
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
    // -------- Parallel distMatrix --------
    static void* threadDistEntry(void* arg) {
        RangeContext* ctx = static_cast<RangeContext*>(arg);
        AntColonyOptimization* aco = ctx->aco_instance;
        int n = aco->n;

        for (int i = ctx->start; i < ctx->end; ++i) {
            for (int j = 0; j < n; ++j) {
                long long dx = (long long)aco->cities[i].x - (long long)aco->cities[j].x;
                long long dy = (long long)aco->cities[i].y - (long long)aco->cities[j].y;
                aco->distMatrix[(size_t)i * n + j] = (float)sqrt((double)(dx*dx + dy*dy));
            }
        }
        return nullptr;
    }

    // -------- Candidate list (parallel over i) --------
    void buildCandidateLists() {
        candidates.resize((size_t)n * CANDIDATE_K, -1);

        pthread_t threads[NUM_THREADS];
        RangeContext ctx[NUM_THREADS];

        int chunk = (n + NUM_THREADS - 1) / NUM_THREADS;
        for (int t = 0; t < NUM_THREADS; ++t) {
            ctx[t].thread_id = t;
            ctx[t].start = t * chunk;
            ctx[t].end = min(n, (t + 1) * chunk);
            ctx[t].aco_instance = this;
            pthread_create(&threads[t], nullptr, &AntColonyOptimization::threadCandidatesEntry, &ctx[t]);
        }
        for (int t = 0; t < NUM_THREADS; ++t) pthread_join(threads[t], nullptr);
    }

    static void* threadCandidatesEntry(void* arg) {
        RangeContext* ctx = static_cast<RangeContext*>(arg);
        AntColonyOptimization* aco = ctx->aco_instance;
        int n = aco->n;

        for (int i = ctx->start; i < ctx->end; ++i) {
            vector<pair<float, int>> neighbors;
            neighbors.reserve((size_t)n - 1);

            for (int j = 0; j < n; ++j) {
                if (i != j) neighbors.push_back({aco->distMatrix[(size_t)i * n + j], j});
            }

            if (n - 1 > CANDIDATE_K) {
                nth_element(neighbors.begin(), neighbors.begin() + CANDIDATE_K, neighbors.end());
                sort(neighbors.begin(), neighbors.begin() + CANDIDATE_K);
            } else {
                sort(neighbors.begin(), neighbors.end());
            }

            int limit = min(n - 1, CANDIDATE_K);
            for (int k = 0; k < limit; ++k) {
                aco->candidates[(size_t)i * CANDIDATE_K + k] = neighbors[k].second;
            }
        }
        return nullptr;
    }

    // -------- ACO main loop (pthread for ants + pheromone evaporation/clamp) --------
    void solveACO() {
        pthread_t threads[NUM_THREADS];
        RangeContext ctx[NUM_THREADS];

        for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
            auto now = chrono::steady_clock::now();
            chrono::duration<double> elapsed = now - startTime;
            if (elapsed.count() > TIME_LIMIT) break;

            // 1) Construct solutions in parallel (ants)
            int ants_per_thread = (NUM_ANTS + NUM_THREADS - 1) / NUM_THREADS;
            for (int t = 0; t < NUM_THREADS; ++t) {
                ctx[t].thread_id = t;
                ctx[t].start = t * ants_per_thread;
                ctx[t].end = min(NUM_ANTS, (t + 1) * ants_per_thread);
                ctx[t].aco_instance = this;
                ctx[t].iter = iter;
                pthread_create(&threads[t], nullptr, &AntColonyOptimization::threadAntEntry, &ctx[t]);
            }
            for (int t = 0; t < NUM_THREADS; ++t) pthread_join(threads[t], nullptr);

            // 2) Find Iteration Best
            Ant* iterBest = &ants[0];
            for (int i = 1; i < NUM_ANTS; i++) {
                if (ants[i].tourLength < iterBest->tourLength) iterBest = &ants[i];
            }

            // 3) Local search on best only
            localSearch2Opt(*iterBest);

            // 4) Update global best + MMAS bounds
            if (iterBest->tourLength < globalBestAnt.tourLength) {
                globalBestAnt = *iterBest;
                TAU_MAX = 1.0 / (EVAPORATION * globalBestAnt.tourLength);
                TAU_MIN = TAU_MAX / (2.0 * n);
            }

            // 5) Pheromone update (parallel evaporation/clamp, serial deposit)
            pheromoneUpdateMMAS(iterBest);
        }
    }

    static void* threadAntEntry(void* arg) {
        RangeContext* ctx = static_cast<RangeContext*>(arg);
        AntColonyOptimization* aco = ctx->aco_instance;

        // Thread-local RNG & buffer
        mt19937 local_rng(5489u + (unsigned)ctx->iter + (unsigned)ctx->thread_id * 100u);
        vector<double> probsBuffer;
        probsBuffer.reserve(CANDIDATE_K);

        for (int antId = ctx->start; antId < ctx->end; ++antId) {
            aco->constructSolution(antId, local_rng, probsBuffer);
        }
        return nullptr;
    }

    // --- OPTIMIZED 2-OPT (CANDIDATE RESTRICTED) ---
    void localSearch2Opt(Ant& ant) {
        vector<int> pos(n);
        for (int i = 0; i < n; ++i) pos[ant.path[i]] = i;

        bool improvement = true;
        int passes = 0;

        while (improvement && passes < 2) {
            improvement = false;
            passes++;

            for (int i = 0; i < n; ++i) {
                int u = ant.path[i];
                int v = ant.path[(i + 1) % n];

                for (int k = 0; k < CANDIDATE_K; ++k) {
                    int x = candidates[(size_t)u * CANDIDATE_K + k];
                    if (x < 0) continue;
                    if (x == v || x == ant.path[(i - 1 + n) % n]) continue;

                    int idx_x = pos[x];
                    int idx_y = (idx_x + 1) % n;
                    int y = ant.path[idx_y];

                    float currentDist = distMatrix[(size_t)u*n + v] + distMatrix[(size_t)x*n + y];
                    float newDist = distMatrix[(size_t)u*n + x] + distMatrix[(size_t)v*n + y];

                    if (newDist < currentDist) {
                        int start = i + 1;
                        int end = idx_x;
                        if (end > start) {
                            reverse(ant.path.begin() + start, ant.path.begin() + end + 1);
                            for (int z = start; z <= end; ++z) pos[ant.path[z]] = z;
                            ant.tourLength -= (currentDist - newDist);
                            improvement = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    // --- SPARSE MMAS UPDATE (O(N*K)) ---
    void pheromoneUpdateMMAS(Ant* iterBest) {
        pthread_t threads[NUM_THREADS];
        RangeContext ctx[NUM_THREADS];

        // 1) Evaporate + clamp min (parallel over pheromones)
        int m = (int)pheromones.size();
        int chunk = (m + NUM_THREADS - 1) / NUM_THREADS;

        for (int t = 0; t < NUM_THREADS; ++t) {
            ctx[t].thread_id = t;
            ctx[t].start = t * chunk;
            ctx[t].end = min(m, (t + 1) * chunk);
            ctx[t].aco_instance = this;
            pthread_create(&threads[t], nullptr, &AntColonyOptimization::threadEvapEntry, &ctx[t]);
        }
        for (int t = 0; t < NUM_THREADS; ++t) pthread_join(threads[t], nullptr);

        // 2) Deposit (serial, cheap: N edges, each scans K=25)
        double contribution = 1.0 / iterBest->tourLength;

        for (int i = 0; i < n; i++) {
            int u = iterBest->path[i];
            int v = iterBest->path[(i + 1) % n];

            for (int k = 0; k < CANDIDATE_K; ++k) {
                int nb = candidates[(size_t)u * CANDIDATE_K + k];
                if (nb == v) {
                    pheromones[(size_t)u * CANDIDATE_K + k] += (float)contribution;
                    break;
                }
            }
            for (int k = 0; k < CANDIDATE_K; ++k) {
                int nb = candidates[(size_t)v * CANDIDATE_K + k];
                if (nb == u) {
                    pheromones[(size_t)v * CANDIDATE_K + k] += (float)contribution;
                    break;
                }
            }
        }

        // 3) Clamp max (parallel)
        for (int t = 0; t < NUM_THREADS; ++t) {
            ctx[t].thread_id = t;
            ctx[t].start = t * chunk;
            ctx[t].end = min(m, (t + 1) * chunk);
            ctx[t].aco_instance = this;
            pthread_create(&threads[t], nullptr, &AntColonyOptimization::threadClampMaxEntry, &ctx[t]);
        }
        for (int t = 0; t < NUM_THREADS; ++t) pthread_join(threads[t], nullptr);
    }

    static void* threadEvapEntry(void* arg) {
        RangeContext* ctx = static_cast<RangeContext*>(arg);
        AntColonyOptimization* aco = ctx->aco_instance;

        for (int idx = ctx->start; idx < ctx->end; ++idx) {
            float p = aco->pheromones[(size_t)idx];
            p *= (1.0f - (float)EVAPORATION);
            if (p < (float)TAU_MIN) p = (float)TAU_MIN;
            aco->pheromones[(size_t)idx] = p;
        }
        return nullptr;
    }

    static void* threadClampMaxEntry(void* arg) {
        RangeContext* ctx = static_cast<RangeContext*>(arg);
        AntColonyOptimization* aco = ctx->aco_instance;

        for (int idx = ctx->start; idx < ctx->end; ++idx) {
            float p = aco->pheromones[(size_t)idx];
            if (p > (float)TAU_MAX) p = (float)TAU_MAX;
            aco->pheromones[(size_t)idx] = p;
        }
        return nullptr;
    }

    // --- Standard Helpers ---
    inline float getDist(int i, int j) const { return distMatrix[(size_t)i * n + j]; }

    inline double getDistLarge(int i, int j) {
        long long dx = (long long)cities[i].x - (long long)cities[j].x;
        long long dy = (long long)cities[i].y - (long long)cities[j].y;
        return sqrt((double)(dx*dx + dy*dy));
    }

public:
    void constructSolution(int antId, mt19937& local_rng, vector<double>& probsBuffer) {
        Ant& ant = ants[antId];
        ant.reset(n);

        int currentCity = 0; // Forced Start
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

        int limit = min(n - 1, CANDIDATE_K);

        // Candidate-only roulette
        for (int k = 0; k < limit; k++) {
            int neighbor = candidates[(size_t)currentCity * CANDIDATE_K + k];
            if (neighbor < 0) { probs.push_back(0.0); continue; }

            if (!visited[neighbor]) {
                double tau = (double)pheromones[(size_t)currentCity * CANDIDATE_K + k];
                double dist = getDist(currentCity, neighbor);
                double eta = 1.0 / (dist + 1e-10);
                double p = pow(tau, ALPHA) * pow(eta, BETA);
                probs.push_back(p);
                sum += p;
            } else {
                probs.push_back(0.0);
            }
        }

        if (sum == 0.0) {
            // Fallback: scan all, tau=TAU_MIN
            probs.clear();
            probs.reserve(n);

            for (int i = 0; i < n; i++) {
                if (!visited[i]) {
                    double tau = TAU_MIN;
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
            double r = dist(local_rng);
            double partialSum = 0.0;

            for (int i = 0; i < n; i++) {
                if (!visited[i]) {
                    partialSum += probs[i];
                    if (partialSum >= r) return i;
                }
            }
            for (int i = 0; i < n; i++) if (!visited[i]) return i;
        } else {
            uniform_real_distribution<double> dist(0.0, sum);
            double r = dist(local_rng);
            double partialSum = 0.0;

            for (int k = 0; k < limit; k++) {
                int neighbor = candidates[(size_t)currentCity * CANDIDATE_K + k];
                if (neighbor >= 0 && !visited[neighbor]) {
                    partialSum += probs[k];
                    if (partialSum >= r) return neighbor;
                }
            }

            for (int k = 0; k < limit; k++) {
                int neighbor = candidates[(size_t)currentCity * CANDIDATE_K + k];
                if (neighbor >= 0 && !visited[neighbor]) return neighbor;
            }
        }

        return -1;
    }

    void solveGreedyLargeN() {
        static vector<int> grid[MAX_COORD + 1][MAX_COORD + 1];

        for (int i = 0; i <= MAX_COORD; ++i)
            for (int j = 0; j <= MAX_COORD; ++j)
                grid[i][j].clear();

        for (int i = 0; i < n; ++i) {
            int x = clamp(cities[i].x, 0, MAX_COORD);
            int y = clamp(cities[i].y, 0, MAX_COORD);
            grid[x][y].push_back(i);
        }

        globalBestAnt.path.clear();
        globalBestAnt.path.reserve(n);
        globalBestAnt.tourLength = 0;
        int currentIdx = 0;

        int sx = clamp(cities[0].x, 0, MAX_COORD);
        int sy = clamp(cities[0].y, 0, MAX_COORD);
        auto& sc = grid[sx][sy];
        for (size_t k = 0; k < sc.size(); ++k)
            if (sc[k] == 0) { sc[k] = sc.back(); sc.pop_back(); break; }

        globalBestAnt.path.push_back(0);

        for (int step = 1; step < n; ++step) {
            int cx = clamp(cities[currentIdx].x, 0, MAX_COORD);
            int cy = clamp(cities[currentIdx].y, 0, MAX_COORD);
            int nextIdx = -1;
            double minDist = numeric_limits<double>::max();
            bool found = false;

            for (int r = 0; r <= MAX_COORD; ++r) {
                int xMin = max(0, cx - r), xMax = min(MAX_COORD, cx + r);
                int yMin = max(0, cy - r), yMax = min(MAX_COORD, cy + r);

                auto check = [&](int x, int y) {
                    if (grid[x][y].empty()) return;
                    for (int c : grid[x][y]) {
                        double d = getDistLarge(currentIdx, c);
                        if (d < minDist) { minDist = d; nextIdx = c; found = true; }
                    }
                };

                for (int x = xMin; x <= xMax; ++x) {
                    check(x, yMin);
                    if (yMax != yMin) check(x, yMax);
                }
                for (int y = yMin + 1; y < yMax; ++y) {
                    check(xMin, y);
                    if (xMax != xMin) check(xMax, y);
                }

                if (found) break;
            }

            if (!found) {
                for (int i = 0; i <= MAX_COORD; ++i)
                    for (int j = 0; j <= MAX_COORD; ++j)
                        if (!grid[i][j].empty()) { nextIdx = grid[i][j].back(); goto Found; }
                Found:;
                minDist = getDistLarge(currentIdx, nextIdx);
            }

            int nx = clamp(cities[nextIdx].x, 0, MAX_COORD);
            int ny = clamp(cities[nextIdx].y, 0, MAX_COORD);
            auto& cell = grid[nx][ny];
            for (size_t k = 0; k < cell.size(); ++k)
                if (cell[k] == nextIdx) { cell[k] = cell.back(); cell.pop_back(); break; }

            globalBestAnt.tourLength += minDist;
            globalBestAnt.path.push_back(nextIdx);
            currentIdx = nextIdx;
        }

        globalBestAnt.tourLength += getDistLarge(currentIdx, globalBestAnt.path[0]);
    }
};

int main(int argc, char* argv[]) {
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
    while (i < n && file >> cities[i].first >> cities[i].second) i++;
    if (i < n) { cities.resize(i); n = i; }

    AntColonyOptimization aco(cities);
    aco.solve();
    return 0;
}