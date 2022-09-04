#!/bin/bash
python3 ../scripts/generate_microrom.py
mkdir -p rom/
mv *.mem rom/

if [ "$(uname)" == "Darwin" ]
then
    $VERILATOR_ROOT/bin/verilator -O3 -Wno-fatal -trace --top-module minx -I../rtl --cc ../rtl/minx.sv --exe minx_sdl2_sim.cpp -LDFLAGS "-framework OpenGL `sdl2-config  --libs` -lglew"
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]
then
    $VERILATOR_ROOT/bin/verilator -O3 -Wno-fatal -trace --top-module minx -I../rtl --cc ../rtl/minx.sv --exe minx_sdl2_sim.cpp -LDFLAGS "-lGL `sdl2-config  --libs` -lGLEW"
fi

make -C obj_dir/ -f Vminx.mk
