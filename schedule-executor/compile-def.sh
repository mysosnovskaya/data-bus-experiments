#!/bin/bash

g++ -std=c++17 ./TbbBasedExecutor.cpp -lblas64 -llapacke64 -lpthread -lm -ldl

chmod +x a.out
