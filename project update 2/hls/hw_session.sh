#!/bin/bash
# hw_session.sh - one hardware session: synthesize / build / run decoder variants.
# Run from the project root. Each variant gets its own copy of the project in
# $RUNS/<tag> (default ~/qec_runs), so several can build at the same time.
#
#   bash hls/hw_session.sh synth <tag> <uf_fast.h> [clock_ns]
#       build VM: C simulation, HLS synthesis and batch co-simulation of the variant
#       whose decoder is <uf_fast.h> (e.g. results/hw/v2_src/uf_fast_v7.h), at
#       clock_ns (default 3.33). Writes $RUNS/<tag>/summary_synth.txt: the II of every
#       loop, the clock estimate, resources and the co-simulation cycles.
#
#   bash hls/hw_session.sh bitstream <tag> <uf_fast.h> [MHz]
#       build VM: bitstream (about 2 h) and host program at MHz (default 300). Copies
#       the xclbin to $RUNS/xclbin/<tag>.xclbin and the routed timing and kernel
#       utilization reports to $RUNS/<tag>/reports/, and writes summary_bitstream.txt.
#
#   bash hls/hw_session.sh run <xclbin> <tag> <case_dir> [<case_dir> ...]
#       FPGA node: run the bitstream on every case; results/hw_<tag>.log and one
#       per-shot CSV (with the on-board stage counters) per case in
#       results/work_<tag>/. Then: python python/hw_summary.py results/hw_<tag>.log
#       results/work_<tag> results/hw_<tag>.csv
set -u
MODE=${1:-}
RUNS=${RUNS:-$HOME/qec_runs}
ROOT=$(pwd)

source_tools() {
    for f in /fpga/Xilinx/Vitis_HLS/2023.1/settings64.sh /fpga/Xilinx/Vitis/2023.1/settings64.sh \
             /opt/xilinx/xrt/setup.sh; do
        [ -f "$f" ] && { source "$f" >/dev/null 2>&1 || true; }
    done
}

# Fresh copy of the project with the variant's decoder in src/uf_fast.h.
make_copy() {
    local tag=$1 uf=$2
    rm -rf "$RUNS/$tag"
    mkdir -p "$RUNS/$tag"
    cp -r "$ROOT/src" "$ROOT/hls" "$ROOT/tests" "$ROOT/Makefile" "$RUNS/$tag/"
    cp "$uf" "$RUNS/$tag/src/uf_fast.h"
    # KDEFS="UF_MAX_DET=5120 UF_MAX_EDGES=28672" changes the kernel's size limits
    # (the #defines at the top of src/kernel/qec_decoder.cpp) in this copy only.
    for kv in ${KDEFS:-}; do
        sed -i "s/^#define ${kv%%=*} .*/#define ${kv%%=*} ${kv#*=}/" "$RUNS/$tag/src/kernel/qec_decoder.cpp"
    done
    grep "^#define UF_MAX" "$RUNS/$tag/src/kernel/qec_decoder.cpp"
    mkdir -p "$RUNS/$tag/results"
}

case "$MODE" in
synth)
    tag=$2; uf=$3; clk=${4:-3.33}
    source_tools
    make_copy "$tag" "$uf"
    cd "$RUNS/$tag"
    HLS_CLOCK_NS=$clk vitis_hls -f hls/run_hls.tcl -tclargs cosim_batch > synth.log 2>&1
    {
        echo "== $tag  clock target $clk ns  $(date)"
        echo "-- loops (II, depth)"
        grep "Pipelining result" vitis_hls.log | sed 's/.*Pipelining result : //'
        echo "-- clock estimate"
        grep "ap_clk" hls_prj/sol1/syn/report/qec_decoder_csynth.rpt | head -1
        echo "-- resources (BRAM_18K DSP FF LUT URAM)"
        grep "^|Total" hls_prj/sol1/syn/report/qec_decoder_csynth.rpt | head -1
        echo "-- C/RTL co-simulation (200 d=3 shots, one call)"
        grep "COSIM 212-1000\|cases PASS" synth.log
        grep "Verilog|" hls_prj/sol1/sim/report/qec_decoder_cosim.rpt
    } > summary_synth.txt 2>&1
    cat summary_synth.txt
    ;;
bitstream)
    tag=$2; uf=$3; mhz=${4:-300}
    source_tools
    make_copy "$tag" "$uf"
    cd "$RUNS/$tag"
    make kernel host KERNEL_FREQ=$([ "$mhz" = 300 ] && echo "" || echo "$mhz") > build.log 2>&1
    mkdir -p reports "$RUNS/xclbin"
    cp _x/reports/link/imp/impl_1_kernel_util_routed.rpt \
       _x/reports/link/imp/impl_1_hw_bb_locked_timing_summary_routed.rpt reports/ 2>/dev/null
    [ -f build/qec_decoder.xclbin ] && cp build/qec_decoder.xclbin "$RUNS/xclbin/$tag.xclbin"
    {
        echo "== $tag  $mhz MHz  $(date)"
        grep -n "ERROR" build.log | head -5
        ls -la build/qec_decoder.xclbin 2>&1
        echo "-- routed timing (WNS first)"
        grep -A6 "Design Timing Summary" reports/impl_1_hw_bb_locked_timing_summary_routed.rpt | tail -1
        echo "-- routed kernel utilization"
        grep "^|    qec_decoder_1" reports/impl_1_kernel_util_routed.rpt
    } > summary_bitstream.txt 2>&1
    cat summary_bitstream.txt
    ;;
run)
    xclbin=$2; tag=$3; shift 3
    source_tools
    mkdir -p "results/work_$tag"
    for d in "$@"; do
        c=$(basename "${d%/}")
        echo "### $c"
        ./build/host "$xclbin" "${d%/}" "results/work_$tag/$c.csv"
    done > "results/hw_$tag.log" 2>&1
    echo "$(grep -c '^PASS' "results/hw_$tag.log") / $# cases PASS"
    grep "differ from golden: [1-9]" "results/hw_$tag.log"
    true
    ;;
*)
    sed -n '2,24p' "$0"
    exit 2
    ;;
esac
