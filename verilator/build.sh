#!/bin/bash
python3 ../scripts/generate_microrom.py
$VERILATOR_ROOT/bin/verilator -O3 -Wno-fatal -trace --top-module $1 -I../rtl --cc ../rtl/$1.sv --exe $1_sim.cpp
#verilator -O3 -Wno-fatal -trace --top-module 's1c88' -I.. --cc ../s1c88.sv --exe s1c88_sim.cpp
