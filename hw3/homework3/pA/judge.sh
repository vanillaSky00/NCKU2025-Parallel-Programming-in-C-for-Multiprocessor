#!/bin/bash

# Constants
EXECUTABLE="./hw3a"                 # 執行檔案名稱
INPUT_PATH="./data/input/filename"   # 測資輸入路徑
OUTPUT_PATH="./data/answer"         # 正確輸出路徑
TIMEOUT=3                          # 最大執行時間上限（秒）
ABS_EPS=1e-6                        # 絕對誤差閾值
REL_EPS=1e-6                        # 相對誤差閾值
MPI_PROCESSES=4                     # Number of MPI processes

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable $EXECUTABLE not found"
    exit 1
fi

# Initialize maximum execution time
max_time=0

echo "==============================="
echo "Running tests for $EXECUTABLE"
echo "==============================="

# Function to compare outputs with floating-point tolerance
compare_outputs() {
    local actual_file="$1"
    local expected_file="$2"
    local test_num="$3"

    if [ ! -f "$actual_file" ] || [ ! -f "$expected_file" ]; then
        echo "Error: One or both files not found for comparison"
        return 1
    fi

    awk -v abs_eps="$ABS_EPS" -v rel_eps="$REL_EPS" -v test_num="$test_num" '
    # Use FNR for the first file and NR for the second
    FNR == NR {
        # Store expected lines from the first file (expected_file)
        expected_lines[FNR] = $0
        next
    }
    
    # Process actual lines from the second file (actual_file)
    {
        line_num = FNR
        expected_line = expected_lines[line_num]

        # Handle different number of lines
        if (expected_line == "") {
            print "Wrong Answer on Test #" test_num ": Actual output has more lines than expected."
            exit 1
        }
        
        # Trim leading/trailing whitespace before splitting
        gsub(/^[ \t]+|[ \t]+$/, "", $0)
        gsub(/^[ \t]+|[ \t]+$/, "", expected_line)
        
        # Split tokens
        split($0, actual_tokens, /[ \t]+/)
        split(expected_line, exp_tokens, /[ \t]+/)
        
        if (length(actual_tokens) != length(exp_tokens)) {
            print "Wrong Answer on Test #" test_num ": Different number of tokens on line " line_num
            print "  Actual tokens: " length(actual_tokens)
            print "  Expected tokens: " length(exp_tokens)
            exit 1
        }
        
        for (i = 1; i <= length(actual_tokens); i++) {
            actual_str = actual_tokens[i]
            exp_str = exp_tokens[i]
            
            if (actual_str == exp_str) continue
            
            if (actual_str ~ /^-?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?$/ &&
                exp_str ~ /^-?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?$/) {
                
                a = actual_str + 0
                b = exp_str + 0
                
                abs_diff = (a > b ? a - b : b - a)
                
                if (abs_diff > abs_eps) {
                    abs_a = (a >= 0 ? a : -a)
                    abs_b = (b >= 0 ? b : -b)
                    max_abs = (abs_a > abs_b ? abs_a : abs_b)
                    rel_diff = (max_abs == 0 ? 0 : abs_diff / max_abs)
                    
                    if (rel_diff > rel_eps) {
                        print "Wrong Answer on Test #" test_num ", line " line_num ", token " i ": Values differ beyond tolerance."
                        print "  Actual: " actual_str " (=" a ")"
                        print "  Expected: " exp_str " (=" b ")"
                        print "  Abs diff: " abs_diff " (threshold: " abs_eps ")"
                        print "  Rel diff: " rel_diff " (threshold: " rel_eps ")"
                        exit 1
                    }
                }
            } else {
                print "Wrong Answer on Test #" test_num ", line " line_num ", token " i ": Exact mismatch for non-numeric values."
                print "  Actual: " actual_str
                print "  Expected: " exp_str
                exit 1
            }
        }
    }
    END {
        # Check if actual output has fewer lines than expected
        if (NR < length(expected_lines)) {
            print "Wrong Answer on Test #" test_num ": Actual output has fewer lines than expected."
            exit 1
        }
    }' "$expected_file" "$actual_file"
    return $?
}

input_files=("$INPUT_PATH"/*.in)
if [ ${#input_files[@]} -eq 0 ] || [ ! -f "${input_files[0]}" ]; then
    echo "Error: No input files found in $INPUT_PATH"
    exit 1
fi

for input_file in "${input_files[@]}"; do
    # Extract test case number from filename (e.g., 001.in -> 001)
    test_num=$(basename "$input_file" .in)
    output_file="$OUTPUT_PATH/$test_num.out"
    
    # Check if corresponding output file exists
    if [ ! -f "$output_file" ]; then
        echo "Error: Output file $output_file not found"
        # The script stops here, which is why you see the error message.
        exit 1
    fi
    
    echo "Running Test #$test_num..."

    # Run the executable with timeout and measure execution time
    start_time=$(date +%s%N)
    timeout "$TIMEOUT"s mpiexec -n "$MPI_PROCESSES" "$EXECUTABLE" < "$input_file" > temp.out
    # timeout "$TIMEOUT"s "$EXECUTABLE" < "$input_file" > temp.out
    status=$?
    end_time=$(date +%s%N)
    
    # Calculate execution time in seconds
    exec_time=$(echo "scale=6; ($end_time - $start_time) / 1000000000" | bc -l)
    
    # Update maximum execution time
    if [ "$(echo "$exec_time > $max_time" | bc -l)" -eq 1 ]; then
        max_time=$exec_time
    fi
    
    if [ $status -ne 0 ] && [ $status -ne 124 ]; then
        echo "Runtime error on Test #$test_num. Command exited with status $status."
        rm -f temp.out
        exit 1

    # Check if timeout occurred
    elif [ $status -eq 124 ]; then
        echo "Time Limit Exceeded on Test #$test_num"
        rm -f temp.out
        exit 1
    fi
    
    # Compare output with expected output using floating-point tolerance
    if ! compare_outputs temp.out "$output_file" "$test_num"; then
        rm -f temp.out
        exit 1
    fi
    echo "Test #$test_num passed in $exec_time seconds."
done

# Clean up temporary output file
rm -f temp.out

# Check if maximum execution time exceeds timeout
if [ "$(echo "$max_time > $TIMEOUT" | bc -l)" -eq 1 ]; then
    echo "Time Limit Exceeded"
    exit 1
fi

echo "All tests passed."
# Calculate and display the ratio of max_time to TIMEOUT
ratio=$(echo "scale=6; $max_time / $TIMEOUT" | bc -l)
echo "Maximum execution time: $max_time seconds"
echo "Ratio to timeout: $ratio"
