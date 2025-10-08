#!/usr/bin/env python3
"""
Test data generation script
Used to generate test data for Problem B

Usage:
python3 generate_tests.py --n 5 --m 10 --output input01
python3 generate_tests.py --n 100 --m 50 --output input02
"""

import argparse
import random
import os

def generate_test_data(n, m, output_file):
    output_dir = "data/input"
    os.makedirs(output_dir, exist_ok=True)
    
    output_path = os.path.join(output_dir, output_file)
    

    print(f"Generating abilities for {n} players...")
    abilities = []
    chunk_size = min(1000000, n) 
    for i in range(0, n, chunk_size):
        end_idx = min(i + chunk_size, n)
        chunk = [random.randint(1, 10**3) for _ in range(end_idx - i)]
        abilities.extend(chunk)
        if n > 100000:
            print(f"  Progress: {end_idx}/{n} abilities generated")
    
    print(f"Generating difficulties for {m} problems...")
    difficulties = []
    for i in range(0, m, chunk_size):
        end_idx = min(i + chunk_size, m)
        chunk = [random.randint(1, 10**3) for _ in range(end_idx - i)]
        difficulties.extend(chunk)
        if m > 100000:
            print(f"  Progress: {end_idx}/{m} difficulties generated")

    print(f"Writing to file: {output_path}")
    with open(output_path, 'w') as f:
        f.write(f"{n} {m}\n")
        f.write(" ".join(map(str, abilities)) + "\n")
        f.write(" ".join(map(str, difficulties)) + "\n")
    
    print(f"Generated test data: {output_path}")
    print(f"Number of players: {n}, Number of problems: {m}")

def main():
    parser = argparse.ArgumentParser(description='Generate test data for Problem B')
    parser.add_argument('--n', type=int, required=True, help='Number of players (1 <= n <= 5e7)')
    parser.add_argument('--m', type=int, required=True, help='Number of problems (1 <= m <= 5e7)')
    parser.add_argument('--output', type=str, required=True, help='Output file name (e.g. input01)')
    parser.add_argument('--seed', type=int, default=None, help='Random seed (optional)')
    
    args = parser.parse_args()
    
    if args.n < 1 or args.n > 5 * 10**7:
        print("Error: n must be between 1 and 5 * 10^7")
        return
    
    if args.m < 1 or args.m > 5 * 10**7:
        print("Error: m must be between 1 and 5 * 10^7")
        return
    
    if args.seed is not None:
        random.seed(args.seed)
        print(f"Using random seed: {args.seed}")
    
    generate_test_data(args.n, args.m, args.output)

if __name__ == "__main__":
    main()
