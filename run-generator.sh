#!/bin/bash

# This script runs generator to generate sets of jobs (examples). The full command looks like:
#
# ./generator --jobs-count 50 --examples-count 10 --partial-order-type RANDOM --output-dir "jobs-generator/results" --edge-probability 0.02 --avg-graph-component-size 10
#
# where
#   --jobs-count is required option,
#   --examples-count is required option,
#   --partial-order-type is optional (has no default value, will be generated randomly),
#   --output-dir is "jobs-generator/results" by default,
#   --edge-probability is 0.02 by default,
#   --avg-graph-component-size is 10 by default.

./generator --jobs-count 50 --examples-count 10
