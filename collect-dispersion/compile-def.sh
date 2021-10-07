#!/bin/bash

g++ ./DispersionCollector.cpp -lblas64 -llapacke64 -lpthread -lm -ldl && chmod +x a.out
