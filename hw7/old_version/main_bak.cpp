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
const double EVAPORATION = 0.1;
const double Q = 100.0;
const int NUM_THREADS = 4;

// int NUM_ANTS = 20; 
// int NUM_ITERATIONS = 50; 
// double ALPHA = 1.0;
// double BETA = 2.0; 
// double EVAPORATION = 0.5;
// double Q = 100.0;
// int NUM_THREADS = 4;

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
private:
    int n; // city num
    vector<Point> cities;
    vector<float> pheromones; // pheromones between city_ij

    vector<Ant> ants;
    Ant globalBestAnt;

    mt19937 rng; // random num

public:
    AntColonyOptimization(const vector<pair<int, int>>& inputCities)
        : n(inputCities.size()), 
          globalBestAnt(inputCities.size()) {
        
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

        pthread_t threads[NUM_THREADS];
        ThreadContext contexts[NUM_THREADS];
        int ants_per_thread = NUM_ANTS / NUM_THREADS;

        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {

            // Parallel steps:
            for (int t = 0; t < NUM_THREADS; t++) {
                contexts[t].thread_id = t;
                contexts[t].start_ant_idx = t * ants_per_thread;
                contexts[t].end_ant_idx = (t == NUM_THREADS - 1) ? NUM_ANTS : (t + 1) * ants_per_thread;
                contexts[t].aco_instance = this;

                pthread_create(&threads[t], nullptr, AntColonyOptimization::threadEntry, &contexts[t]);
            }

            // Wait for they all finish then go next iter
            for (int t = 0; t < NUM_THREADS; t++) {
                pthread_join(threads[t], nullptr);
            }   

            // Serial steps: global updates
            daemonActions();
            pheromoneUpdate();
        }

        cout << fixed << setprecision(6) << globalBestAnt.tourLength << endl;
        for (size_t i = 0; i < globalBestAnt.path.size(); ++i) {
            cout << globalBestAnt.path[i] << (i == globalBestAnt.path.size() - 1 ? "" : " ");
        }
        cout << endl;
    }

    
    /**
     * [Note] Made public so threadEntry can access it (or make threadEntry a friend)
     * Simulates the complete journey of "a single ant".
     * 1. Clear the ant's memory.
     * 2. Place the ant at a random starting city.
     * 3. Loops N - 1 times, calling selectNextCity() each step to pick where to go.
     * 4. Close the loop and sum up the tourLength
     */
    void constructSolution(int antId, mt19937& local_rng, vector<double>& probBuffer) {
        Ant& ant = ants[antId];
        ant.reset(n);

        uniform_int_distribution<int> dist(0, n-1);
        int currentCity = dist(local_rng);

        ant.path.push_back(currentCity);
        ant.visited[currentCity] = true;

        for (int step = 0; step < n - 1; step++) {
            int nextCity = selectNextCity(currentCity, ant.visited, local_rng, probBuffer);
            ant.path.push_back(nextCity);
            ant.tourLength += getDist(currentCity, nextCity);
            ant.visited[nextCity] = true;
            currentCity = nextCity;
        }
        
        ant.tourLength += getDist(currentCity, ant.path[0]);
    }

private:
    inline double getDist(int i, int j) {
        double dx = cities[i].x - cities[j].x;
        double dy = cities[i].y - cities[j].y;
        return sqrt(dx*dx + dy*dy);
    }

    inline float& getPheromone(int i, int j) {
        return pheromones[i * n + j];
    }

    /**
     * The decision brain 
     * 1. check all unvisited city 
     * 2. assign scores for unvisited city according to ACO formula
     *    Score = (Pheromone)^alpah + (1/Distance)^beta
     * 3. roll a random number. Cities with higher scores have a higher 
     *    chance of being picked, but it is not guaranteed 
     *    (this randomness allows exploration).
     */
    int selectNextCity(int currentCity, const vector<bool>& visited, mt19937& local_rng, vector<double>& probs) {
        
        // Reset the buffer (does not free memory, just sets size to 0)
        probs.clear();
        double sum = 0.0;
        
        for (int i = 0; i < n; i++) {
            if (!visited[i]) {
                double tau = (double)getPheromone(currentCity, i);
                double dist = getDist(currentCity, i);
                double eta = 1.0 / (dist + 1e-10);
                
                double p = pow(tau, ALPHA) * pow(eta, BETA);
                probs.push_back(p);
                sum += p;
            } 
            else {
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

        // Fallback
        for (int i = 0; i < n; i++) if (!visited[i]) return i;
        return -1;
    }

    /**
     * The Record Keeper. Find best ant with shorter path
     */
    void daemonActions() {
        for (const auto& ant : ants) {
            if (ant.tourLength < globalBestAnt.tourLength) {
                globalBestAnt = ant;
            }
        }
    }

    /**
     * 1. Evaperation
     * 2. Deposit
     */
    void pheromoneUpdate() {
        for (size_t i = 0; i < pheromones.size(); ++i) {
            pheromones[i] *= (1.0f - (float)EVAPORATION);
        }

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

    /**
     * Modify here to add things to thread
     */
    static void* threadEntry(void* arg) {
        ThreadContext* ctx = static_cast<ThreadContext*>(arg);
        AntColonyOptimization* aco = ctx->aco_instance;

        mt19937 local_rng(ctx->thread_id + 5489u);

        vector<double> probBuffer;
        probBuffer.reserve(aco->n);

        for (int i = ctx->start_ant_idx; i < ctx->end_ant_idx; i++) {
            aco->constructSolution(i, local_rng, probBuffer);
        }

        return nullptr;
    }
};

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string filename;
    if (!(cin >> filename)) return 0;

    // OVERWRITE defaults if arguments are provided
    // Order: ./hw7 <ants> <iter> <alpha> <beta> <evap> <Q>
    // if (argc >= 7) {
    //     NUM_ANTS = stoi(argv[1]);
    //     NUM_ITERATIONS = stoi(argv[2]);
    //     ALPHA = stod(argv[3]);
    //     BETA = stod(argv[4]);
    //     EVAPORATION = stod(argv[5]);
    //     Q = stod(argv[6]);
    // }

    ifstream file(filename);
    if (!file) {
        cerr << "[Error] opening file." << endl;
        return 1;
    }

    int n;
    if (!(file >> n)) return 0;
    
    vector<pair<int, int>> cities(n);

    int i = 0;
    while (i < n && file >> cities[i].first >> cities[i].second) {
        i++;
    }

    if (i < n) {
        cities.resize(i);
        n = i;
    }

    AntColonyOptimization aco(cities);
    aco.solve();
    return 0;
}