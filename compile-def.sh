#!/bin/bash
set -x  #echo on

g++ -std=c++17 ./jobs-generator/JobsGenerator.cpp -o generator && chmod +x generator

g++ -std=c++17 ./bus-bandwidth-collector/BasBandwidthCollector.cpp -lblas64 -llapacke64 -lpthread -lm -ldl -o databuscollector && chmod +x databuscollector

g++ -std=c++17 ./jobs-executor/JobsExecutor.cpp -lblas64 -llapacke64 -lpthread -lm -ldl -o executor && chmod +x executor
