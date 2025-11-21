#!/usr/bin/env python3

import json
import os
import subprocess
import sys
import time
from subprocess import TimeoutExpired

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    MAGENTA = '\033[95m'
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'

def check_executable(executable="main"):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    exec_path = os.path.join(script_dir, executable)
    
    if not os.path.exists(exec_path):
        print(f"{Colors.RED}錯誤：找不到執行檔 {executable}{Colors.RESET}")
        print(f"{Colors.YELLOW}請先執行 'make' 編譯 main.cpp 產生執行檔{Colors.RESET}")
        return False
    
    if not os.access(exec_path, os.X_OK):
        print(f"{Colors.YELLOW}警告：{executable} 沒有執行權限，嘗試加上執行權限...{Colors.RESET}")
        os.chmod(exec_path, 0o755)
    
    return True

def run_test(input_file, output_file, testcase_dir, time_limit, executable="main"):
    input_path = os.path.join(testcase_dir, input_file)
    expected_output_path = os.path.join(testcase_dir, output_file)
    
    if not os.path.exists(input_path):
        return False, f"輸入檔案不存在：{input_path}", None, 0
    
    if not os.path.exists(expected_output_path):
        return False, f"預期輸出檔案不存在：{expected_output_path}", None, 0
    
    with open(expected_output_path, 'r', encoding='utf-8') as f:
        expected_output = f.read().strip()
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    exec_path = os.path.join(script_dir, executable)
    
    start_time = time.time()
    try:
        result = subprocess.run(
            ["mpirun", "-np", "4", exec_path],
            input=os.path.abspath(input_path),
            capture_output=True,
            text=True,
            timeout=time_limit,
            cwd=script_dir
        )
        elapsed_time = time.time() - start_time
        
        if result.returncode != 0:
            return False, f"執行失敗：\n{result.stderr}", None, elapsed_time
        
        actual_output = result.stdout.strip()
        
        if actual_output == expected_output:
            return True, None, actual_output, elapsed_time
        else:
            return False, f"輸出不符\n預期：{expected_output}\n實際：{actual_output}", actual_output, elapsed_time
    except TimeoutExpired:
        elapsed_time = time.time() - start_time
        return False, f"執行超時（超過 {time_limit} 秒）", None, elapsed_time

def score():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    testcase_dir = os.path.join(script_dir, "testcase")
    json_path = os.path.join(testcase_dir, "testing.json")
    
    if not os.path.exists(json_path):
        print(f"{Colors.RED}錯誤：找不到 {json_path}{Colors.RESET}")
        return False
    
    with open(json_path, 'r', encoding='utf-8') as f:
        config = json.load(f)
    
    total_score = config.get("score", 100)
    time_limit = config.get("time_limit", 1)
    testcases = config["testcase"]
    
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*50}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}          評分開始{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*50}{Colors.RESET}")
    print(f"{Colors.DIM}總分：{Colors.RESET}{Colors.BOLD}{total_score}{Colors.RESET}")
    print(f"{Colors.DIM}時間限制：{Colors.RESET}{Colors.BOLD}{time_limit} 秒{Colors.RESET}")
    print(f"{Colors.DIM}測試資料數：{Colors.RESET}{Colors.BOLD}{len(testcases)}{Colors.RESET}\n")
    
    if not check_executable("main"):
        return False
    
    print(f"{Colors.GREEN}✓{Colors.RESET} {Colors.DIM}找到執行檔 main{Colors.RESET}\n")
    print(f"{Colors.BOLD}{Colors.BLUE}{'─'*50}{Colors.RESET}\n")
    
    passed = 0
    failed = 0
    score_per_test = total_score / len(testcases) if testcases else 0
    earned_score = 0
    
    for i, testcase in enumerate(testcases, 1):
        input_file = testcase["input"]
        output_file = testcase["output"]
        
        test_name = os.path.basename(input_file)
        print(f"{Colors.BOLD}[{i}/{len(testcases)}]{Colors.RESET} {Colors.CYAN}{test_name}{Colors.RESET}", end=" ... ")
        
        success, error_msg, actual_output, elapsed_time = run_test(
            input_file, output_file, testcase_dir, time_limit, "main"
        )
        
        if success:
            passed += 1
            earned_score += score_per_test
            time_color = Colors.GREEN if elapsed_time < time_limit * 0.5 else Colors.YELLOW
            print(f"{Colors.GREEN}✓ PASS{Colors.RESET} {Colors.DIM}({elapsed_time:.3f}s){Colors.RESET}")
        else:
            failed += 1
            print(f"{Colors.RED}✗ FAIL{Colors.RESET}")
            if error_msg:
                if "執行超時" in error_msg:
                    print(f"  {Colors.RED}●{Colors.RESET} {Colors.RED}{error_msg}{Colors.RESET}")
                else:
                    print(f"  {Colors.YELLOW}●{Colors.RESET} {Colors.YELLOW}{error_msg}{Colors.RESET}")
            if elapsed_time > 0:
                print(f"  {Colors.DIM}執行時間：{elapsed_time:.3f}s{Colors.RESET}")
        print()
    
    print(f"{Colors.BOLD}{Colors.CYAN}{'─'*50}{Colors.RESET}\n")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*50}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}          評分結果{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*50}{Colors.RESET}")
    
    pass_rate = (passed / len(testcases) * 100) if testcases else 0
    status_color = Colors.GREEN if failed == 0 else Colors.RED
    
    print(f"\n{Colors.BOLD}通過：{Colors.RESET}{Colors.GREEN}{passed}{Colors.RESET} / {Colors.DIM}{len(testcases)}{Colors.RESET} {Colors.DIM}({pass_rate:.1f}%){Colors.RESET}")
    print(f"{Colors.BOLD}失敗：{Colors.RESET}{Colors.RED}{failed}{Colors.RESET} / {Colors.DIM}{len(testcases)}{Colors.RESET}")
    print(f"\n{Colors.BOLD}得分：{Colors.RESET}{status_color}{earned_score:.1f}{Colors.RESET} {Colors.DIM}/ {total_score}{Colors.RESET}")
    
    if failed == 0:
        print(f"\n{Colors.GREEN}{Colors.BOLD}🎉 全部通過！{Colors.RESET}")
    else:
        print(f"\n{Colors.RED}{Colors.BOLD}❌ 仍有 {failed} 個測試未通過{Colors.RESET}")
    
    print()
    
    return failed == 0

if __name__ == "__main__":
    success = score()
    sys.exit(0 if success else 1)
