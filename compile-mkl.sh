#!/bin/bash
set -x  #echo on

dpcpp -g -O3 -std=c++17 ./jobs-generator/JobsGenerator.cpp -o generator && chmod +x generator

dpcpp -g -O3 -std=c++17 -DMKL_ILP64 -I${MKLROOT}/include ./bus-bandwidth-collector/BusBandwidthCollector.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl -o databuscollector && chmod +x databuscollector

dpcpp -g -O3 -std=c++17 -DMKL_ILP64 -I${MKLROOT}/include ./jobs-executor/JobsExecutor.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -ltbb -lpthread -lm -ldl -o executor && chmod +x executor

chmod +x run-generator.sh run-executor.sh run-databuscollector.sh
