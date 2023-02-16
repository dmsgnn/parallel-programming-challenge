# __Parallel Programming challenge__

This project has been developed for the "Advanced Algorithms and Parallel Programming" course, attended during my Master of Science (A.Y. 2022/23) at the Polytechnic University of Milan. The highest possible final grade has been achieved: 4.0/4.0.

## Description

Aim of this project was to implement a parallel version of the grep function. The algorithm has been implemented in C++ using Message Passing Interface (MPI).

## Compilation and execution

The algorithm can be compiled using the following command. 

```shell
mpicxx grep-main.cpp grep.cpp -o grep
```

Then, it can be executed, using the provided [input file](/input_file.txt), in the following way

```shell
mpiexec --allow-run-as-root -np 4 ./grep word input_file.txt
```

where `word` is an arbitrary word to look for in the document and `4` is the number of processes the algorithm will be executed by, it can be arbitrarily set.

The output of the parallel grep implementation is printed in the [program result](/program_result.txt) file. An example file is provided, created using the `word` word.

Finally, the correctness of the algorithm's output can be verified by using the real grep implementation and the diff command to compare the two results.

```shell
grep -n word input_file.txt &> correct_result.txt
diff correct_result.txt program_result.txt
```
