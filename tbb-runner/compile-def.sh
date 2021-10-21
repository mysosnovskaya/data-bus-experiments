#!/bin/bash

g++ -g -O3 -std=c++17 ./TbbRunner.cpp -lblas64 -llapacke64 -ltbb -lpthread -lm -ldl && chmod +x a.out
