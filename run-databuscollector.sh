#!/bin/bash

numactl -m 1 -N 1 ./databuscollector --core-preset HUAWEI_SERVER_NODE_1
