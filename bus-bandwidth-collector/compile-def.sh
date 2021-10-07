#!/bin/bash

g++ ./CalculateBusPercent.cpp -lblas64 -llapacke64 -lpthread -lm -ldl -o ManualCollector.out && chmod +x ManualCollector.out
