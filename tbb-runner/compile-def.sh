#!/bin/bash

g++ -std=c++17 ./TbbRunner.cpp -lblas64 -llapacke64 -ltbb -lpthread -lm -ldl

chmod +x a.out
