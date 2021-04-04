#!/bin/bash


dpcpp  -DMKL_ILP64 -I${MKLROOT}/include ./StatisticsCollector.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl
