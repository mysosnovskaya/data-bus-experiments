#!/bin/bash

numactl -m 0 -N 0 ./a.out 2> debug.txt
