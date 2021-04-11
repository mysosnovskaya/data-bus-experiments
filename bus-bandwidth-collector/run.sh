#!/bin/bash

sudo vtune -collect io -result-dir results numactl -m 0 -N 0 ./VtuneCollector.out
sudo numactl -m 0 -N 0 ./ManualCollector.out
