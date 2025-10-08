#!/bin/bash

YELLOW='\033[1;33m'
BLUE='\033[0;34m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <test_cases_file>${NC}"
    exit 1
fi

test_cases_file=$1
if [ ! -f "$test_cases_file" ]; then
    echo -e "${RED}Error: Test file '$test_cases_file' not found.${NC}"
    exit 1
fi

echo -e "${YELLOW}==========================================${NC}"
echo -e "${YELLOW}        Problem A Test Results           ${NC}"
echo -e "${YELLOW}        (Divisor Sum Calculation)        ${NC}"
echo -e "${YELLOW}        Efficiency-Based Scoring         ${NC}"
echo -e "${YELLOW}==========================================${NC}"
echo ""


total_score=0
test_count=0
correct_count=0

if [ ! -f "./main" ]; then
    echo -e "${RED}Error: Sequential version 'main' not found. Please compile first.${NC}"
    exit 1
fi

if [ ! -f "./main_mpi" ]; then
    echo -e "${RED}Error: MPI version 'main_mpi' not found. Please compile first.${NC}"
    exit 1
fi

while IFS= read -r line; do

    if [[ -z "$line" || "$line" =~ ^# ]]; then
        continue
    fi

    min_n=$(echo "$line" | grep -oP '(?<=-min=)\d+')
    max_n=$(echo "$line" | grep -oP '(?<=-max=)\d+')

    if [ -z "$min_n" ] || [ -z "$max_n" ]; then
        echo -e "${YELLOW}Warning: Skipping invalid line format: $line${NC}"
        echo -e "${YELLOW}Expected format: -min=<min_value> -max=<max_value>${NC}"
        continue
    fi

    test_file="data/input/auto_test_${min_n}_${max_n}.tmp"
    MPI_OUTPUT="mpi_output.tmp"
    SEQ_OUTPUT="seq_output.tmp"

    echo -e "${CYAN}Generating test data for range [$min_n, $max_n]...${NC}"

    if ! python3 generate_tests.py --min $min_n --max $max_n --output "auto_test_${min_n}_${max_n}.tmp" --seed 42 > /dev/null 2>&1; then
        echo -e "${RED}Error: Failed to generate test data for range [$min_n, $max_n]${NC}"
        continue
    fi

    if [ ! -f "$test_file" ]; then
        echo -e "${RED}Error: Test file $test_file was not created${NC}"
        continue
    fi

    echo -e "${CYAN}Running MPI version (3 runs for accuracy)...${NC}"
    mpi_times=()
    for run in {1..3}; do
        echo -n "  Run $run: "
        for i in {1..15}; do
            echo -n "."
            sleep 0.05
        done
        echo -n " "

        sync
        sleep 0.5

        start_time_mpi=$(date +%s.%N)
        echo "$test_file" | mpirun -np 4 ./main_mpi > "$MPI_OUTPUT" 2>&1
        end_time_mpi=$(date +%s.%N)
        run_time=$(echo "$end_time_mpi - $start_time_mpi" | bc -l)
        mpi_times+=($run_time)
        echo -e "${GREEN}${run_time}s${NC}"
    done

    mpi_time=$(echo "scale=4; (${mpi_times[0]} + ${mpi_times[1]} + ${mpi_times[2]}) / 3" | bc -l)

    echo -e "${CYAN}Running Sequential version (3 runs for accuracy)...${NC}"
    seq_times=()
    for run in {1..3}; do
        echo -n "  Run $run: "
        for i in {1..15}; do
            echo -n "."
            sleep 0.05
        done
        echo -n " "

        sync
        sleep 0.5

        start_time_seq=$(date +%s.%N)
        echo "$test_file" | ./main > "$SEQ_OUTPUT" 2>&1
        end_time_seq=$(date +%s.%N)
        run_time=$(echo "$end_time_seq - $start_time_seq" | bc -l)
        seq_times+=($run_time)
        echo -e "${GREEN}${run_time}s${NC}"
    done

    seq_time=$(echo "scale=4; (${seq_times[0]} + ${seq_times[1]} + ${seq_times[2]}) / 3" | bc -l)

    if diff -q "$MPI_OUTPUT" "$SEQ_OUTPUT" > /dev/null 2>&1; then
        correctness_status="${GREEN}Correct ✅${NC}"
        is_correct=true
    else
        correctness_status="${RED}Incorrect ❌${NC}"
        is_correct=false
        echo -e "${RED}Sequential output:${NC}"
        cat "$SEQ_OUTPUT"
        echo -e "${RED}MPI output:${NC}"
        cat "$MPI_OUTPUT"
    fi


    if [ "$is_correct" = true ] && (( $(echo "$mpi_time > 0" | bc -l) )); then
        speedup=$(echo "scale=4; $seq_time / $mpi_time" | bc -l)

        P=4
        k=1.25

        efficiency=$(echo "scale=4; $speedup / $P" | bc -l)

        raw_score=$(echo "scale=4; $efficiency * 100 * $k" | bc -l)
        score=$(echo "scale=2; if ($raw_score > 100) 100 else $raw_score" | bc -l)

        efficiency_percent=$(echo "scale=1; $efficiency * 100" | bc -l)
    else
        speedup="N/A"
        efficiency_percent="N/A"
        score=0
    fi


    echo -e "${BLUE}┌─────────────────────────────────────────────────────────┐${NC}"
    echo -e "${BLUE}│${NC} ${CYAN}Test Case:${NC} ${YELLOW}Range [$min_n, $max_n]${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}├─────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}Sequential Time:${NC} ${YELLOW}${seq_time}s${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}MPI Time:${NC}       ${YELLOW}${mpi_time}s${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}Speedup:${NC}        ${YELLOW}${speedup}x${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}Efficiency:${NC}     ${YELLOW}${efficiency_percent}%${NC} ${BLUE}│${NC}"

    if (( $(echo "$score >= 80" | bc -l) )); then
        score_color="${GREEN}"
    elif (( $(echo "$score >= 60" | bc -l) )); then
        score_color="${YELLOW}"
    else
        score_color="${RED}"
    fi

    echo -e "${BLUE}│${NC} ${GREEN}Score:${NC}          ${score_color}${score}分${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}Result:${NC}         $correctness_status ${BLUE}│${NC}"
    echo -e "${BLUE}└─────────────────────────────────────────────────────────┘${NC}"
    echo ""

    total_score=$(echo "scale=2; $total_score + $score" | bc -l)
    test_count=$((test_count + 1))
    if [ "$is_correct" = true ]; then
        correct_count=$((correct_count + 1))
    fi


    rm -f "$test_file" "$MPI_OUTPUT" "$SEQ_OUTPUT"

    sleep 1

done < "$test_cases_file"


if [ $test_count -gt 0 ]; then
    average_score=$(echo "scale=2; $total_score / $test_count" | bc -l)
else
    average_score=0
fi

echo -e "${YELLOW}==========================================${NC}"
echo -e "${GREEN}All tests completed!${NC}"
echo -e "${YELLOW}==========================================${NC}"
