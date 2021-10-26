#!/bin/bash
set -x  #echo on

g++ -O3 -std=c++17 ./jobs-generator/JobsGenerator.cpp -o generator && chmod +x generator

g++ -O3 -std=c++17 ./bus-bandwidth-collector/BasBandwidthCollector.cpp -o databuscollector -lblas64 -llapacke64 -lpthread -lm -ldl && chmod +x databuscollector

g++ -O3 -std=c++17 ./jobs-executor/JobsExecutor.cpp -o executor -Ioneapi-tbb-2021.4.0/include -Loneapi-tbb-2021.4.0/lib/intel64/gcc4.8 -ltbb -lblas64 -llapacke64 -lpthread -lm -ldl && chmod +x executor

chmod +x run-generator.sh run-executor.sh run-databuscollector.sh
