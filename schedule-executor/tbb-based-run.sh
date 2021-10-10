#!/bin/bash

dpcpp -std=c++17 -DMKL_ILP64 -I${MKLROOT}/include ./TbbBasedExecutor.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl

chmod +x a-tbb.out

numactl -m 0 -N 0 ./a-tbb.out 2> debug.txt
