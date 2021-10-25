#!/bin/bash
set -x  #echo on

dpcpp -std=c++17 jobs-generator/JobsGenerator.cpp -o generator && chmod +x generator

dpcpp -std=c++17 -DMKL_ILP64 -I${MKLROOT}/include ./bus-bandwidth-collector/BasBandwidthCollector.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl -o databuscollector && chmod +x databuscollector

dpcpp -std=c++17 -DMKL_ILP64 -I${MKLROOT}/include ./jobs-executor/JobsExecutor.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -ltbb -lpthread -lm -ldl -o executor && chmod +x executor
