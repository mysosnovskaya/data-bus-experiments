#!/bin/bash
set -x  #echo on

g++ -g -O3 -std=c++17 ./jobs-generator/JobsGenerator.cpp -o generator && chmod +x generator

g++ -g -O3 -std=c++17 ./bus-bandwidth-collector/BusBandwidthCollector.cpp -o databuscollector -lblas64 -llapacke64 -lpthread -lm -ldl && chmod +x databuscollector

g++ -g -O3 -std=c++17 ./jobs-executor/JobsExecutor.cpp -o executor -ltbb -lblas64 -llapacke64 -lpthread -lm -ldl && chmod +x executor

chmod +x run-generator.sh run-executor.sh run-databuscollector.sh
