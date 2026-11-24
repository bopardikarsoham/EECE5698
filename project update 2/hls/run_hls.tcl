# run_hls.tcl - Vitis HLS C simulation, synthesis and C/RTL co-simulation of the
# union-find kernel.
#
# Usage (from the project root):
#   vitis_hls -f hls/run_hls.tcl -tclargs csim    C simulation against tests/cases/
#   vitis_hls -f hls/run_hls.tcl -tclargs synth   C simulation, then synthesis
#   vitis_hls -f hls/run_hls.tcl -tclargs cosim   all of the above, then co-simulation
#
# Reports:
#   hls_prj/sol1/syn/report/qec_decoder_csynth.rpt   latency, resources, clock estimate
#   hls_prj/sol1/sim/report/qec_decoder_cosim.rpt    cycles per call (min / avg / max)
#
# Co-simulation decodes one shot per call (tb.cpp --per-shot), so its per-call cycle
# counts are per-shot cycle counts. The first call only loads the graph.

# vitis_hls puts its own options (-f <script>) in argv too, so the mode is the last word.
set mode [lindex $argv end]
if {$mode ni {csim synth cosim cosim_batch}} { set mode csim }

set root [pwd]
open_project -reset hls_prj
set_top qec_decoder
add_files $root/src/kernel/qec_decoder.cpp
add_files -tb $root/src/kernel/tb.cpp

open_solution -reset sol1 -flow_target vitis
set_part xcu280-fsvh2892-2L-e
# 300 MHz, the clock the U280 kernel ran at in the bring-up. HLS_CLOCK_NS in the
# environment overrides it (clock sweeps).
set clock_ns 3.33
if {[info exists ::env(HLS_CLOCK_NS)]} { set clock_ns $::env(HLS_CLOCK_NS) }
create_clock -period $clock_ns -name default

# The testbench runs in hls_prj/sol1/csim/build, so the case paths must be absolute.
set cases [lsort [glob -type d $root/tests/cases/*]]
csim_design -argv [join $cases " "]

if {$mode in {synth cosim cosim_batch}} {
    csynth_design
}
if {$mode eq "cosim"} {
    # 200 shots of the d = 3 surface code case, one per call, plus a separate CSV of
    # each shot's work count to pair with the per-call cycle counts.
    set d3 $root/tests/cases/surf_z_d3_p0.01_n100000
    cosim_design -trace_level none \
        -argv "--per-shot 200 --work-csv $root/results/cosim_work_d3.csv $d3"
}
if {$mode eq "cosim_batch"} {
    # The same 200 shots in a single call, as the host runs the FPGA: total cycles / 200
    # is the cost per shot including the burst reads and writes.
    set d3 $root/tests/cases/surf_z_d3_p0.01_n100000
    cosim_design -trace_level none -argv "--batch 200 $d3"
}
exit
