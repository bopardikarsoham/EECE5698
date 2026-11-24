# run_par.tcl - Vitis HLS flow for the parallel prototype (src/par/).
#
#   PAR_CASE=<case_dir> [PAR_SHOTS=200] [PAR_CLK=3.33] [PAR_DEFS=-DPAR_FUSED] vitis_hls -f hls/run_par.tcl
#
# (Settings come from the environment: vitis_hls puts its own options in argv.)
# src/par/par_graph.h must be generated from <case_dir>/graph.txt first
# (python/gen_par_graph.py), and <case_dir>/par_expected.bin written by
# tools/par_model.cpp --write-par-expected. Runs C simulation on every shot, synthesis,
# then C/RTL co-simulation of cosim_shots shots (default 200).
set case_dir $::env(PAR_CASE)
set nshots [expr {[info exists ::env(PAR_SHOTS)] ? $::env(PAR_SHOTS) : 200}]
set clk [expr {[info exists ::env(PAR_CLK)] ? $::env(PAR_CLK) : 3.33}]
set defs [expr {[info exists ::env(PAR_DEFS)] ? $::env(PAR_DEFS) : ""}]
set root [pwd]

open_project -reset par_prj
set_top par_decoder
add_files src/par/par_decoder.cpp -cflags "-std=c++14 $defs"
add_files -tb src/par/par_tb.cpp -cflags "-std=c++14 -Wno-unknown-pragmas $defs"
open_solution -reset sol1 -flow_target vitis
set_part xcu280-fsvh2892-2L-e
create_clock -period $clk -name default
config_compile -unsafe_math_optimizations=false

csim_design -argv "$root/$case_dir"
csynth_design
cosim_design -argv "$root/$case_dir $nshots" -trace_level none
exit
