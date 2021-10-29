#!/bin/bash

# This script runs databuscollector to collect the data bus bandwidth. The full command looks like:
#
# numactl -m 1 -N 1 ./databuscollector --core-preset HUAWEI_SERVER_NODE_1 --iteration-count 30 --output-file "databus_collector_results.txt"
#
# where
#   --core-preset is required option,
#   --iteration-count is 7 by default,
#   --output-file is "bus-bandwidth-collector/results/bus_percents_results.txt" by default.

numactl -m 1 -N 1 ./databuscollector --core-preset HUAWEI_SERVER_NODE_1
