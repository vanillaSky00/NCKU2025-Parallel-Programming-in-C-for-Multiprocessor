#!/bin/bash

# ==============================
#  Simple Judge for MPI Program
# ==============================

# Config
EXECUTABLE="./counting"          # your compiled MPI program
INPUT_PATH="./data/input"        # folder containing *.in
OUTPUT_PATH="./data/answer"      # folder containing *.out
TIMEOUT=5                        # per-test time limit (seconds)
MPI_PROCESSES=4                  # number of MPI processes

# ------------- sanity checks -------------

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable $EXECUTABLE not found"
    exit 1
fi

if [ ! -d "$INPUT_PATH" ]; then
    echo "Error: Input directory $INPUT_PATH not found"
    exit 1
fi

if [ ! -d "$OUTPUT_PATH" ]; then
    echo "Error: Output directory $OUTPUT_PATH not found"
    exit 1
fi

# Collect input files
shopt -s nullglob
input_files=("$INPUT_PATH"/*.in)
shopt -u nullglob

if [ ${#input_files[@]} -eq 0 ]; then
    echo "Error: No .in files found in $INPUT_PATH"
    exit 1
fi

# ------------- helper: compare outputs -------------

compare_outputs() {
    local file1=$1
    local file2=$2
    local test_num=$3

    # Normalize whitespace and compare
    # -s: squeeze spaces, remove leading/trailing
    # This avoids WA just because of extra spaces/newlines.
    sed 's/[[:space:]]\+/ /g; s/^ //; s/ $//' "$file1" > .tmp_user_norm
    sed 's/[[:space:]]\+/ /g; s/^ //; s/ $//' "$file2" > .tmp_ans_norm

    if diff -q .tmp_user_norm .tmp_ans_norm > /dev/null; then
        rm -f .tmp_user_norm .tmp_ans_norm
        return 0
    else
        echo "Wrong Answer on Test #$test_num"
        echo "Differences (normalized):"
        diff .tmp_user_norm .tmp_ans_norm
        rm -f .tmp_user_norm .tmp_ans_norm
        return 1
    fi
}

# ------------- main loop -------------

max_time=0

echo "==============================="
echo "Running tests for $EXECUTABLE"
echo "MPI processes: $MPI_PROCESSES"
echo "Timeout per test: ${TIMEOUT}s"
echo "==============================="

for input_file in "${input_files[@]}"; do
    test_num=$(basename "$input_file" .in)
    output_file="$OUTPUT_PATH/$test_num.out"

    if [ ! -f "$output_file" ]; then
        echo "Error: Answer file $output_file not found"
        exit 1
    fi

    echo "Running Test #$test_num ..."

    # Time measurement (nanoseconds → seconds)
    start_time=$(date +%s%N)

    # Run program under timeout
    timeout "$TIMEOUT"s mpiexec -n "$MPI_PROCESSES" "$EXECUTABLE" < "$input_file" > temp.out
    status=$?

    end_time=$(date +%s%N)
    exec_time=$(echo "scale=6; ($end_time - $start_time) / 1000000000" | bc -l)

    # Track max execution time
    if [ "$(echo "$exec_time > $max_time" | bc -l)" -eq 1 ]; then
        max_time=$exec_time
    fi

    # Handle status
    if [ $status -eq 124 ]; then
        echo "Time Limit Exceeded on Test #$test_num"
        rm -f temp.out
        exit 1
    elif [ $status -ne 0 ]; then
        echo "Runtime Error on Test #$test_num (exit code $status)"
        rm -f temp.out
        exit 1
    fi

    # Compare outputs
    if ! compare_outputs temp.out "$output_file" "$test_num"; then
        rm -f temp.out
        exit 1
    fi

    echo "Test #$test_num PASSED in ${exec_time}s"
    echo "--------------------------------"
done

rm -f temp.out

# Final report
ratio=$(echo "scale=6; $max_time / $TIMEOUT" | bc -l)
echo "All tests PASSED."
echo "Max execution time: $max_time s"
echo "Max / TIMEOUT ratio: $ratio"