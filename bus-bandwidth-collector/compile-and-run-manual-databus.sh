#!/bin/bash

dpcpp -DMKL_ILP64 -I${MKLROOT}/include ./CalculateBusPercent.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl -o ManualCollector.out

chmod +x ManualCollector.out

sudo numactl -m 0 -N 0 ./ManualCollector.out
