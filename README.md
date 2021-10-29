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