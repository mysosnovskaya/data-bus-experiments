#!/bin/bash

export LD_LIBRARY_PATH=oneapi-tbb-2021.4.0/lib/intel64/gcc4.8

# This script runs executor to execute a set of jobs according to the input order. The full command looks like:
#
# numactl -m 1 -N 1 ./executor --core-preset HUAWEI_SERVER_NODE_1 --input-dir "jobs-executor/input" --iteration-count 30 --output-file "executor_results.txt"
#
# where
#   --core-preset is required option,
#   --input-dir is "jobs-executor/input" by default,
#   --iteration-count is 30 by default,
#   --output-file is "jobs-executor/results/jobs_executor_results.txt" by default,
# Also you can define --input-file instead of --input-dir option.

numactl -m 1 -N 1 ./executor --core-preset HUAWEI_SERVER_NODE_1
