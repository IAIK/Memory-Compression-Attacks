#!/bin/bash

declare -a SECRET_ARRAY=(2c c42c 00c42c 9d00c42c)

for i in {0..3}
do
  sec_idx=$((i%4));
  sudo ./resources/setup_cgroup.sh small_mem$i
  sudo python3 fuzzer.py -e 20 -n 5000 -c $i -f findings_zram_$i -l zram -x ${SECRET_ARRAY[$sec_idx]} -g small_mem$i -r 3 2>&1 &
done
