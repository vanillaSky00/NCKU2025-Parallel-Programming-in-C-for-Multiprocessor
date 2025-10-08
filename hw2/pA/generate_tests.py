#!/usr/bin/env python3
"""
Test data generation script for Problem A
Used to generate test data for divisor sum calculation with random n in range

Usage:
python3 generate_tests.py --min 1000 --max 10000 --output test_random
python3 generate_tests.py --min 1000000 --max 10000000 --output test_large
"""

import argparse
import os
import random

def generate_test_data(min_n, max_n, output_file, seed=None):
    output_dir = "data/input"
    os.makedirs(output_dir, exist_ok=True)
    
    output_path = os.path.join(output_dir, output_file)
    

    if seed is not None:
        random.seed(seed)
    

    n = random.randint(min_n, max_n)
    
    print(f"Generating test data for n={n} (range: {min_n} to {max_n})...")
    
    with open(output_path, 'w') as f:
        f.write(f"{n}\n")
    
    print(f"Generated test data: {output_path}")
    print(f"Random n value: {n}")
    
    return n

def main():
    parser = argparse.ArgumentParser(description='Generate test data for Problem A (Divisor Sum) with random n in range')
    parser.add_argument('--min', type=str, required=True, help='Minimum value for n (1 <= min <= 1e18)')
    parser.add_argument('--max', type=str, required=True, help='Maximum value for n (1 <= max <= 1e18)')
    parser.add_argument('--output', type=str, required=True, help='Output file name (e.g. test_random)')
    parser.add_argument('--seed', type=int, default=None, help='Random seed (optional)')
    
    args = parser.parse_args()
    
    try:
        min_n = int(args.min)
        max_n = int(args.max)
    except ValueError:
        print("Error: min and max must be valid integers")
        return
    
    if min_n < 1 or min_n > 10**18:
        print("Error: min must be between 1 and 10^18")
        return
    
    if max_n < 1 or max_n > 10**18:
        print("Error: max must be between 1 and 10^18")
        return
    
    if min_n > max_n:
        print("Error: min must be less than or equal to max")
        return
    
    if args.seed is not None:
        print(f"Using random seed: {args.seed}")
    
    generate_test_data(min_n, max_n, args.output, args.seed)

if __name__ == "__main__":
    main()
