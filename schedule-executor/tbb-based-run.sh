#!/bin/bash

dpcpp -std=c++17 -DMKL_ILP64 -I${MKLROOT}/include ./TbbBasedExecutor.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -ltbb -lpthread -lm -ldl -o a-tbb.out

chmod +x a-tbb.out

numactl -m 1 -N 1 ./a-tbb.out 2> debug.txt
