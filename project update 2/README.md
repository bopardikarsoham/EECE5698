# Union-Find Quantum Error Decoder on the Alveo U280: Project Update 2

EECE 5698, Soham Bopardikar. The report is [`REPORT.md`](REPORT.md).

A weighted union-find decoder for the rotated surface code, written in C++ for Vitis
HLS. It runs on an AMD Alveo U280, and the card's output matches the C++ reference
decoder bit for bit on every shot. This README covers what is here, the tools and
settings, and step-by-step instructions for every result in the report.

## 1. Tools and settings

| Item | Value |
|---|---|
| FPGA board | AMD Alveo U280 (Open Cloud Testbed, `fpga-alveo` node) |
| Platform (shell) | `xilinx_u280_gen3x16_xdma_1_202211_1` (part `xcu280-fsvh2892-2L-e`) |
| HLS / bitstream tools | Vitis HLS 2023.1, Vitis (`v++`) 2023.1, on the OCT build VM (`fpga-tools`) |
| Runtime | XRT 2.15 (2023.1) on the FPGA node, Ubuntu 22.04 |
| Kernel clock | **300 MHz** (HLS target 3.33 ns; `v++` platform default). Change with `KERNEL_FREQ=<MHz>` |
| Kernel name / top function | `qec_decoder` (`src/kernel/qec_decoder.cpp`) |
| Software reference | g++ (C++17) or Apple clang, `-O2` |
| Python (data generation and scoring only) | 3.10+, packages in `requirements.txt` (stim 1.16, pymatching 2.4, numpy) |

Decoder limits (compile-time): the FPGA kernel takes up to 2048 detectors, 8192 edges
and 12 edges per detector (`#define`s at the top of `src/kernel/qec_decoder.cpp`), which
covers the surface code up to d = 11 (1320 detectors, 6718 edges). The reference decoder
(`src/uf_core.h`) defaults to 4096 / 16384. For d = 13 to 17 (up to 4896 detectors and
26,398 edges), build with larger limits:
`KDEFS="UF_MAX_DET=5120 UF_MAX_EDGES=28672" bash hls/hw_session.sh bitstream ...`, and
build the reference with `-DUF_MAX_DET=5120 -DUF_MAX_EDGES=28672`.

## 2. Files

| Path | What it is |
|---|---|
| `src/uf_core.h` | Reference union-find decoder (Update 1); produces the golden outputs |
| `src/golden.cpp` | Runs the reference decoder on a case directory |
| `src/uf_fast.h` | **The FPGA decoder (v9c)**: same results as `uf_core.h`, restructured for hardware |
| `src/kernel/qec_decoder.cpp` | **The HLS kernel**: AXI interfaces, graph loading, packed-shot input, prediction and info output |
| `src/info_layout.h` | Layout of the 12-word per-shot info record, including the on-board stage counters |
| `src/kernel/tb.cpp` | Kernel testbench (g++, Vitis C simulation, C/RTL co-simulation); `--time` gives CPU timing |
| `src/host/host.cpp`, `src/host/case_io.h` | XRT host program: runs the kernel on the U280 and checks every shot against the golden output |
| `hls/run_hls.tcl` | Vitis HLS script: `csim`, `synth`, `cosim`, `cosim_batch` |
| `hls/hw_session.sh` | One-command synthesis (`synth`), bitstream (`bitstream`) and card runs (`run`) of any decoder version |
| `hls/testbed_run.sh` | The earlier step-by-step flow (csim → synth → cosim → hw → run) |
| `Makefile` | `check_all`, `kernel_csim`, `hls_csim`, `hls_synth`, `hls_cosim`, `kernel`, `host`, data targets |
| `tests/cases/*/` | **Inputs and golden outputs** (section 3) |
| `python/gen_data.py` | Stim simulation: graph, shots, true answers, PyMatching predictions |
| `python/score.py`, `python/sweep.py` | Logical error rates, union-find vs PyMatching, over grids of d and p |
| `python/hw_summary.py` | Summarizes a card run: µs/shot, tail estimates, writes a CSV |
| `python/stage_breakdown.py` | Cycles per decoder stage from the on-board counters and the HLS IIs |
| `python/plot_results.py`, `python/plot_round_budget.py` | Figures in `results/figures/` |
| `results/hw/v2_src/` | Source snapshot of every decoder version (`uf_fast_v3.h` … `uf_fast_v9c.h`) |
| `results/hw/hw_sweep_v*.csv`, `.log` | Card runs of v2 to v5 (40 cases) |
| `results/hw/hw_long_v*.csv`, `.log` | Card runs on the 200,000-shot cases |
| `results/hw/synth_v7_v9/*/summary_synth.txt` | HLS synthesis + co-simulation summaries of v6 to v9c |
| `results/hw/resources.csv`, `timing_summary_routed.txt` | Routed utilization and timing |
| `results/sweep_1m.csv` | Accuracy, 1 million shots per point |
| `results/cpu_baseline_m3.txt` | CPU timing of the same decoder source |
| `results/stage_counts.csv` | Mean stage counters per version and case |
| **Parallel design (report section 5.3)** | |
| `src/par/par_decoder.cpp` | HLS kernel `par_decoder`: one processing element per node and per edge; `-DPAR_FUSED` selects the single state machine that was routed |
| `src/par/par_graph.h` | The graph wired into the kernel (generated; currently d = 5). `par_graph_d3.h`, `par_graph_d5.h` are the two routed graphs |
| `src/par/par_tb.cpp` | Testbench (g++, Vitis C simulation and co-simulation) against `par_expected.bin` |
| `src/host/par_host.cpp` | XRT host program for the parallel kernel |
| `python/gen_par_graph.py` | Writes `src/par/par_graph.h` from a case's `graph.txt` |
| `tools/par_model.cpp` | Cycle model of the parallel decoder; writes the golden output `par_expected.bin` |
| `hls/run_par.tcl`, `hls/build_par_hw.sh` | HLS C simulation / synthesis / co-simulation, and the U280 bitstream build |
| `tests/par_cases/surf_z_d5_p0.003_n100000/` | The d = 5 input and golden outputs used for the d = 5 bitstreams (the d = 3 case is `tests/cases/surf_z_d3_p0.01_n100000`, which also has `par_expected.bin`) |
| `results/par/` | HLS synthesis and co-simulation reports of every parallel version |
| `results/hw/par_*/` | Routed utilization reports and link summaries of the four parallel bitstreams (`par_bitstreams.csv` summarizes them) |
| `results/par_model*.csv` | Cycle-model results |
| `docs/` | Comparison with published FPGA decoders and the research notes the report refers to |

Nothing generated by the build is included: no Vitis project, `build/`, `_x/`,
bitstreams or Stim data. All of it can be regenerated with the steps below.

## 3. Inputs and golden outputs

Each test case is a directory:

| File | Contents |
|---|---|
| `graph.txt` | Decoder graph from Stim's detector error model: a header line `num_detectors num_observables num_edges`, then one edge per line, `u v weight obs_mask` (v = −1 is the boundary); integer weights 4 to 16 |
| `dets.bin` | Shots: for each shot, `ceil(num_det / 8)` bytes, bit i = detector i fired |
| `obs.bin` | True logical flip for each shot (1 byte per shot), from Stim |
| `expected_pred.bin` | **Golden output**: the reference decoder's prediction for each shot (1 byte per shot) |
| `pymatching_pred.bin` | PyMatching's prediction for each shot (accuracy reference) |
| `meta.json` | Code, d, rounds, p, shots, seed used to generate the case |
| `par_expected.bin` | **Golden output of the parallel design** (1 byte per shot). It peels a different spanning forest from the reference, so a few predictions differ from `expected_pred.bin`; accuracy is the same (report section 5.3) |

Shipped cases (`tests/cases/`):

| Case | Code | d | p | Shots |
|---|---|---|---|---|
| `toy_rep_d3`, `toy_rep_d5` | repetition code, hand-checkable | 3, 5 | every pattern of ≤ (d−1)/2 errors | 4, 16 |
| `rep_d3_p0.01_n100000` | repetition code (Stim) | 3 | 0.01 | 100,000 |
| `surf_z_d3_p0.01_n100000` | rotated surface code, Z memory (Stim) | 3 | 0.01 | 100,000 |

The larger cases in the report (d = 3 to 11, p = 0.001 to 0.01) are regenerated with
`python/gen_data.py` (section 4.5). The default seed, 5698, gives the identical shots.

## 4. How to run

All commands run from this folder.

### 4.1 Software check (any machine, g++ and make only, about 1 minute)

```bash
make check_all      # reference decoder vs golden outputs
make kernel_csim    # the FPGA kernel source, compiled with g++, vs golden outputs
```

Expected last lines: `check_all: 4 / 4 cases PASS` and `kernel_tb: 4 / 4 cases PASS`.

### 4.2 HLS synthesis and co-simulation (OCT build VM)

```bash
source /fpga/Xilinx/Vitis_HLS/2023.1/settings64.sh
vitis_hls -f hls/run_hls.tcl -tclargs cosim_batch
```

This runs C simulation (every case must PASS), synthesis at 3.33 ns, then C/RTL
co-simulation of 200 d = 3 shots in one call. Reports:

- `hls_prj/sol1/syn/report/qec_decoder_csynth.rpt`: resources and clock estimate.
- `vitis_hls.log`: the II of every loop, on its `Pipelining result` lines.
- `hls_prj/sol1/sim/report/qec_decoder_cosim.rpt`: total cycles.

Expected for v9c: co-simulation `PASS`, 63,865 cycles, clock estimate 2.848 ns.

The same run in a separate copy of the project, with a short summary:

```bash
bash hls/hw_session.sh synth v9c src/uf_fast.h          # writes ~/qec_runs/v9c/summary_synth.txt
```

### 4.3 Bitstream (OCT build VM, about 2 hours)

```bash
source /fpga/Xilinx/Vitis/2023.1/settings64.sh
make kernel host                                  # build/qec_decoder.xclbin at 300 MHz, build/host
```

Or `bash hls/hw_session.sh bitstream v9c src/uf_fast.h 300`. It also copies the routed
timing and utilization reports and prints the worst negative slack (WNS), which must be
≥ 0.

### 4.4 Run on the U280 (OCT FPGA node)

Copy `build/qec_decoder.xclbin` and this folder to the FPGA node, then:

```bash
source /opt/xilinx/xrt/setup.sh
make host
./build/host build/qec_decoder.xclbin tests/cases/surf_z_d3_p0.01_n100000 work.csv
```

Expected output (numbers vary slightly):

```
Case tests/cases/surf_z_d3_p0.01_n100000: 24 detectors, 78 edges, 100000 shots
...
Kernel time: ... us for 100000 shots (... us/shot, includes kernel launch)
Per shot, counted on the card: defects=... members=... frontier=... apply=... ...
PASS: FPGA output vs golden
```

`PASS` means every one of the 100,000 predictions from the card equals
`expected_pred.bin`. `work.csv` has one row per shot: growth iterations, work, and the
loop counts of each decoder stage as counted on the card.

All cases at once, with a summary:

```bash
bash hls/hw_session.sh run build/qec_decoder.xclbin v9c data/sweep/*    # results/hw_v9c.log, results/work_v9c/
python3 python/hw_summary.py results/hw_v9c.log results/work_v9c results/hw_v9c.csv
python3 python/stage_breakdown.py results/work_v9c/surf_z_d11_p0.001_n10000.csv --version v9c --us-per-shot 7.57
```

### 4.5 Regenerate the data and accuracy results (Python)

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python python/gen_data.py --code surf_z --d 11 --p 0.001 --shots 10000 --out data/sweep/surf_z_d11_p0.001_n10000
make golden && ./build/sw/golden data/sweep/surf_z_d11_p0.001_n10000 --out data/sweep/surf_z_d11_p0.001_n10000/expected_pred.bin   # golden output
.venv/bin/python python/sweep.py --codes surf_z --ds 5 7 9 11 --ps 0.003 0.005 --shots 1000000 --out results/sweep_1m.csv
```

### 4.6 CPU timing of the same decoder source

```bash
make build/sw/kernel_tb
./build/sw/kernel_tb --time --per-shot 200000 data/long/surf_z_d11_p0.001_n200000
```

### 4.7 The parallel design (report section 5.3)

The graph is fixed at synthesis time, so every step starts by generating the header for
one case. The commands below are for the d = 3 case; for d = 5 use
`tests/par_cases/surf_z_d5_p0.003_n100000`.

1. Generate the graph header and the golden output (any machine):

   ```bash
   C=tests/cases/surf_z_d3_p0.01_n100000
   python3 python/gen_par_graph.py $C/graph.txt                 # writes src/par/par_graph.h
   mkdir -p build/sw
   g++ -std=c++17 -O2 -DUF_MAX_DET=5120 -DUF_MAX_EDGES=28672 tools/par_model.cpp -o build/sw/par_model
   ./build/sw/par_model --local-parity --split-boundary 8 --write-par-expected $C
   ```

   `--split-boundary 8` must match the generator's default; without it the golden
   output follows a different peeling forest and a few shots disagree.

2. Check the kernel source in g++ on every shot:

   ```bash
   g++ -std=c++17 -O2 -DPAR_FUSED -Isrc/par src/par/par_tb.cpp src/par/par_decoder.cpp -o build/sw/par_tb
   ./build/sw/par_tb $C
   ```

   Expected: `100000 shots, 0 mismatches, 0 not OK, ... 30.21 state-machine cycles per
   shot` and `PASS` (d = 5: 46.72 cycles).

3. HLS synthesis and co-simulation (build VM):

   ```bash
   source /fpga/Xilinx/Vitis_HLS/2023.1/settings64.sh
   PAR_CASE=$C PAR_DEFS=-DPAR_FUSED vitis_hls -f hls/run_par.tcl
   ```

4. Bitstream (build VM; about 1.5 hours at d = 3, 3 to 4 hours at d = 5):

   ```bash
   source /fpga/Xilinx/Vitis/2023.1/settings64.sh
   bash hls/build_par_hw.sh par_d3 350          # build/par_d3.xclbin
   ```

   `v++` lowers the clock if timing fails; the clock reached is in the
   `AUTO-FREQ-SCALING` line of the output and in the link summary. The routed bitstreams
   reached 346.5 MHz at d = 3 (requested 350) and 250 MHz at d = 5 (requested 250; at
   300 MHz the d = 5 route failed).

5. Run on the U280 (FPGA node):

   ```bash
   source /opt/xilinx/xrt/setup.sh
   g++ -std=c++17 -O2 -I/opt/xilinx/xrt/include src/host/par_host.cpp -o build/par_host -L/opt/xilinx/xrt/lib -lxrt_coreutil -lpthread
   ./build/par_host build/par_d3.xclbin $C 346.5
   ```

   It checks every shot against `par_expected.bin` and prints the state-machine cycles
   per shot. The FPGA node of the last reservation never detected its card, so this
   step has not been run yet (report section 5.3).
