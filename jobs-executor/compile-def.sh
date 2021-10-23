#!/bin/bash

g++ -std=c++17 ./Executor.cpp -lblas64 -llapacke64 -lpthread -lm -ldl

chmod +x a.out
