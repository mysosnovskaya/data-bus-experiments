# Get started

## Environment requirements

Please, make sure the next tools are installed:
1. dpcpp compiler
2. Intel Math Kernel Library (MKL)
3. Intel Threading Building Blocks (TBB) library
4. Utilities lscpu and numactl

Before running, please, do the next steps:
1. Disable Turbo Boost and Hyper-Threading
2. Fix the frequency of the processor


## Collect execution statistics

### Description

The program runs 8 jobs in all possible modes (maximum 4 jobs in a mode), each mode is executed 30 times.
The information about duration of each iteration of each mode and some statistics information are stored in the output files.

### Run
To compile and run the program go to `execution-statistics` directory and execute two scripts:
```bash
./compile.sh
./run.sh
```

### Results

All the results (an output of the lscpu utility and result files of the program) are stored in `execution-statistics/results` directory.


## Run tbb for generated set of jobs

### Description

The program generates sets of jobs (with different partial order types and different amount of jobs in the sets) and runs all jobs from each set with using tbb.
The output files contain information about duration of execution of all jobs from each set.

### Run
To compile and run the program go to `tbb-runner` directory and execute two scripts:
```bash
./compile.sh
./run.sh
```

### Results

All the results (result files of the program) are stored in `tbb-runner/results` directory.


## Measure the data bus bandwidth

### Description

Two programs are suggested for measuring the data bus consumption. 
Both programs run all jobs one by one and measures data bus consumption for each job,
but `BusBandwidthCollector.cpp` does it with using vtune and `CalculateBusPercent.cpp` - with using our custom algorithm. 

### Run
To compile and run the programs go to `bus-bandwidth-collector` directory and execute two scripts:
```bash
./compile.sh
./run.sh
```

### Results

All the results (result files of the programs) are stored in `bus-bandwidth-collector/results` directory.
