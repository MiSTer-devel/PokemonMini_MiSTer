#!/bin/bash
./build.sh $1
make -C obj_dir/ -f V$1.mk
./obj_dir/V$1
