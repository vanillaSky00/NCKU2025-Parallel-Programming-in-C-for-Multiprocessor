import argparse
import subprocess
import numpy as np
import sys
import os
import time

current_seed = 0

try:
    import cupy as cp
    HAS_GPU = True

    xor_sum_kernel = cp.ReductionKernel(
        'T x',
        'T y',
        'x',
        'a ^ b',
        'y = a',
        '0',
        'xor_sum'
    )
except ImportError:
    HAS_GPU = False

MATRIX_VALUE_MAX = 1000

C_EXECUTABLE = './hw6'
if sys.platform.startswith('win'):
    C_EXECUTABLE = 'hw6.exe'


def load_from_file(path: str):
    """
    File format:
      n
      A: n*n ints
      B: n*n ints

    Returns: (n, A, B) where A,B are np.int32 with shape (n,n)
    """
    if not os.path.exists(path):
        raise FileNotFoundError(f"Input file not found: {path}")

    with open(path, "r") as f:
        first = f.readline().strip()
        if not first:
            raise RuntimeError(f"Input file '{path}' is empty")
        n = int(first)

        # Read the remaining 2*n*n integers
        data = np.fromfile(f, dtype=np.int32, sep=' ')

    expected_len = 2 * n * n
    if data.size != expected_len:
        raise RuntimeError(
            f"File format mismatch: expected {expected_len} ints after n, got {data.size}.\n"
            f"Check that the file contains exactly A then B, each n lines of n integers."
        )

    A = data[:n*n].reshape((n, n))
    B = data[n*n:].reshape((n, n))
    return n, A, B


def calc_expected_xor(A_cpu: np.ndarray, B_cpu: np.ndarray) -> int:
    """
    Compute expected XOR of all elements of C=A*B using GPU (CuPy) if available,
    otherwise CPU (NumPy).
    """
    n = A_cpu.shape[0]
    print(f"[*] Computing expected XOR via Python for N={n}...")

    if HAS_GPU:
        print("[*] 🚀 GPU Detected (CuPy)! Using GPU for verification.")

        A_gpu = cp.asarray(A_cpu)
        B_gpu = cp.asarray(B_cpu)

        start_calc = time.perf_counter()
        C_gpu = cp.matmul(A_gpu, B_gpu)
        expected_xor = int(xor_sum_kernel(C_gpu, axis=None))
        end_calc = time.perf_counter()

        print(f"    - GPU Matrix Mul & XOR time: {end_calc - start_calc:.4f}s")
        return expected_xor

    else:
        print("[*] 🐢 No GPU found. Using CPU (NumPy).")
        print("    - WARNING: N=5000 CPU matmul can take a very long time.")

        start_calc = time.perf_counter()
        C_cpu = np.matmul(A_cpu, B_cpu)
        expected_xor = int(np.bitwise_xor.reduce(C_cpu.ravel()))
        end_calc = time.perf_counter()

        print(f"    - CPU Matrix Mul & XOR time: {end_calc - start_calc:.4f}s")
        return expected_xor


def generate_and_calc(n: int):
    """
    Auto choose GPU/CPU to generate A,B and compute expected XOR of C=A*B.
    Return (expected_xor, A_cpu, B_cpu)
    """
    print(f"[*] Initializing for N={n}...")

    if HAS_GPU:
        print("[*] 🚀 GPU Detected (CuPy)! Using GPU for calculation.")

        A_gpu = cp.random.randint(0, MATRIX_VALUE_MAX, size=(n, n), dtype=cp.int32)
        B_gpu = cp.random.randint(0, MATRIX_VALUE_MAX, size=(n, n), dtype=cp.int32)

        start_calc = time.perf_counter()
        C_gpu = cp.matmul(A_gpu, B_gpu)
        expected_xor = int(xor_sum_kernel(C_gpu, axis=None))
        end_calc = time.perf_counter()
        print(f"    - GPU Matrix Mul & XOR time: {end_calc - start_calc:.4f}s")

        print("    - Transferring data from GPU to CPU...")
        A_cpu = cp.asnumpy(A_gpu)
        B_cpu = cp.asnumpy(B_gpu)

    else:
        print("[*] 🐢 No GPU found. Using CPU (NumPy). This might be slow for large N.")

        A_cpu = np.random.randint(0, MATRIX_VALUE_MAX, size=(n, n), dtype=np.int32)
        B_cpu = np.random.randint(0, MATRIX_VALUE_MAX, size=(n, n), dtype=np.int32)

        start_calc = time.perf_counter()
        C_cpu = np.matmul(A_cpu, B_cpu)
        expected_xor = int(np.bitwise_xor.reduce(C_cpu.ravel()))
        end_calc = time.perf_counter()
        print(f"    - CPU Matrix Mul & XOR time: {end_calc - start_calc:.4f}s")

    return expected_xor, A_cpu, B_cpu


def write_to_file(path: str, n: int, A: np.ndarray, B: np.ndarray):
    print(f"[*] Writing data to {path} (Text mode)...")
    t_start = time.perf_counter()

    with open(path, 'w') as f:
        f.write(f"{n}\n")
        np.savetxt(f, A, fmt='%d')
        np.savetxt(f, B, fmt='%d')

    t_end = time.perf_counter()
    print(f"    - File Write time: {t_end - t_start:.4f}s")


def run_c_program(input_file: str, num_threads: int, expected_result=None):
    if not os.path.exists(C_EXECUTABLE):
        print(f"[!] Error: Executable '{C_EXECUTABLE}' not found.")
        sys.exit(1)
    if not os.path.exists(input_file):
        print(f"[!] Error: Input file '{input_file}' not found.")
        sys.exit(1)

    print(f"[*] Running C program: {C_EXECUTABLE} {num_threads} ...")
    print(f"[*] Feeding input file name: {input_file}")

    process = subprocess.Popen(
        [C_EXECUTABLE, str(num_threads)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    try:
        start_time = time.perf_counter()
        stdout_data, stderr_data = process.communicate(input=f"{input_file}\n")
        end_time = time.perf_counter()
    except KeyboardInterrupt:
        process.kill()
        raise

    elapsed_time = end_time - start_time
    print(f"[*] C execution time (Total): {elapsed_time:.4f}s")

    if process.returncode != 0:
        print(f"[!] C program failed (Exit code {process.returncode})")
        print(f"Stderr: {stderr_data}")
        sys.exit(1)

    raw_output = stdout_data.strip()
    if not raw_output:
        print("[!] Error: No output from C program.")
        return

    c_result = int(raw_output)

    if expected_result is None:
        print("\n" + "="*40)
        print("ℹ️  DONE (no Python verification)")
        print(f"   C Output: {c_result}")
        print(f"   Time taken: {elapsed_time:.4f}s")
        print("="*40)
        return

    if c_result == expected_result:
        print("\n" + "="*40)
        print("✅ SUCCESS: Results match!")
        print(f"   Answer: {c_result}")
        print(f"   Time taken: {elapsed_time:.4f}s")
        print("="*40)
    else:
        print("\n" + "="*40)
        print("❌ FAILURE: Results mismatch.")
        print(f"   Python (Expected): {expected_result}")
        print(f"   C Code (Actual):   {c_result}")
        print("="*40)


def main():
    parser = argparse.ArgumentParser(description='Test Matrix XOR with Threads')

    parser.add_argument('-n', type=int, help='Size of matrix (required unless --use-existing)')
    parser.add_argument('-t', '--threads', type=int, default=1, help='Number of threads for C program')
    parser.add_argument('-s', '--seed', type=int, default=42, help='Random seed for reproducibility')

    parser.add_argument('--input', default='matrix_input.txt',
                        help='Input file path (default: matrix_input.txt)')
    parser.add_argument('--use-existing', action='store_true',
                        help='Use existing --input file; skip generating/writing matrices')
    parser.add_argument('--verified', action='store_true',
                        help='Compute expected XOR in Python and verify against C output '
                             '(use with --use-existing; may be slow for large N)')

    # ✅ ADD THIS BACK
    parser.add_argument('--expected', type=int, default=None,
                        help='Expected XOR value to compare (use with --use-existing; skips Python --verified matmul)')

    args = parser.parse_args()

    # ✅ Optional: avoid ambiguity
    if args.verified and args.expected is not None:
        parser.error("Use either --verified OR --expected, not both.")

    input_file = args.input

    # Mode 1: use existing file
    if args.use_existing:
        if args.verified:
            n, A, B = load_from_file(input_file)
            expected_val = calc_expected_xor(A, B)
            run_c_program(input_file, args.threads, expected_result=expected_val)
        elif args.expected is not None:
            run_c_program(input_file, args.threads, expected_result=args.expected)
        else:
            run_c_program(input_file, args.threads, expected_result=None)
        return

    # Mode 2: generate + compute expected + write + verify
    if args.n is None:
        parser.error("You must provide -n unless you use --use-existing")

    global current_seed
    current_seed = args.seed
    np.random.seed(args.seed)
    if HAS_GPU:
        cp.random.seed(args.seed)
    print(f"[*] seed: {current_seed}")

    expected_val, A, B = generate_and_calc(args.n)
    write_to_file(input_file, args.n, A, B)
    run_c_program(input_file, args.threads, expected_result=expected_val)


if __name__ == "__main__":
    main()


# usage:
# 1) Just run C on existing file (no verification):
#    python3 test.py --use-existing --input matrix_input.txt -t 8
#
# 2) Verify existing file using Python (what you want):
#    python3 test.py --use-existing --input matrix_input.txt -t 8 --verified
#
# 3) Generate new random matrices and verify:
#    python3 test.py -n 64 -t 8
# 
# 4) python3 test.py --use-existing --input 5000_matrix_input.txt -t 8 --expected 164552927
