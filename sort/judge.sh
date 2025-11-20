for idx in {001..006}; do
    infile="./data/input/filename/${idx}.in"
    if [ -f "$infile" ]; then
        echo "===== Test $idx ====="
        mpiexec -n 4 ./counting < "$infile"
        echo -e "\n"  # Print an empty line
    fi
done
