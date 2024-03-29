#!/bin/bash

set -e

mkdir -p results
for i in {1..20}
do
    ./dict_attack.py ./TEMPLATE -h 127.0.0.1 -i enp7s0 -o results/test_$i.png
    killall dumpcap || true
    mkdir -p results/$i
    mv measurements*.csv results/$i/
done
