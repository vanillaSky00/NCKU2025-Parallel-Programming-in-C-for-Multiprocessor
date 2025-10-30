```
argv[1]—the file content arrives through std::cin
```

```
./program < "$input_file"
```
Means replace your program’s stdin with the bytes inside 001.in.


| You want to…                            | How to run                      | How to read in C++ (rank 0)               |
| --------------------------------------- | ------------------------------- | ----------------------------------------- |
| Read test data from stdin (judge style) | `mpiexec -n 4 ./hw3 < input.in` | `int n,t; cin >> n >> t; …`               |
| Read from a filename argument           | `mpiexec -n 4 ./hw3 input.in`   | `ifstream fin(argv[1]); fin >> n >> t; …` |
| Support both                            | Either of the above             |                                           |
