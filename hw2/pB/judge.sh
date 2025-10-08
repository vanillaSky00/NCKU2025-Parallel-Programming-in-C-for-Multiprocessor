#!/bin/bash

YELLOW='\033[1;33m'
BLUE='\033[0;34m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m' # No Color

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
echo -e "${YELLOW}        Problem B Test Results         ${NC}"
echo -e "${YELLOW}==========================================${NC}"
echo ""


while IFS= read -r line; do

    if [[ -z "$line" || "$line" =~ ^# ]]; then
        continue
    fi


    n=$(echo "$line" | grep -oP '(?<=-n=)\d+')
    m=$(echo "$line" | grep -oP '(?<=-m=)\d+')

    if [ -z "$n" ] || [ -z "$m" ]; then
        echo -e "${YELLOW}Warning: Skipping invalid line format: $line${NC}"
        continue
    fi


    test_file="data/input/auto_test_${n}_${m}.tmp"
    MPI_OUTPUT="mpi_output.tmp"
    SEQ_OUTPUT="seq_output.tmp"

    # Generate test data with animation
    echo -e "${CYAN}Generating test data for n=$n, m=$m...${NC}"


    if ! python3 generate_tests.py --n $n --m $m --output "auto_test_${n}_${m}.tmp" > /dev/null 2>&1; then
        echo -e "${RED}Error: Failed to generate test data for n=$n, m=$m${NC}"
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
        echo "$test_file" | ./main2 > "$SEQ_OUTPUT" 2>&1
        end_time_seq=$(date +%s.%N)
        run_time=$(echo "$end_time_seq - $start_time_seq" | bc -l)
        seq_times+=($run_time)
        echo -e "${GREEN}${run_time}s${NC}"
    done

    seq_time=$(echo "scale=4; (${seq_times[0]} + ${seq_times[1]} + ${seq_times[2]}) / 3" | bc -l)

    if diff -q "$MPI_OUTPUT" "$SEQ_OUTPUT" > /dev/null 2>&1; then
        correctness_status="${GREEN}Correct ✅${NC}"
    else
        correctness_status="${RED}Incorrect ❌${NC}"
    fi

    echo -e "${BLUE}┌─────────────────────────────────────────────────────────┐${NC}"
    echo -e "${BLUE}│${NC} ${CYAN}Test Case:${NC} ${YELLOW}n=$n, m=$m${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}├─────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}Sequential Time:${NC} ${YELLOW}${seq_time}s${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}MPI Time:${NC}       ${YELLOW}${mpi_time}s${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}Speedup:${NC}        ${YELLOW}$(echo "scale=2; $seq_time / $mpi_time" | bc -l)x${NC} ${BLUE}│${NC}"
    echo -e "${BLUE}│${NC} ${GREEN}Result:${NC}         $correctness_status ${BLUE}│${NC}"
    echo -e "${BLUE}└─────────────────────────────────────────────────────────┘${NC}"
    echo ""

    # Clean up all temporary files for this run
    rm -f "$test_file" "$MPI_OUTPUT" "$SEQ_OUTPUT"

    sleep 1

done < "$test_cases_file"

# --- 4. Print Footer ---
echo -e "${YELLOW}==========================================${NC}"
echo -e "${GREEN}All tests completed!${NC}"
echo -e "${YELLOW}==========================================${NC}"
