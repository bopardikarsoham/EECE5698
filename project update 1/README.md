# Hardware-Accelerated Error Decoding for Quantum Computers on the Alveo U280

EECE 5698, Soham Bopardikar. Project Update 1.

> **What this update delivers.** The requirement for Update 1 is to get the application
> running in software, preferably C++. The full decoder runs in C++ (`src/uf_core.h`,
> `src/golden.cpp`). It comes with a testbench, input data and golden outputs (`tests/`),
> and has been checked on 9.5 million simulated runs, from small to large code sizes
> (section 6). The FPGA build flow was already brought up on the U280 with a placeholder
> kernel (section 3.1). Porting this decoder onto the FPGA is the next step (section 7).
>
> **Quickest check:** `make check_all` (needs only `g++` and `make`). Expected last line:
> `check_all: 4 / 4 cases PASS`.

No quantum computing background is assumed. Section 1 explains every term used later,
and the glossary at the end of section 1 can be used as a reference.

## 1. Background in plain terms

### 1.1 The problem: quantum bits are noisy

A quantum computer stores information in *qubits*. Today's qubits are very unreliable:
roughly 1 operation in 100 to 1 in 1000 goes wrong. A useful computation needs millions
of operations, so without protection almost every run would fail.

### 1.2 The fix: error-correcting codes

The classical idea works here too: store one bit of information redundantly across
several physical bits, and check for disagreements. The simplest example is the
**repetition code**: store 0 as `000` and 1 as `111`. If one bit flips (`010`), you can
still tell the original was `000`.

Quantum mechanics adds one restriction: you are not allowed to look at the data qubits
directly, because measuring them destroys the information. Instead, extra helper qubits
repeatedly measure **parity checks**: "are these two (or four) neighbouring data qubits
equal or not?" Each check answers with one bit. In the `000` example, the checks are
"bit 1 = bit 2?" and "bit 2 = bit 3?". For `010` both checks say "not equal". That
pattern points to the middle bit, without ever reading the data itself.

The checks are repeated in **rounds**, about once per microsecond on today's hardware.
A check whose answer *changes* between two rounds is called a **detection event**, or
an *alarm*. The full set of alarms from one experiment is the **syndrome**.

### 1.3 Decoding: working out what went wrong

The **decoder** is a classical program, the part this project builds. It reads the
alarms and decides which errors most likely happened, and from that whether the stored
bit of information got flipped. The decoder's final answer is one bit per run: "the
stored value was flipped" or "it was not". If that answer is wrong, the information is
lost. This is a **logical error**.

This is a graph problem. Every alarm is a vertex. Every possible physical error is an
edge between the alarms it would set off. Each edge has a weight: rarer errors get
higher weights. The decoder has to explain all the alarms using the cheapest set of
edges.

### 1.4 The surface code, distance and threshold

The **surface code** is the code most quantum hardware groups are building towards
(Google, IBM and others). It is a 2D grid version of the parity-check idea above. Its
size is set by the **code distance d** (3, 5, 7, ...). A distance-d code survives any
(d-1)/2 errors, so bigger d means more protection, but also more qubits, more alarms and
more work for the decoder. In this project each experiment runs d rounds of checks, so
the decoder's graph grows roughly as d³ (24 alarm positions at d = 3, 1320 at d = 11).

Two error rates matter:
- the **physical error rate p**: how often an individual qubit or operation fails (the
  input condition, set in the simulator);
- the **logical error rate**: how often the decoder's final answer is wrong (the output
  we measure).

Increasing d helps only if p is below the **threshold**. Below threshold, every step up
in d cuts the logical error rate. Above it, bigger codes make things worse, because there
are more qubits that can fail. Showing a falling logical error rate as d grows is the
standard sign that a decoder works.

### 1.5 Why this needs hardware: the time budget

The hardware produces a new round of alarms about every **1 µs**. The decoder must
process alarms at least that fast. If it is slower, unprocessed rounds pile up and the
quantum computer has to stall until the decoder catches up (Terhal 2015; Battistel et
al. 2023; Skoric et al. 2023, cited in the proposal). It also needs a **guaranteed
worst case**, not just a good average. A CPU cannot promise that, because of caches,
the operating system and other programs. A dedicated hardware pipeline can, because its
cycle count is fixed by design. That is why real-time decoders are being built on FPGAs
and ASICs.

### 1.6 The decoders

- **Minimum-weight perfect matching (MWPM)** finds the cheapest explanation exactly. It
  is the accuracy reference used in research. We use the open-source **PyMatching**
  package for it. MWPM is complex and hard to put into hardware.
- **Union-find** (Delfosse & Nickerson 2021) is a cheaper approximation and is the usual
  choice for hardware decoders. It works like flood fill: a small region grows around
  every alarm, regions that touch merge into one cluster, and a cluster stops growing
  once its alarms can be paired up inside it. A correction is then picked inside each
  cluster. It is slightly less accurate than MWPM, but uses only simple integer
  operations on small arrays, which suits an FPGA. **This project implements union-find.**

### 1.7 Where the data comes from

No quantum hardware is used. **Stim**, an open-source simulator, generates realistic
noisy experiments. For each simulated experiment (a **shot**) it gives the alarms and
also the true answer (whether the stored bit really flipped). Stim also lists every
possible error and its probability, which is exactly the decoder's graph. Because the
true answer is known, we can count the decoder's mistakes exactly.

### Glossary

| Term | Meaning |
|---|---|
| Qubit | Quantum bit; the noisy hardware element being protected |
| Parity check | A measurement of whether neighbouring qubits agree, answering with one bit |
| Round | One repetition of all parity checks, about every 1 µs on real hardware |
| Detection event / alarm | A parity check whose answer changed from the previous round |
| Syndrome | All alarms from one shot; the decoder's input |
| Shot | One simulated experiment: store a bit, run d rounds of checks, read out |
| Decoder | Classical algorithm that reads the alarms and predicts whether the stored bit flipped |
| Logical error | The decoder's prediction is wrong for that shot |
| Logical error rate (LER) | Logical errors / shots; lower is better |
| Physical error rate p | Probability that an individual qubit operation fails; set in the simulator |
| Code distance d | Size of the code; survives (d-1)/2 errors; larger is stronger but more work |
| Threshold | The p below which increasing d lowers the logical error rate |
| Repetition code | Simplest code (store 0 as 000...); used as an easy first test |
| Surface code (rotated, Z memory) | The 2D code used by real hardware; the project's main target |
| Stim | Simulator that generates alarms plus the true answer |
| PyMatching / MWPM | The accurate software decoder used as the accuracy yardstick |
| Union-find | The hardware-friendly decoder implemented in this project |

## 2. The project and its benchmark

**Goal.** Build a union-find decoder for the surface code on an AMD Alveo U280 FPGA
(Open Cloud Testbed, Vitis HLS 2023.1). Measure how accurate it is, how fast it is per
shot, how much of the chip it uses, and how large a code fits. The accelerated part is
purely classical: a graph algorithm on integers.

**The benchmark: what success means.** These targets come from the project proposal.
The *Status* column shows where things stand after Update 1.

| Target | What it means | How it is measured | Status after Update 1 |
|---|---|---|---|
| **1. Correct** | Every correction must actually explain the alarms; the FPGA must give the same answer as the C++ model on every shot | Testbench compares outputs bit for bit; decoder self-check counts invalid corrections | C++ model done: 0 invalid corrections in 9.5 million shots, all test cases pass. FPGA comparison comes after the port |
| **2. Accurate** | Logical error rate close to PyMatching (MWPM) on the same shots, and falling as d grows below threshold | Count logical errors vs Stim's true answers, for both decoders | Done in software for d = 3 to 11, Z and X errors, rotated and unrotated surface codes (sections 6.2, 6.4, 6.5). 0.6 to 2.4% more errors than PyMatching at d = 3 (Z); 30% more at d = 11, p = 0.005 |
| **3. Fast enough** | Keep up with the hardware: at most about **1 µs per round of checks**, with a guaranteed worst case | Decoder time per shot / rounds per shot | The C++ version meets 1 µs/round only at d = 3 (and d = 5 at low p), on average only. Even PyMatching, the fastest CPU decoder, misses it from d = 9 (section 6.3). Meeting this at larger d is the FPGA's job |
| **4. Scale** | Minimum: surface code d = 3, 5, 7 on the U280. Full: largest d that fits and meets timing (aim d = 11), X and Z errors decoded together | Synthesis reports: chip area and achieved clock | Not started on FPGA. Software handles d = 3 to 11 for both X and Z errors, and for the unrotated surface code (graphs up to 2420 vertices) |

The speed target is the reason for the project. Target 3 is about the decoder's own
compute time. The U280 sits behind PCIe, where one round trip already takes
microseconds, so the project measures decoder core latency and does not claim a full
real-time feedback loop.

## 3. What has been done so far

### 3.1 Hardware bring-up on the U280 (done before Update 1)

A first, deliberately trivial kernel was taken through the complete FPGA flow: Stim
data, HLS kernel, `v++` build, bitstream, loading onto the U280, DMA transfer and
readback. **The kernel does not decode.** It copies the low bits of each input word to
the output. Its purpose was to prove that the toolchain, the cloud FPGA node and the
host program all work, so later failures can be blamed on the decoder rather than the
setup.

| Item | Result | Source |
|---|---|---|
| Platform | Alveo U280, `xilinx_u280_gen3x16_xdma_1_202211_1`, Vitis 2023.1 | `v++_qec_decoder.log` |
| Kernel clock | Target 300 MHz; HLS estimated 411 MHz; built at 300 MHz | `_x/reports/.../system_estimate_qec_decoder.xtxt`, `build/qec_decoder.xclbin.info` |
| Chip area used | 2,272 LUTs, 2,086 flip-flops, 4 BRAM, 0 DSP: at most 0.2% of any resource on the U280 (about 1.3 M LUTs, 2.6 M flip-flops, 2,016 BRAM, 9,024 DSP) | `system_estimate_qec_decoder.xtxt` |
| Build time | About 1 h 17 min for compile + link + bitstream | `xcd.log` |
| Hardware run | 1,000 shots of d = 3 surface code data processed on the card in 146 µs | Measured on the U280 in September and reported in the proposal; the console output was not saved in this folder |

**Correction to the proposal.** The proposal reports this run as "24,000 shots". The
input file `simulation/sample_syndromes.bin` actually holds 1,000 shots × 24 alarm bits,
with each bit stored in its own 32-bit word. The host program counted each word as a
shot, so the run was 1,000 shots, or 24,000 individual alarm bits. The 146 µs covers
launching and running the kernel, not copying data to and from the card. Since the
kernel does no decoding, this number says nothing about decoder speed yet.

### 3.2 The decoder in software (Update 1)

This is Layer 1 (sequential, straightforward implementation) and Layer 2 (weighted
edges) of the proposal's plan, in C++:

1. **Data generation (`python/gen_data.py`).** Stim simulates the repetition code or the
   rotated surface code with noise p on every operation. The script writes the decoder's
   graph (`graph.txt`), the alarms for every shot (`dets.bin`) and the true answers
   (`obs.bin`). It also decodes the same shots with PyMatching (`pymatching_pred.bin`).
   Edge weights are small integers from 4 to 16, computed from each error's
   probability, so 8-bit weights on the FPGA are enough.
2. **The decoder (`src/uf_core.h`).** It follows the proposal's steps:
   - **Grow:** every unresolved cluster grows along its edges. An edge is covered once
     the growth reaches its weight.
   - **Merge:** covered edges join clusters. A cluster is resolved once it holds an even
     number of alarms, or touches the edge of the code.
   - **Peel:** inside each cluster, walk a spanning tree from the leaves inwards and pick
     the edges that pair up the alarms.
   - **Output:** whether the picked edges flip the stored bit.

   It also checks itself: the picked edges must reproduce the input alarms exactly
   (otherwise it is an **invalid correction**), and the growth loop has a fixed upper
   limit (reaching it is a **cap hit**). All arrays have fixed sizes, with no dynamic
   memory and no recursion, so the same code can move into Vitis HLS.
3. **The C++ program (`src/golden.cpp`).** It decodes every shot in a case directory,
   writes the predictions, and prints the time taken, invalid corrections and cap hits.
4. **Scoring (`python/score.py`, `python/sweep.py`).** These count logical errors for
   union-find and for PyMatching against the true answers, and run whole grids of d
   and p.

## 4. Files submitted

| File | What it is |
|---|---|
| `src/uf_core.h` | The union-find decoder |
| `src/golden.cpp` | C++ program: decodes a case directory, writes predictions, reports time, invalid corrections and cap hits |
| `tests/check_all.sh` | **Testbench**: runs the decoder on every case in `tests/cases/` and compares with the golden output |
| `tests/cases/*/dets.bin`, `graph.txt` | **Inputs**: alarms for each shot, and the decoder's graph |
| `tests/cases/*/expected_pred.bin` | **Golden outputs**: the expected prediction for each shot |
| `tests/cases/*/obs.bin`, `meta.json` | True answers from the simulator, and the settings used to make the case |
| `tests/freeze.sh` | Adds a new data case to `tests/cases/` (`make freeze`) |
| `python/gen_data.py` | Stim simulation: decoder graph, alarms, true answers, PyMatching predictions |
| `python/score.py` | Logical error rate of union-find vs the true answers and vs PyMatching |
| `python/sweep.py` | Runs simulation, decoding and scoring over a grid of d and p and writes a CSV |
| `python/make_toy.py` | Writes the two toy test cases |
| `python/cpu_latency.py` | Times union-find and PyMatching on the same shots, and reports the growth-iteration tail |
| `results/sweep_d3-11.csv`, `.log` | Results of the distance and error-rate sweep (section 6.2) |
| `results/cpu_latency_d3-11.csv` | CPU speed vs PyMatching and growth-iteration tail (section 6.3) |
| `results/sweep_surf_x_d3-11.csv` | X-memory sweep (section 6.4) |
| `results/wscale_{1,4,8}_p0.005.csv` | Weight-rounding experiment (section 6.6) |
| `results/sweep_unsurf_z_d3-11.csv`, `.log` | Unrotated surface code sweep (section 6.5) |
| `results/extra_runs.log` | Console output of the runs behind sections 6.3, 6.4 and 6.6 |
| `Makefile` | Software targets (`sw`, `score`, `freeze`, `check_all`, `toy`, `golden`) and FPGA targets (`host`, `kernel`) |
| `requirements.txt` | Python package versions |
| `src/kernel/qec_decoder.cpp`, `src/host/host.cpp` | FPGA placeholder kernel and XRT host program (section 3.1) |
| `qec_decoder.xclbin`, `v++_*.log`, `xcd.log` | Bitstream and build logs from the hardware bring-up |
| `simulation/` | Original Stim script and the data file used for the hardware bring-up |

### 4.1 Test cases (inputs and golden outputs)

`make check_all` decodes every case in `tests/cases/`. Each case must match its
`expected_pred.bin` exactly, with 0 invalid corrections and 0 cap hits.

| Case | Code | d | Rounds | p | Shots | Where the golden output comes from |
|---|---|---|---|---|---|---|
| `toy_rep_d3` | repetition code, no measurement noise, all weights 1 | 3 | 1 | every pattern of at most 1 error | 4 | the true answers |
| `toy_rep_d5` | repetition code, no measurement noise, all weights 1 | 5 | 1 | every pattern of at most 2 errors | 16 | the true answers |
| `rep_d3_p0.01_n100000` | Stim repetition code, noise on every operation | 3 | 3 | 0.01 | 100000 | frozen decoder output (regression) |
| `surf_z_d3_p0.01_n100000` | Stim rotated surface code, noise on every operation | 3 | 3 | 0.01 | 100000 | frozen decoder output (regression) |

The two toy cases are small enough to check by hand. A distance-d code can always
correct up to (d-1)/2 errors, so the right answer is known without trusting the decoder.
The two Stim cases are frozen outputs: the FPGA version must later reproduce them bit for
bit, shot by shot (target 1).

## 5. How to run

**Quick check (no Python needed).** Needs `g++` (C++17) and `make`:

```bash
make check_all
```

Expected output: one `PASS` line per case, then `check_all: 4 / 4 cases PASS`.

**Decode and score a case.** Scoring needs the Python packages:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt

make sw    CFG=tests/cases/surf_z_d3_p0.01_n100000   # decode with the C++ decoder
make score CFG=tests/cases/surf_z_d3_p0.01_n100000   # logical errors vs true answers and PyMatching
```

**Regenerate data from scratch** (reproduces section 6; the default seed 5698 gives
identical data):

```bash
.venv/bin/python python/gen_data.py --code surf_z --d 3 --p 0.01 --shots 100000
make sw    CFG=data/surf_z_d3_p0.01_n100000
make score CFG=data/surf_z_d3_p0.01_n100000
make freeze CFG=data/surf_z_d3_p0.01_n100000         # optional: copy into tests/cases/
```

**Full sweep** (section 6.2; takes several minutes, mostly the d = 9 and 11 points):

```bash
make golden
.venv/bin/python python/sweep.py --codes rep surf_z --ds 3 5 7 9 11 --ps 0.001 0.003 0.005 0.01 --shots 100000 --out results/sweep_d3-11.csv
```

**Sections 6.3–6.6** (run after the full sweep, which writes the data they use):

```bash
.venv/bin/python python/cpu_latency.py --out results/cpu_latency_d3-11.csv
.venv/bin/python python/sweep.py --codes surf_x --ds 3 5 7 9 11 --ps 0.001 0.003 0.005 0.01 --shots 100000 --data-root data/sweep_x --out results/sweep_surf_x_d3-11.csv
.venv/bin/python python/sweep.py --codes unsurf_z --ds 3 5 7 9 11 --ps 0.001 0.003 0.005 0.01 --shots 100000 --data-root data/sweep_unsurf --out results/sweep_unsurf_z_d3-11.csv
.venv/bin/python python/sweep.py --codes surf_z --ds 3 5 7 9 11 --ps 0.005 --shots 100000 --wscale 4 --data-root data/ws4 --out results/wscale_4_p0.005.csv
```

(Use `--wscale 1` and `--wscale 8` with matching `--data-root` and `--out` for the other
two weight scales.)

`gen_data.py` options: `--code rep|surf_z|surf_x|unsurf_z`, `--d`, `--p`, `--shots`,
`--rounds` (default d), `--seed` (default 5698), `--wscale` (weight scale, default 2).
`make toy` regenerates the toy cases. The FPGA targets (`make host`, `make kernel`) need
XRT, Vitis and the U280 platform files.

## 6. Results (software)

All runs: 100000 shots per point, d rounds of checks, Stim seed 5698, Apple M3 laptop,
macOS 15.2, Apple clang 16.0.0 (`-O2`), one CPU core. **Across all 95 runs (9.5 million
shots: the 40-point main sweep, 20 X-error points, 20 unrotated-code points and 15 weight-scale points) the decoder
produced 0 invalid corrections and 0 cap hits.** Times are measured wall-clock and vary
slightly between runs.

### 6.1 The two shipped cases (d = 3, p = 0.01)

| Case | Alarm positions | Edges | Shots with any alarm | Union-find logical errors | PyMatching logical errors | Time per shot |
|---|---|---|---|---|---|---|
| `rep_d3_p0.01_n100000` | 8 | 21 | 28327 | 768 / 100000 = 0.007680 | 696 / 100000 = 0.006960 | 0.079 µs |
| `surf_z_d3_p0.01_n100000` | 24 | 78 | 81608 | 5996 / 100000 = 0.059960 | 5963 / 100000 = 0.059630 | 0.732 µs |

At d = 3 union-find makes 0.6% more mistakes than PyMatching on the surface code (5996
vs 5963). That difference is smaller than the statistical uncertainty (±0.000751 on
the error rate). p = 0.01 is *above*
threshold for this noise model (section 6.2), so these two cases are used as test data,
not to show error suppression.

### 6.2 Accuracy vs code size (target 2)

Each cell: **union-find logical errors / PyMatching logical errors**, out of 100000
shots. Lower is better. Full data, including how often the two decoders disagree:
`results/sweep_d3-11.csv`.

Rotated surface code:

| d | Alarm positions | Edges | p = 0.001 | p = 0.003 | p = 0.005 | p = 0.01 |
|---|---|---|---|---|---|---|
| 3 | 24 | 78 | 86 / 84 | 721 / 707 | 1789 / 1749 | 5996 / 5963 |
| 5 | 120 | 502 | 17 / 15 | 385 / 327 | 1513 / 1383 | 8690 / 8367 |
| 7 | 336 | 1558 | 2 / 3 | 155 / 132 | 1126 / 986 | 11217 / 10362 |
| 9 | 720 | 3534 | 0 / 0 | 66 / 52 | 791 / 665 | 14055 / 12506 |
| 11 | 1320 | 6718 | 0 / 0 | 25 / 16 | 565 / 435 | 16482 / 14312 |

Repetition code:

| d | Alarm positions | Edges | p = 0.001 | p = 0.003 | p = 0.005 | p = 0.01 |
|---|---|---|---|---|---|---|
| 3 | 8 | 21 | 9 / 8 | 75 / 76 | 194 / 181 | 768 / 696 |
| 5 | 24 | 65 | 0 / 0 | 5 / 4 | 22 / 18 | 149 / 141 |
| 7 | 48 | 133 | 0 / 0 | 0 / 0 | 1 / 1 | 33 / 36 |
| 9 | 80 | 225 | 0 / 0 | 0 / 0 | 0 / 0 | 7 / 6 |
| 11 | 120 | 341 | 0 / 0 | 0 / 0 | 1 / 1 | 0 / 0 |

How to read this:
- **The decoder works: bigger codes give fewer mistakes below threshold.** For the
  surface code at p = 0.005, union-find's errors fall from 1789 (d = 3) to 565 (d = 11).
  At p = 0.003 they fall from 721 to 25.
- **Threshold.** At p = 0.01 the errors *rise* with d (5996 to 16482), so the threshold
  for this noise model lies between p = 0.005 and p = 0.01. Both decoders agree on this.
- **Accuracy cost of union-find.** At p = 0.005 union-find makes 2.3% more mistakes than
  PyMatching at d = 3 (1789 vs 1749) and 30% more at d = 11 (565 vs 435). Some gap is
  expected, because union-find is an approximation. Section 6.6 tests how much of it
  comes from rounding the edge weights. The answer is only a small part, so most of the
  gap is in the algorithm itself.
- **Cells with only a handful of errors** (for example 2 vs 3, or 0 vs 0) are too small
  to compare. Separating them would need many more shots.

### 6.3 Speed vs the 1 µs per round target (target 3)

Average CPU decode time per round of checks, surface code (µs per shot / d rounds),
computed from `results/sweep_d3-11.csv`. **Bold** = meets the 1 µs/round target on
average.

| d | p = 0.001 | p = 0.003 | p = 0.005 | p = 0.01 |
|---|---|---|---|---|
| 3 | **0.028** | **0.073** | **0.125** | **0.250** |
| 5 | **0.338** | **0.952** | 1.530 | 3.004 |
| 7 | 1.466 | 3.678 | 5.447 | 9.764 |
| 9 | 3.942 | 8.576 | 12.190 | 21.408 |
| 11 | 8.044 | 15.975 | 22.523 | 39.255 |

How to read this:
- The C++ decoder keeps up only for the smallest codes. At d = 11 it is 8 to 39 times
  too slow.
- Time grows quickly with d because each growth step visits every edge one after
  another, and both the number of edges and the number of growth steps grow with d.
  The planned FPGA design processes all vertices in parallel in each clock cycle, so
  this serial loop is exactly what the hardware removes.
- These are **averages**. The target also requires a guaranteed worst case, which a CPU
  cannot give. The tail of the work per shot is shown below.
- The C++ version is the reference model for correctness. It has not been tuned for speed.

**Comparison with PyMatching's speed.** PyMatching is a heavily optimized C++ library
(called from Python), so it shows the best a CPU can realistically do. Both decoders ran
back to back on the same shots, on one core. Full data: `results/cpu_latency_d3-11.csv`.

| Surface code | Union-find µs/shot | PyMatching µs/shot | PyMatching µs/round | PyMatching meets 1 µs/round? |
|---|---|---|---|---|
| d = 3, p = 0.005 | 0.371 | 0.175 | 0.058 | yes |
| d = 5, p = 0.005 | 7.907 | 1.588 | 0.318 | yes |
| d = 7, p = 0.005 | 38.533 | 5.531 | 0.790 | yes |
| d = 9, p = 0.005 | 111.381 | 13.476 | 1.497 | no |
| d = 11, p = 0.005 | 252.572 | 26.698 | 2.427 | no |
| d = 11, p = 0.01 | 435.299 | 81.029 | 7.366 | no |

- On a CPU, PyMatching is 2 to 25 times faster than this union-find code, depending on d
  and p. Union-find is not meant to win on a CPU. Its advantage is that its simple,
  regular operations can be laid out in parallel hardware, which matching cannot easily do.
- **Even the best CPU decoder misses the target at large codes.** On one core, PyMatching
  falls behind 1 µs/round from d = 9 at p = 0.005, and from d = 7 at p = 0.01
  (2.084 µs/round). That is on average, with no worst-case guarantee. This is the case for
  dedicated hardware.

**Worst case: growth iterations per shot.** The number of grow-and-merge iterations
sets how long the hardware takes for a shot, so its tail matters for a guaranteed worst
case. Surface code, from `results/cpu_latency_d3-11.csv`:

| Surface code | Mean | Median | 99% of shots within | 99.9% of shots within | Maximum seen |
|---|---|---|---|---|---|
| d = 3, p = 0.005 | 0.902 | 1 | 4 | 5 | 8 |
| d = 5, p = 0.005 | 3.858 | 4 | 10 | 13 | 16 |
| d = 7, p = 0.005 | 6.928 | 6 | 14 | 18 | 24 |
| d = 9, p = 0.005 | 9.239 | 9 | 17 | 22 | 29 |
| d = 11, p = 0.005 | 11.260 | 11 | 20 | 25 | 32 |
| d = 11, p = 0.01 | 14.960 | 15 | 24 | 28 | 33 |

- The worst shot needs about 3 times the average number of iterations (32 vs 11.26 at
  d = 11). A hardware design has to budget for the worst case, or for a cap whose hit
  rate is measured, as the proposal plans.
- The maximum grows slowly with d (8 at d = 3, 32 at d = 11). This supports the
  proposal's expectation that hardware latency grows roughly linearly with d, while
  chip area grows much faster.

### 6.4 X-type errors (rotated surface code, X memory)

The surface code has to catch two kinds of errors, called X and Z. Stim simulates each
in a separate experiment, and the proposal plans two decoder copies, one per type. The
results above are for Z memory (`surf_z`). The same sweep for X memory (`surf_x`), as
union-find / PyMatching logical errors out of 100000 shots. Full data:
`results/sweep_surf_x_d3-11.csv`.

| d | p = 0.001 | p = 0.003 | p = 0.005 | p = 0.01 |
|---|---|---|---|---|
| 3 | 88 / 87 | 770 / 739 | 2003 / 1921 | 6692 / 6578 |
| 5 | 16 / 14 | 436 / 400 | 1786 / 1621 | 9994 / 9597 |
| 7 | 5 / 3 | 193 / 173 | 1418 / 1288 | 13306 / 12439 |
| 9 | 0 / 0 | 81 / 68 | 1094 / 916 | 16911 / 15388 |
| 11 | 0 / 0 | 51 / 36 | 801 / 614 | 20958 / 18722 |

The same decoder handles X memory with no changes: 0 invalid corrections, error rates
falling with d at p ≤ 0.005 and rising at p = 0.01. The rates are a little higher than
for Z memory, and union-find's gap to PyMatching is similar (31% more errors at d = 11,
p = 0.005: 801 vs 614).

### 6.5 A second code: the unrotated surface code

The proposal also covers other error-correcting codes. Besides the repetition code
(section 6.2), the decoder was run on the **unrotated surface code** (`unsurf_z`). This
is the original layout of the surface code. The rotated layout used everywhere else in
this report is a later, more compact version of the same idea. At the same distance d,
both codes survive the same number of errors. The unrotated one uses more qubits:
(2d-1)² instead of 2d²-1, which is 441 instead of 241 at d = 11. Nothing in the
decoder changed, except the fixed array limits in `uf_core.h`. Those were raised to 4096
vertices and 16384 edges, because the d = 11 graph has 2420 vertices and 13018 edges.

Union-find / PyMatching logical errors out of 100000 shots, Z memory. Full data:
`results/sweep_unsurf_z_d3-11.csv`.

| d | p = 0.001 | p = 0.003 | p = 0.005 | p = 0.01 |
|---|---|---|---|---|
| 3 | 100 / 94 | 938 / 922 | 2514 / 2433 | 8429 / 8299 |
| 5 | 14 / 11 | 372 / 357 | 1717 / 1607 | 11822 / 10935 |
| 7 | 1 / 2 | 126 / 116 | 1145 / 959 | 15178 / 13134 |
| 9 | 0 / 0 | 43 / 29 | 704 / 575 | 19478 / 16485 |
| 11 | 0 / 0 | 10 / 7 | 491 / 375 | 23465 / 19421 |

The two layouts side by side at p = 0.005:

| d | Graph vertices (rotated / unrotated) | Union-find errors (rotated / unrotated) | Gap to PyMatching (rotated / unrotated) | Worst-case growth iterations (rotated / unrotated) | CPU µs per shot (rotated / unrotated) |
|---|---|---|---|---|---|
| 3 | 24 / 36 | 1789 / 2514 | +2.3% / +3.3% | 8 / 8 | 0.374 / 0.883 |
| 7 | 336 / 588 | 1126 / 1145 | +14.2% / +19.4% | 24 / 26 | 38.128 / 84.637 |
| 11 | 1320 / 2420 | 565 / 491 | +29.9% / +30.9% | 32 / 40 | 247.756 / 557.014 |

- **The decoder works on a second code unchanged.** It made 0 invalid corrections and 0
  cap hits. Errors fall with d at p ≤ 0.005 and rise at p = 0.01, the same threshold
  behaviour as the rotated code. The gap to PyMatching is about the same size.
- **The rotated layout is the better hardware target.** At d = 11 the unrotated code
  makes about 13% fewer logical errors, because it has more qubits. In exchange, the
  decoder's graph is 1.8 times larger and the worst case is 40 growth iterations
  instead of 32. For the FPGA, that means more chip area and a longer worst-case
  latency for a small accuracy gain, so the hardware work stays with the rotated code.

### 6.6 Experiment: does finer weight rounding close the gap?

Edge weights are integers: `round(scale × ln((1-p)/p))` per edge. All results above use
scale 2. Here the Z-memory surface code at p = 0.005 was rerun with scales 1, 4 and 8 on
the same shots. Each cell: union-find logical errors (percent more than PyMatching).
PyMatching uses exact weights, so its numbers do not change. Full data:
`results/wscale_{1,4,8}_p0.005.csv`.

| d | PyMatching | Scale 1 (weights 3–7) | Scale 2, default (6–13) | Scale 4 (11–26) | Scale 8 (22–53) |
|---|---|---|---|---|---|
| 3 | 1749 | 2038 (+16.5%) | 1789 (+2.3%) | 1773 (+1.4%) | 1759 (+0.6%) |
| 5 | 1383 | 1607 (+16.2%) | 1513 (+9.4%) | 1510 (+9.2%) | 1501 (+8.5%) |
| 7 | 986 | 1200 (+21.7%) | 1126 (+14.2%) | 1113 (+12.9%) | 1114 (+13.0%) |
| 9 | 665 | 848 (+27.5%) | 791 (+18.9%) | 771 (+15.9%) | 772 (+16.1%) |
| 11 | 435 | 610 (+40.2%) | 565 (+29.9%) | 547 (+25.7%) | 547 (+25.7%) |

(Weight ranges are the d = 11 graph's.)

- Coarser weights (scale 1) clearly hurt. Going finer than scale 2 helps a little up to
  scale 4, then stops helping: at d = 11 the gap shrinks from 30% to 26% and stays there.
- So **most of the gap at large d comes from the union-find algorithm itself**, not from
  rounding. The next thing to try is growing the smallest cluster first, as in the
  original paper (section 7).
- Scale 4 still fits 8-bit hardware counters (weights up to 26, so growth up to 52).
  It costs more growth iterations, though: CPU time at d = 11 rose from 247.756 to
  406.315 µs/shot. The shipped test cases keep scale 2.

### 6.7 What this means for the FPGA plan

- **Graph size.** With d rounds of checks, the surface code graph has 336 vertices at
  d = 7 and 1320 at d = 11. That is about twice the proposal's estimate (~190 and ~720),
  so a design with one processing element per vertex runs out of chip area sooner than
  planned. d = 3 to 7 is the realistic range for that design; d = 9 to 11 is a stretch.
- **Data widths.** Edge weights stay between 4 and 16, so the proposal's 8-bit weights
  and counters are enough. Vertex counts fit in 16-bit IDs.

## 7. Plan for the rest of the semester

1. **HLS port (sequential).** Replace the placeholder in `src/kernel/qec_decoder.cpp`
   with the decoder from `uf_core.h`, with the decoder's graph in on-chip memory. Change
   the input format to one packed bit-vector per shot (fixing the one-word-per-bit
   format from section 3.1). Add a C++ HLS testbench that runs Vitis C simulation on
   `tests/cases/` and must match `expected_pred.bin` bit for bit (target 1).
2. **Run on the U280.** Update `src/host/host.cpp` to send the shots and compare the
   results with the golden outputs on the card. Report per-shot cycle counts.
3. **Parallel design (targets 3 and 4).** One processing element per vertex, with arrays
   partitioned so all vertices update in the same clock cycle, and the serial `find()`
   loop replaced by minimum-ID propagation. Report worst-case cycles per shot, chip area
   and achieved clock for d = 3, 5, 7, then larger d while it fits.
4. **Accuracy gap.** Weight rounding has been tested (section 6.6) and explains only a
   small part of the gap to PyMatching. Next, try growing the smallest cluster first,
   and decide whether scale 4 weights are worth their extra iterations in hardware.
5. **Final report.** Accuracy vs PyMatching, CPU vs FPGA speed against the 1 µs/round
   target, chip area vs d, and what limits the FPGA design.

## 8. Use of AI

I used Claude in Project Update 1. It helped me with setup, debugging, explaining
concepts and checking my work. I planned the designs, wrote the observations, and
decided what to try.
