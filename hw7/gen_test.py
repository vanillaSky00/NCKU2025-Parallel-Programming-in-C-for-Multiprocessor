import random
import os

def generate_testcase(filename, n):
    """
    Generates a TSP test case file.
    Format:
    N
    x y
    ...
    """
    print(f"Generating {filename} with N={n}...")
    
    with open(filename, 'w') as f:
        # 1. Write N
        f.write(f"{n}\n")
        
        # 2. Write N lines of x y coordinates
        # Constraint: 0 <= x, y <= 1000
        for _ in range(n):
            x = random.randint(0, 1000)
            y = random.randint(0, 1000)
            f.write(f"{x} {y}\n")
            
    print(f"Done! Saved to {filename}")

def main():
    # Create a directory for the inputs
    output_dir = "data/input/data"
    os.makedirs(output_dir, exist_ok=True)

    # --- Round 1 Configuration (N = 200) ---
    generate_testcase(os.path.join(output_dir, "002.in"), 200)

    # --- Round 2 Configuration (N = 1,000) ---
    generate_testcase(os.path.join(output_dir, "003.in"), 1000)

    # --- Round 3 Configuration (N = 2,000 ~ 1,000,000) ---
    # Generating a few tiers to test your ACO scaling
    generate_testcase(os.path.join(output_dir, "004.in"), 2000)
    generate_testcase(os.path.join(output_dir, "005.in"), 5000)
    
    # WARNING: 10^6 is huge (approx 15MB file). 
    # Only uncomment this if you want to test the extreme limit.
    generate_testcase(os.path.join(output_dir, "006.in"), 10000)
    generate_testcase(os.path.join(output_dir, "007.in"), 100000)
    generate_testcase(os.path.join(output_dir, "008.in"), 1000000)

    print(f"\nAll test cases generated in {output_dir} folder.")

if __name__ == "__main__":
    main()