#!/bin/bash

g++ -O3 -std=c++17 ./TbbRunner.cpp \
  -I/home/sosnovskaya/.local/include \
  -L/home/sosnovskaya/.local/lib \
  -Wl,-rpath,/home/sosnovskaya/.local/lib \
  /home/sosnovskaya/.local/lib/libmkl_intel_lp64.so.2 \
  /home/sosnovskaya/.local/lib/libmkl_sequential.so.2 \
  /home/sosnovskaya/.local/lib/libmkl_core.so.2 \
  /home/sosnovskaya/.local/lib/libtbb.so \
  -lpthread -lm -ldl \
  -o a.out && chmod +x a.out
