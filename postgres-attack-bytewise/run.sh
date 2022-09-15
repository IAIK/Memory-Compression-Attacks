#!/bin/bash

set -e

./attack.py ./TEMPLATE400k -h 147.75.100.61 -i enp7s0 -o test.png
killall dumpcap || true
