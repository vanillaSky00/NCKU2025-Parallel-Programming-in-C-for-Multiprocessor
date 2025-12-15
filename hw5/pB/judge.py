import os
import sys
import subprocess
import time
import statistics
from enum import IntEnum
from typing import Tuple, List, Optional
import argparse

# ==================== 設定區 ====================
threads = 4
RUNS = 5
WARMUP = 3
# ================================================

class Result(IntEnum):
    AC  = 42; WA = 43; RE = 44; TLE = 45; NO = 46; CE = 47

RESULT_NAME = {
    Result.AC:  "Accepted",
    Result.WA:  "Wrong Answer",
    Result.RE:  "Runtime Error",
    Result.TLE: "Time Limit Exceeded",
    Result.NO:  "No Output",
    Result.CE:  "Compilation Error",
}

def die(msg: str):
    print(f"\n[ERROR] {msg}")
    sys.exit(1)

def compile_program() -> None:
    print("Compiling...", end=" ")
    subprocess.run("make clean", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ret = subprocess.run("make", shell=True, capture_output=True, text=True)
    if ret.returncode != 0:
        print("Failed")
        if ret.stderr: print(ret.stderr.strip())
        die("Compilation failed")
    print("OK")

def run_once(cmd: str, time_limit: float) -> Tuple[Result, str, float]:
    """單次執行，回傳 (結果, 輸出內容, 執行時間秒)"""
    try:
        start = time.perf_counter()
        proc = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=time_limit
        )
        elapsed = time.perf_counter() - start

        if proc.returncode != 0:
            return Result.RE, proc.stderr.strip() or proc.stdout.strip(), elapsed
        if not proc.stdout.strip():
            return Result.NO, "Empty output", elapsed

        return Result.AC, proc.stdout, elapsed

    except subprocess.TimeoutExpired:
        return Result.TLE, "Time Limit Exceeded", time_limit
    except Exception as e:
        return Result.RE, f"Exception: {e}", 0.0

def benchmark_case(input_path: str, executable: str, time_limit: float) -> Tuple[Result, str, List[float]]:
    cmd = f"./{executable} {threads} < {input_path}"

    times: List[float] = []

    # Warmup
    for _ in range(WARMUP):
        res, out, t = run_once(cmd, time_limit)
        if res != Result.AC:
            return res, out or RESULT_NAME[res], []

    # 正式測量
    for _ in range(RUNS):
        res, out, t = run_once(cmd, time_limit)
        if res != Result.AC:
            return res, out or RESULT_NAME[res], []
        times.append(t)

    return Result.AC, out, times

def validate_output(
    team_output: str,
    ans_path: str,
    case_sensitive: bool = False,
    float_abs_tol: Optional[float] = None,
    float_rel_tol: Optional[float] = None,
) -> Tuple[Result, str]:
    if not team_output.strip():
        return Result.NO, "Empty output"

    use_float = float_abs_tol is not None or float_rel_tol is not None
    abs_tol = float_abs_tol or 0
    rel_tol = float_rel_tol or 0

    with open(ans_path, "r", encoding="utf-8") as f:
        judge_tokens = f.read().split()
    team_tokens = team_output.split()

    if len(team_tokens) != len(judge_tokens):
        return Result.WA, f"Token count mismatch: expected {len(judge_tokens)}, got {len(team_tokens)}"

    for i, (j_tok, t_tok) in enumerate(zip(judge_tokens, team_tokens), 1):
        if use_float:
            try:
                j_val = float(j_tok)
                t_val = float(t_tok)
                abs_diff = abs(t_val - j_val)
                rel_diff = abs_diff / (abs(j_val) + 1e-12)
                if abs_diff > abs_tol and rel_diff > rel_tol:
                    return Result.WA, (
                        f"Token {i} float error\n"
                        f"  Expected : {j_tok}\n"
                        f"  Got      : {t_tok}\n"
                        f"  Abs diff : {abs_diff:.10f}\n"
                        f"  Rel diff : {rel_diff:.10f}"
                    )
                continue
            except ValueError:
                pass

        if case_sensitive:
            if j_tok != t_tok:
                return Result.WA, f"Token {i} mismatch: '{j_tok}' vs '{t_tok}'"
        else:
            if j_tok.lower() != t_tok.lower():
                return Result.WA, f"Token {i} mismatch (ignore case): '{j_tok}' vs '{t_tok}'"

    return Result.AC, "Accepted"

def print_congratulations():
    print("\n" + "="*100)
    print("   ██████╗ ██████╗ ███╗   ██╗ ██████╗ ██████╗  █████╗ ████████╗███████╗".center(100))
    print("  ██╔════╝██╔═══██╗████╗  ██║██╔════╝ ██╔══██╗██╔══██╗╚══██╔══╝██╔════╝".center(100))
    print("  ██║     ██║   ██║██╔██╗ ██║██║  ███╗██████╔╝███████║   ██║   ███████╗".center(100))
    print("  ██║     ██║   ██║██║╚██╗██║██║   ██║██╔══██╗██╔══██║   ██║   ╚════██║".center(100))
    print("  ╚██████╗╚██████╔╝██║ ╚████║╚██████╔╝██║  ██║██║  ██║   ██║   ███████║".center(100))
    print("   ╚═════╝ ╚═════╝ ╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚══════╝".center(100))
    print("="*100 + "\n")

# ==================== 主程式 ====================
def main():
    global RUNS, WARMUP

    executable = "hw5B"
    parser = argparse.ArgumentParser()
    parser.add_argument("--type", type=int, default=1, help="1: TL=20s, 2: TL=6s")
    parser.add_argument("--runs", type=int, default=RUNS, help="測量次數")
    parser.add_argument("--warmup", type=int, default=WARMUP, help="暖機次數")
    args = parser.parse_args()


    RUNS = args.runs
    WARMUP = args.warmup

    time_limit = 10.0 if args.type == 1 else 5.0
    if args.type not in (1, 2):
        die("Invalid --type")

    input_folder = "data/input/filename/"
    answer_folder = "data/answer/"
    test_cases = [(f"{i:03d}.in", f"{i:03d}.out") for i in range(1, 11)]

    print(f"Time Limit: {time_limit}s  |  Threads: {threads}  |  Runs: {WARMUP} warmup + {RUNS} measured")
    compile_program()

    print("\n" + "═"*100)
    print(f"{'Case':<6} {'Status':<12} {'Time (ms)':<12} {'σ (ms)':<10} {'Min/Max':<14} {'vs TL'}")
    print("─"*100)

    all_means = []

    for in_file, ans_file in test_cases:
        in_path = os.path.join(input_folder, in_file)
        ans_path = os.path.join(answer_folder, ans_file)

        if not os.path.exists(in_path) or not os.path.exists(ans_path):
            die(f"Missing file: {in_file} or {ans_file}")

        result, output, times = benchmark_case(in_path, executable, time_limit)
        case_name = in_file[:3]

        if result != Result.AC:
            print(f"{case_name:<6} {RESULT_NAME[result]:<12} {'-'*12} {'-'*10} {'-'*14} {'-'*8}")
            if output:
                res = output.replace('\n', '\n   ')
                print(f"   {res}")
            sys.exit(1)

        val_res, val_msg = validate_output(output, ans_path)
        if val_res != Result.AC:
            print(f"{case_name:<6} Wrong Answer {'-'*12} {'-'*10} {'-'*14} {'-'*8}")
            res = val_msg.replace('\n', '\n   ')
            print(f"   {res}")
            sys.exit(1)

        mean_ms = statistics.mean(times) * 1000
        std_ms = statistics.stdev(times) * 1000 if len(times) > 1 else 0.0
        min_ms = min(times) * 1000
        max_ms = max(times) * 1000
        ratio = time_limit * 1000 / mean_ms

        all_means.append(mean_ms)

        status = "Accepted" if mean_ms < time_limit * 1000 * 0.9 else "Accepted (Close)"
        print(f"{case_name:<6} {status:<12} {mean_ms:8.2f}   {std_ms:6.2f}    {min_ms:5.1f}─{max_ms:5.1f}    {ratio:5.1f}x")

    print_congratulations()
    print(f"   最慢測資平均時間：{max(all_means):.2f} ms")
    print(f"   整體平均時間    ：{sum(all_means)/len(all_means):.2f} ms")
    print(f"   加速比      ：{time_limit * 1000 / (max(all_means)):.2f}x faster than TL\n")

if __name__ == "__main__":
    main()