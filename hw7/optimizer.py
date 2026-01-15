import subprocess
import itertools


# --- CONFIGURATION ---
param_grid = {
    "ants": [30],
    "iterations": [200],
    "alpha": [1.0],       
    "beta": [2.0],        
    "evaporation": [0.2], 
    "Q": [1.0],
    "threads": [4]             
}
# param_grid = {
#     "ants": [20, 50],
#     "iterations": [50, 100],
#     "alpha": [1.0, 0.5],       
#     "beta": [2.0, 5.0],        
#     "evaporation": [0.5, 0.1], 
#     "Q": [100.0],
#     "threads": [4]             
# }

input_filenames = [
    "data/input/data/001.in",
    "data/input/data/002.in",
    "data/input/data/003.in", 
    "data/input/data/004.in",
    "data/input/data/005.in", 
    "data/input/data/006.in",
    "data/input/data/007.in",
    "data/input/data/008.in"
]

keys = list(param_grid.keys())
values = list(param_grid.values())
combinations = list(itertools.product(*values))

print(f"Testing {len(combinations)} combinations per file...")

for target_file in input_filenames:
    print("\n" + "=" * 60)
    print(f"Optimizing for FILE: {target_file}")
    print("=" * 60)
    print(f"{'Score':<15} | Params")
    print("-" * 60)

    # RESET best score for this specific file
    local_best_score = float('inf')
    local_best_params = None

    for combo in combinations:
        ants, iterations, alpha, beta, evap, q, threads = combo
        cmd = ["./hw7", str(ants), str(iterations), str(alpha), str(beta), str(evap), str(q)]
        
        try:
            result = subprocess.run(
                cmd, 
                input=target_file, 
                text=True, 
                capture_output=True, 
                check=True
            )
            
            output_lines = result.stdout.strip().split('\n')
            if not output_lines: continue
            
            score = float(output_lines[0])
            print(f"{score:<15.4f} | {combo}")

            if score < local_best_score:
                local_best_score = score
                local_best_params = combo

        except Exception as e:
            print(f"Error: {e}")

    print("-" * 60)
    print(f"BEST FOR {target_file}: {local_best_score}")
    
    if local_best_params is not None:
        print(f"CONFIG: {dict(zip(keys, local_best_params))}")
    else:
        print("CONFIG: No valid result found (C++ might have crashed or timed out)")