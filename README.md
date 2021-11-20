# Get started

## Environment requirements

Please, make sure the next tools are installed:
1. dpcpp or g++ compiler
2. Intel Math Kernel Library (MKL)
3. Intel Threading Building Blocks (TBB) library
4. Utilities lscpu and numactl

Before running, please, do the next steps:
1. Disable Turbo Boost and Hyper-Threading
2. Fix the frequency of the processor


## Generate examples

### Description

The program generates examples and has the following set of arguments: 
* --jobs-count is count of jobs in the generated examples (required),
* --examples-count is count of examples to generate (required),
* --partial-order-type is the type of the partial order (optional). The value could be one of NO_ORDER, BITREE, RANDOM or ONE_TO_MANY_TO_ONE. 
If the option is missing, the type of the partial order is generated randomly for each example. 
* --output-dir is a path to a directory for saving the generated examples (optional),
* --edge-probability is a probability of edge between two jobs in case of RANDOM partial order (optional),
* --avg-graph-component-size is average size of a connectivity graph component in case of BITREE or ONE_TO_MANY_TO_ONE partial orders.

### Run

To compile all the programs run script compile-*.sh (mkl for intel, def for arm).
To run the program execute script run-generator.sh.

### Input and output format

Input: none

Output:

    file names: "example_2j_NO_ORDER_0.txt", where 2j means "two jobs", "NO_ORDER" means the type of the partial order, the last number 0 - the example number.
    file format (total jobs count, jobs ids and the order table):
        2
        25 COPY  200 SUM
        0 0
        0 0


## Measure the data bus bandwidth consumption

### Description

The program measures the data bus bandwidth consumption for each job using our custom algorithm. It should be run once on each new computer. 
The program has the following set of arguments:
* --core-preset is one of persets (INTEL_SERVER_NODE_0, INTEL_SERVER_NODE_1, HUAWEI_SERVER_NODE_0, HUAWEI_SERVER_NODE_1, LOCALHOST) 
* with physical cores numbers to use for the program execution (required),
* --iteration-count is count of iterations to perform for each job (optional),
* --output-file is a path to a file for saving the results (optional).

### Run

To compile all the programs run script compile-*.sh (mkl for intel, def for arm).
To run the program execute script run-databuscollector.sh.

### Input and output format

Input: none

Output:

    file name: "bus_percents_results.txt"
    file format (job id, standard time of execution and calculated data bus bandwidth consumption):
        20_XPY   3142   59.0935
        1000_QR   3119   59.9232
        10_COPY   3168   66.4569
        ...


## Execute jobs

### Description

The program performs jobs according to the order table from the input. The table can rigidly define the order of jobs execution (schedule), but also 
can contain only the partial order of the jobs (from the generated examples). Tbb is used for the execution.
The program has the following set of arguments:
* --core-preset is one of persets (INTEL_SERVER_NODE_0, INTEL_SERVER_NODE_1, HUAWEI_SERVER_NODE_0, HUAWEI_SERVER_NODE_1, LOCALHOST)
* with physical cores numbers to use for the program execution (required),
* --input-dir is a path to a directory with the input files (optional),
* --input-file is a path to an input file (optional),
* --iteration-count is count of iterations to perform for each input file (optional),
* --output-file is a path to a file for saving the results (optional).

### Run

To compile all the programs run script compile-*.sh (mkl for intel, def for arm).
To run the program execute script run-executor.sh.

### Input and output format

Input: 
The files with any name and with following format:

    4                                  // jobs count
    60 SUM  35 COPY  40 SUM  45 XPY    // the job ids
    0 1 0 0                            // order table. this line means "job0 should be executed after job1"
    0 0 0 0                            // order table. this line means "no job should be executed before job1"
    1 0 0 1                            // order table. this line means "job2 should be executed after job0 and job3"
    0 0 0 0                            // order table. this line means "no job should be executed before job3"

Output:

    file name: "executor_results.txt"
    file format (input file name, iteration durations):
        data_4j_BITREE_4c_60_SUM_35_COPY_40_SUM_45_XPY.txt
        10078,10144,10002,9986,11762

        data_6j_NO_ORDER_4c_40_COPY_50_SUM_1200_QR_45_SUM_40_XPY_30_COPY.txt
        7234,7366,7149,7199,7250


## Run greedy algorithm

### Description

The program runs the greedy algorithms to build schedules.
The program has the following set of arguments:
* --cores-count is the cores count to build schedule for
* --data-bus-file is a path to a file with measuring data bus results
* --path-to-examples is a path to a dir with examples
* --output-dir is a path to a dir for storing results

### Run

To compile the program use `java -./greedy-algorithm/GreedyAlgorithm.java` command.

### Input and output format

Input:
The files generated by jobs-generator.

    file names: "example_2j_NO_ORDER_0.txt", where 2j means "two jobs", "NO_ORDER" means the type of the partial order, the last number 0 - the example number.
    file format (total jobs count, jobs ids and the order table):
        2
        25 COPY  200 SUM
        0 0
        0 0

Output:
The files for execution by jobs-executor.

    file names: "ga_out_4j_NO_ORDER_0.txt", where 4j means "four jobs", "NO_ORDER" means the type of the partial order, the last number 0 - the example number.
    file format (total jobs count, jobs ids and the order table):
        4                                  // jobs count
        60 SUM  35 COPY  40 SUM  45 XPY    // the job ids
        0 1 0 0                            // order table. this line means "job0 should be executed after job1"
        0 0 0 0                            // order table. this line means "no job should be executed before job1"
        1 0 0 1                            // order table. this line means "job2 should be executed after job0 and job3"
        0 0 0 0                            // order table. this line means "no job should be executed before job3"

## Compare results

### Description

Two programs are available to compare the result: TimeComparator and DispersionComparator.
TimeComparator allows comparing time execution of tbb and ga for the same example.
DispersionComparator allows comparing dispersion of execution of tbb and ga for the same example.
The programs have the following set of arguments:
* --tbb-results-file is a path to a file with tbb results
* --ga-results-file is a path to a file with ga results

### Run

To compile the programs use `java -./comparators/TimeComparator.java` or `java -./comparators/DispersionComparator.java` commands.

### Input and output format

Input:
The files generated by jobs-executor.

    file name: "executor_results.txt"
    file format (input file name, iteration durations):
        data_4j_BITREE_4c_60_SUM_35_COPY_40_SUM_45_XPY.txt
        10078,10144,10002,9986,11762

        data_6j_NO_ORDER_4c_40_COPY_50_SUM_1200_QR_45_SUM_40_XPY_30_COPY.txt
        7234,7366,7149,7199,7250

Output:
The console output with the following format:

    TimeComparator.java:
        filename	            avg ga time	    avg tbb time
        200j_NO_ORDER_0.txt	    401724,00	    401663,14
        200j_BITREE_0.txt	    438563,00	    412534,00
        200j_RANDOM_0.txt	    483105,00	    428989,07
        100j_NO_ORDER_0.txt	    188540,00	    195816,71
        100j_BITREE_0.txt	    284275,00	    284958,93
        100j_RANDOM_0.txt	    278200,00	    243663,64

    DispersionComparator.java:
        filename	            ga dispersion	    ga avg time	        tbb dispersion	    tbb avg time
        100j_NO_ORDER_1.txt	    2058661,53	        161336,57	        17248312,66	        178810,64
        50j_NO_ORDER_7.txt	    6104323,03	        79890,21	        18265710,03	        87609,21
        200j_BITREE_7.txt	    6356138,43	        389846,00	        137105844,96	    386189,57
        200j_RANDOM_0.txt	    20266932,63     	429445,29	        90901769,66	        443943,64
        100j_RANDOM_4.txt	    25621092,45	        268376,21	        99468114,12	        263000,14
        200j_BITREE_8.txt	    21474878,21     	321139,07	        56370897,23	        314856,36

It's convenient  to copy the output to an Excel table and add columns to see the improvement in percent.