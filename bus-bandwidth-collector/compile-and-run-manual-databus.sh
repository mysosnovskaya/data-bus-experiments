#!/bin/bash

dpcpp -g -O3 -DMKL_ILP64 -I${MKLROOT}/include ./CalculateBusPercent.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl -o ManualCollector.out && chmod +x ManualCollector.out

numactl -m 1 -N 1 ./ManualCollector.out
