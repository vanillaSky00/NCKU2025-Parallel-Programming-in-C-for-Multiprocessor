#!/bin/bash

# Constants
EXECUTABLE="./hw3b"                 # 執行檔案名稱
INPUT_PATH="./data/input/filename"   # 測資輸入路徑
OUTPUT_PATH="./data/answer"         # 正確輸出路徑
TIMEOUT=5                          # 最大執行時間上限（秒）
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
    
    # Compare output with expected output using diff
    compare_outputs() {
        local file1=$1
        local file2=$2
        local test_num=$3
        if diff -q "$file1" "$file2" > /dev/null; then
            return 0
        else
            echo "Wrong Answer on Test #$test_num"
            echo "Differences:"
            diff "$file1" "$file2"
            return 1
        fi
    }
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
