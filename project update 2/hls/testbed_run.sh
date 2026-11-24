#!/bin/bash
# testbed_run.sh - run the kernel flow on the OCT build machine, from the project root.
#
#   bash hls/testbed_run.sh csim     g++ testbench, then Vitis HLS C simulation (minutes)
#   bash hls/testbed_run.sh synth    the above, then HLS synthesis + report summary
#   bash hls/testbed_run.sh cosim    the above, then C/RTL co-simulation (cycles per shot)
#   bash hls/testbed_run.sh hw       the above, then bitstream (about 1-2 h) + host build
#   bash hls/testbed_run.sh run      run the built bitstream on the U280 for every test case
#
# Everything printed is also saved to results/testbed_<step>.log.
set -e
STEP=${1:-csim}
mkdir -p results
LOG=results/testbed_$STEP.log

# Source the tools if they are not on the PATH yet (paths on the OCT build VMs).
if ! command -v vitis_hls >/dev/null 2>&1; then
    for f in /fpga/Xilinx/Vitis_HLS/2023.1/settings64.sh /fpga/Xilinx/Vitis/2023.1/settings64.sh \
             /opt/xilinx/xrt/setup.sh; do
        [ -f "$f" ] && { source "$f" >/dev/null 2>&1 || true; }
    done
fi

{
echo "== $(date)  $(hostname)  step=$STEP"
if [ "$STEP" = run ]; then
    for dir in tests/cases/*/; do
        ./build/host build/qec_decoder.xclbin "${dir%/}"
    done
    exit 0
fi

echo "== g++ kernel testbench"
make kernel_csim

echo "== Vitis HLS C simulation"
case "$STEP" in csim|synth|cosim) HLS_MODE=$STEP ;; *) HLS_MODE=synth ;; esac
vitis_hls -f hls/run_hls.tcl -tclargs "$HLS_MODE"

if [ "$STEP" != csim ]; then
    RPT=hls_prj/sol1/syn/report/qec_decoder_csynth.rpt
    echo "== Synthesis report summary ($RPT)"
    sed -n '/== Performance Estimates/,/== Utilization Estimates/p' "$RPT" | head -40
    sed -n '/== Utilization Estimates/,/Detail/p' "$RPT" | head -30
    cp "$RPT" results/
fi

if [ "$STEP" = cosim ]; then
    CRPT=hls_prj/sol1/sim/report/qec_decoder_cosim.rpt
    echo "== Co-simulation report ($CRPT)"
    cat "$CRPT"
    cp "$CRPT" results/
fi

if [ "$STEP" = hw ]; then
    echo "== Bitstream (v++ compile + link)"
    make kernel
    echo "== Host program"
    make host
fi
} 2>&1 | tee "$LOG"
