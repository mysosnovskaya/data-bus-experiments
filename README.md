# Description

The program runs 8 jobs in all possible modes (maximum 4 jobs in a mode), each mode is executed 30 times. The information about duration of each iteration of each mode and some statisctis information are stored in the output files.

# Get started

## Environment requirements

Please, make sure the next tools are installed:
1. dpcpp compiler
2. Intel Math Kernel Library
3. Utilities lscpu and numactl

## Run

Before running, please, do the next steps:
1. Disable Turbo Boost and Hyper-Threading
2. Fix the frequency of the processor

To compile and run the program execute two scripts:
```bash
./compile.sh
./run.sh
```

## Results

All the results (an output of the lscpu utility and result files of the program) are stored in `results` directory.
