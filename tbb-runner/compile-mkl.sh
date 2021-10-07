#!/bin/bash

dpcpp -DMKL_ILP64 -I${MKLROOT}/include ./TbbRunner.cpp -L${MKLROOT}/lib/intel64 -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -ltbb -lpthread -lm -ldl

chmod +x a.out
