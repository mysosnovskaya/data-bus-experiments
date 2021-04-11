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

### Input and output format

Input: none

Output: 

    file names: "data_2j__20_XPY_1000_QR.txt", where 2j means "two jobs", "20_XPY_1000_QR" - job ids.
    file format (job id, iteration durations and statistic information for each job):
        Job 20_XPY
        1428.44,1428.13,1435.44,1420.89,1432.17,1422.08,1432.41,1432.61,1422.25,1429.22,1421.45,1426.09,1424.25,1423.11,1432.59,1423.76,1430.81,1431.41,1427.26,1432.18,1420.07,1432.67,1423.81,1428.37,1430.9,1422.08,1429.09,1424.98,1426.7,1425.47
        Expected value: 1427.36
        Dispersion: 18.228
        Standard Deviation: 4.34241
        Minimum value: 1420.07
        Maximum value: 1435.44

        Job 1000_QR
        2695.16,2694.58,2708.38,2680.92,2702.2,2683.17,2702.66,2703.04,2683.49,2696.65,2681.99,2690.74,2687.26,2685.11,2703,2686.35,2699.64,2700.78,2692.94,2702.23,2679.38,2703.15,2686.44,2695.03,2699.82,2683.16,2696.4,2688.65,2691.88,2689.56
        Expected value: 2693.13
        Dispersion: 64.8914
        Standard Deviation: 8.19323
        Minimum value: 2679.38
        Maximum value: 2708.38
    

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

### Input and output format

Input: none

Output:

    file names: "tbb_output_2j_NO_ORDER_20_XPY_1000_QR.txt", where 2j means "two jobs", "NO_ORDER" means the type of partial order, "20_XPY_1000_QR" - job ids.
    file format (job ids, iteration durations and order table for each set of jobs):
        Jobs: 20_XPY  1000_QR
        
        1428.44,1428.13,1435.44,1420.89,1432.17,1422.08,1432.41,1432.61,1422.25,1429.22,1421.45,1426.09,1424.25,1423.11,1432.59,1423.76,1430.81,1431.41,1427.26,1432.18,1420.07,1432.67,1423.81,1428.37,1430.9,1422.08,1429.09,1424.98,1426.7,1425.47

        order table:
        0 0
        0 1


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

### Input and output format

Input: none

Output:
    
    vtune results and custom file with results of manual calculations.

    file name: "bus_percents_results.txt"
    file format (job id, standard time of execution and calculated data bus consumption):
        20_XPY   3142   59.0935
        1000_QR   3119   59.9232
        10_COPY   3168   66.4569
