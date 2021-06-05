#!/bin/bash

lscpu > ./results/lscpu_output.txt && \
numactl -m 0 -N 0 ./a.out
