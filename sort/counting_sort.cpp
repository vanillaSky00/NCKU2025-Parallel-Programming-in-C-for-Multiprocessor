#include <bits/stdc++.h>
#include <mpi.h>
using namespace std;

vector<int> counting_sort(int max_val, vector<int>& nums, MPI_Comm comm) {
    int world_size, me;
    MPI_Comm_size(comm, &world_size);
    MPI_Comm_rank(comm, &me);

    int n = nums.size();
    int chunk = n / world_size;
    int start = me * chunk;
    int end = (me == world_size - 1) ? n : (me + 1) * chunk;

    vector<long long> local_freq(max_val + 1, 0);

    for (int i = start; i < end; i++) local_freq[nums[i]]++;

    vector<long long> global_freq;
    if (me == 0) global_freq.assign(max_val + 1, 0);
    
    MPI_Reduce(
        local_freq.data(),
        me == 0 ? global_freq.data() : nullptr, // receive buffer, only rank 0 receive
        max_val + 1,
        MPI_LONG_LONG,
        MPI_SUM,
        0,
        comm
    );
    
    vector<int> ans;
    if (me == 0) {
        ans.resize(n);
        int idx = 0;
        for (int i = 0; i <= max_val; i++) {
            while (global_freq[i]-- > 0) res[idx++] = i;
        }
    }

    return ans;
}

int main(int argc, char **argv) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);

    MPI_Init(&argc, &argv);

    int world_rank = 0;
    int world_size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int n = 0, max_val = 0;
    vector<int> nums;

    if (world_rank == 0) {
        string file_name;
        cin >> file_name;
        ifstream file(file_name);

        file >> n >> max_val;
        nums.resize(n);
        for (int i = 0; i < n; i++) 
            file >> nums[i];
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&max_val, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (world_rank != 0) 
        nums.resize(n);
    
    MPI_Bcast(nums.data(), n, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> ans = counting_sort(max_val, nums, MPI_COMM_WORLD);

    if (world_rank == 0) 
        for (int a : ans) cout << a << ' ';
    
    MPI_Finalize();
    return 0;
}