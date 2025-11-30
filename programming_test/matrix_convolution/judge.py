import os
import sys
import subprocess
import time
from pathlib import Path
from enum import IntEnum
from typing import Tuple, Optional
import argparse

size = 4  # MPI processes

class Result(IntEnum):
    AC  = 42  # Accepted
    WA  = 43  # Wrong Answer
    RE  = 44  # Runtime Error
    TLE = 45  # Time Limit Exceeded
    NO  = 46  # No Output
    CE  = 47  # Compilation Error

RESULT_NAME = {
    Result.AC:  "Accepted",
    Result.WA:  "Wrong Answer",
    Result.RE:  "Runtime Error",
    Result.TLE: "Time Limit Exceeded",
    Result.NO:  "No Output",
    Result.CE:  "Compilation Error",
}

def die(msg: str):
    """印錯誤訊息後立刻結束程式"""
    print(f"\n[ERROR] {msg}")
    sys.exit(1)

def compile_program() -> None:
    """編譯，失敗就直接 die"""
    print("Compiling...", end=" ")
    subprocess.run("make clean", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ret = subprocess.run("make", shell=True, capture_output=True, text=True)
    if ret.returncode != 0:
        print("Compilation Error")
        if ret.stderr:
            print(ret.stderr.strip())
        die("Compilation failed")
    print("OK")

def run_with_time(input_path: str, executable: str, time_limit: float = 5.0) -> Tuple[Result, str, float]:
    if not os.path.exists(executable):
        die(f"Executable '{executable}' not found (maybe compilation failed?)")

    # 重要：用 shell 的 < redirection
    cmd = f"mpiexec -n {size} ./{executable} < {input_path}"

    try:
        start = time.perf_counter()

        proc = subprocess.run(
            cmd,
            shell=True,                 # 必須開啟
            capture_output=True,
            text=True,
            timeout=time_limit
        )

        elapsed = time.perf_counter() - start

        if proc.returncode != 0:
            return Result.RE, proc.stdout + "\n" + proc.stderr, elapsed

        return Result.AC, proc.stdout, elapsed

    except subprocess.TimeoutExpired:
        return Result.TLE, "", time_limit
    except Exception as e:
        return Result.RE, f"Run exception: {e}", 0.0

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
                pass  # 不是數字，當字串比

        if case_sensitive:
            if j_tok != t_tok:
                return Result.WA, f"Token {i} mismatch: '{j_tok}' vs '{t_tok}'"
        else:
            if j_tok.lower() != t_tok.lower():
                return Result.WA, f"Token {i} mismatch (ignore case): '{j_tok}' vs '{t_tok}'"

    return Result.AC, "Accepted"

# ==============================
# 主程式：只要改這裡的 test_cases 即可
# ==============================
def main():
    executable = "pA"           # make 產生的執行檔名稱
    time_limit = 5.0             # 秒

    parser = argparse.ArgumentParser()
    parser.add_argument("--type", type=int, default=1, help="1: TL=12, 2: TL=5")
    args = parser.parse_args()

    # type 決定 time limit
    if args.type == 1:
        time_limit = 12.0
    elif args.type == 2:
        time_limit = 5.0
    else:
        die("Invalid type (only 1 or 2 allowed)")

    input_folder = "data/input/filename/"
    answer_folder = "data/answer/"

    test_cases = [
        ("001.in", "c3b12005bea1be1b.out"),
        ("002.in", "54655959ceb940a2.out"),
        ("003.in", "765a5f3478fdc4d9.out"),
        ("004.in", "4039b4b1e8d1d2f9.out"),
        ("005.in", "d716cb9e9815f1cd.out"),
        ("006.in", "676642449b0aab7b.out"),
        ("007.in", "0e2977d6f24afeaf.out"),
        ("008.in", "eda28c4d81fe28b8.out"),
        ("009.in", "6be22f41855284b4.out"),
        ("010.in", "5570f8a2ff36c131.out"),
    ]

    compare_opts = {
        "case_sensitive": True,
        "float_abs_tol": None,
        "float_rel_tol": None,
    }

    print("Time Limit:", time_limit) 

    compile_program()

    print("\n" + "="*70)
    print(f"{'File':<25} {'Result':<15} {'Time':<10}")
    print("="*70)

    for in_file, ans_file in test_cases:
        in_path = os.path.join(input_folder, in_file)
        ans_path = os.path.join(answer_folder, ans_file)
        if not os.path.exists(in_path):
            die(f"Input file not found: {in_path}")
        if not os.path.exists(ans_path):
            die(f"Answer file not found: {ans_path}")

        result, output, runtime = run_with_time(in_path, executable, time_limit)

        # runtime display
        time_str = f"{runtime*1000:6.1f} ms" if runtime > 0 else "   ---   "

        if result != Result.AC:
            # 印基本資訊
            print(f"{Path(in_path).name:<25} {RESULT_NAME[result]:<15} {time_str:<10}")
            # 在下面印詳細錯誤（多一行）
            print("   " + output.strip().replace("\n", "\n   "))
            print("-"*70)
            sys.exit(1)

        if not output.strip():
            print(f"{Path(in_path).name:<25} No Output      {time_str:<10}")
            print("   (empty output)")
            print("-"*70)
            sys.exit(1)

        val_res, msg = validate_output(output, ans_path, **compare_opts)
        if val_res != Result.AC:
            print(f"{Path(in_path).name:<25} Wrong Answer   {time_str:<10}")
            print("   " + msg.replace("\n", "\n   "))
            print("-"*70)
            sys.exit(1)

        # 到這裡一定是 AC
        print(f"{Path(in_path).name:<25} Accepted       {time_str:<10}")


    print("\nAll test cases passed!")
    sys.exit(0)

if __name__ == "__main__":
    main()
