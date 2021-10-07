#!/bin/bash

g++ ./StatisticsCollector.cpp -lblas64 -llapacke64 -lpthread -lm -ldl

chmod +x a.out
