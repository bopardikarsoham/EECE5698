#!/bin/bash
# build_par_hw.sh - builds a U280 bitstream of the parallel prototype (fused state
# machine) for the graph currently in src/par/par_graph.h.
#
#   hls/build_par_hw.sh <name> [mhz]      run from the repository root
#
# Writes build/<name>.xclbin. v++ lowers the clock on its own if timing fails at the
# requested frequency (default 300 MHz); the link summary gives the clock it reached.
set -e
NAME=${1:?usage: build_par_hw.sh <name> [mhz]}
MHZ=${2:-300}
PLATFORM=/opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm
mkdir -p build
v++ -t hw --platform $PLATFORM -k par_decoder --kernel_frequency $MHZ \
    -D PAR_FUSED -I src/par --compile -o build/$NAME.xo src/par/par_decoder.cpp
v++ -t hw --platform $PLATFORM --kernel_frequency $MHZ \
    --link build/$NAME.xo -o build/$NAME.xclbin
echo "built build/$NAME.xclbin"
