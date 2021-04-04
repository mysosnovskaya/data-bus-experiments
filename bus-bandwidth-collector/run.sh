#!/bin/bash

sudo /opt/intel/oneapi/vtune/latest/bin64/vtune -collect io numactl -m 0 -N 0 ./a.out
