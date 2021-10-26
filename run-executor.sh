#!/bin/bash

export LD_LIBRARY_PATH=oneapi-tbb-2021.4.0/lib/intel64/gcc4.8

numactl -m 1 -N 1 ./executor --core-preset HUAWEI_SERVER_NODE_1
