# Project Update 2: A Union-Find Quantum Error Decoder on the Alveo U280

EECE 5698, Soham Bopardikar.

## 1. The project

A quantum computer stores information redundantly in a **surface code** and measures
parity checks once per **round**, about every 1 µs. A classical **decoder** reads the
checks that fired (the **syndrome**) and works out which errors most likely happened.
It has to keep up with the hardware: about **1 µs of decoding per round**, with a
bounded worst case. Otherwise unprocessed rounds pile up [2, 3, 4].

This project builds a **weighted union-find decoder** [5, 6] for the rotated surface
code as a Vitis HLS kernel on an **AMD Alveo U280**, on the Open Cloud Testbed. It
measures four things, as set out in the proposal:

1. **Correct:** the FPGA output equals the C++ reference decoder bit for bit, on every shot.
2. **Accurate:** the logical error rate is close to minimum-weight matching (PyMatching [7, 8]).
3. **Fast enough:** mean, tail and maximum latency per shot and per round, against the 1 µs/round budget.
4. **Scale:** chip area and achieved clock, and the largest distance d that fits.

Data come from the Stim simulator [9]: circuit-level noise with probability p on every
operation, d rounds of checks, Z-memory experiments. At d = 11 the decoder's graph has
1320 detectors and 6718 edges.

## 2. What was done for this update

Update 1 delivered the decoder in C++ (`src/uf_core.h`) with test cases and golden
outputs. For Update 2 the decoder runs **on the U280**: it was ported to HLS, checked
against the golden outputs on the card, and then made 334× faster through nine
versions, each measured.

### 2.1 The FPGA kernel and the host program

- **Kernel (`src/kernel/qec_decoder.cpp`, `src/uf_fast.h`).**
  - The host loads the decoder graph into on-chip memory once.
  - Shots then stream in as packed bit vectors, one bit per detector, read in blocks with burst AXI transfers.
  - For each shot the kernel writes one prediction bit, plus a 12-word **info record**: status, work, and the number of loop iterations each decoder stage took on the card (section 4).
  - The decoder uses only fixed-size arrays, with no dynamic memory or recursion.
  - The workspace is cleaned incrementally after every shot, touching only what that shot used, so no full reset is needed between shots.
- **Host (`src/host/host.cpp`).**
  - XRT program: loads the bitstream, sends the graph and the shots, runs the kernel, reads the results back.
  - Compares every shot with `expected_pred.bin` and prints PASS or FAIL.
  - Writes one CSV row per shot with the on-card counters.
- **Testbench (`src/kernel/tb.cpp`).**
  - Runs the same kernel source compiled with g++, in Vitis C simulation, and in C/RTL co-simulation.
  - Checks every shot against the golden output.

### 2.2 Making it fast: v1 to v9

Each version was synthesized and checked bit-exact against the golden decoder. v1 to v5
were also built into bitstreams and run on the card. Times are for the d = 11, p = 0.001
surface code (11 rounds per shot), measured on the U280 at 300 MHz:

| Version | Main change | µs per shot | µs per round |
|---|---|---|---|
| v1 | First working kernel: direct HLS port of the C++ decoder | 2499 | 227 |
| v2 | Grow only near the active clusters instead of scanning the whole graph; reset only what the shot touched | 49.0 | 4.45 |
| v3 | One edge per clock in the main growth loop (fixed edge slots per detector); no root search (parent points straight at the root) | 20.8 | 1.89 |
| v4 | One-cycle-per-detector bit unpacking; inputs and outputs moved 64 shots at a time (burst I/O) | 19.0 | 1.73 |
| v5 | Three slow loops brought to II = 1 (growth step, compaction, cleanup); short sorted list of grown boundary edges for peeling | 13.4 | 1.22 |
| v6 | v5 plus the on-board per-stage loop counters (section 4) | 13.4 | 1.22 |
| v9c | 12-lane frontier, 2-edge growth step, one-loop peeling search | 7.47 | 0.68 |
| **v9c, 348.5 MHz** | same source, built at 350 MHz (section 3.4) | **6.87** | **0.62** |

v5 and v6 are from the 10,000-shot case; v9c is from the 200,000-shot case (7.57 µs on the
10,000-shot case). All rows except the last are at 300 MHz.

v1 → v9c is **334× faster** on the same card and clock, and v9c is 1.8× faster than v5
(364× and 2.0× with the 348.5 MHz build).
At d = 11, p = 0.001 its mean is **under the 1 µs/round budget** (section 5.1). Section 3 explains the
techniques. v7 and v8 were synthesis experiments that were never built into bitstreams
(section 3.3).

### 2.3 Verification

| Check | Scope | Result |
|---|---|---|
| Card output vs golden, v5, v6 and v9c | 40 cases (d = 3 to 11, p = 0.001 to 0.01, repetition and surface code, 10,000 shots each) + 3 cases of 200,000 shots | 43 / 43 PASS for each version, every shot identical |
| Kernel source vs golden (g++), v9c | 47 cases, 4.8 million shots | 47 / 47 PASS |
| Stage counters v9c vs v8 vs v7 vs v6 | same 4.8 million shots | identical shot by shot, apart from the counters a restructured loop defines differently |
| C/RTL co-simulation, v6 to v9c | 200 shots, d = 3 | PASS for every version |
| Workspace left clean after every shot | 1.6 million shots, with a checker that inspects every array | 0 dirty shots (the checker itself was tested by injecting a bug) |
| Invalid corrections, cap hits | all runs | 0 |

### 2.4 Accuracy against PyMatching (1 million shots per point)

Ratio of union-find logical errors to PyMatching logical errors, from
`results/sweep_1m.csv`:

| d | p = 0.003 | p = 0.005 |
|---|---|---|
| 5 | 1.09 | 1.08 |
| 7 | 1.11 | 1.13 |
| 9 | 1.18 | 1.19 |
| 11 | 1.19 | 1.26 |

Union-find makes 8–26% more logical errors than exact matching, which is the expected
cost of the approximation. The gap grows slowly with d. Logical errors fall as d grows
at both p, so the decoder is below threshold there. In Update 1 the d = 11 gap appeared
to be 1.56×; with 10× more shots it turns out to be 1.19 ± 0.11. The earlier number was
statistical noise.

### 2.5 The FPGA against a CPU

The same decoder source, compiled with g++ -O2 on an Apple M3 (one core, graph loaded
first, decode time only) (`results/cpu_baseline_m3.txt`):

| Case | CPU mean | CPU p99.9 | U280 v5 mean | U280 v9c mean | U280 v9c p99.9 |
|---|---|---|---|---|---|
| d = 7, p = 0.001 | 0.83 µs | 6.6 µs | 3.01 µs | 1.94 µs | 10.7 µs |
| d = 11, p = 0.001 | 4.28 µs | 15.5 µs | 13.4 µs | 7.47 µs | 20.8 µs |
| d = 11, p = 0.003 | 15.0 µs | 49.1 µs | 39.8 µs | 20.9 µs | 50.5 µs |

(v9c's mean is measured on the 200,000-shot cases; its p99.9 is estimated from each
shot's work, section 6.) A 3.5 GHz laptop core was 2.7–3.6× faster than v5 at 300 MHz
on the mean; against v9c the gap is down to 1.4–2.3×, and at d = 11, p = 0.003 the tails
are about equal. The FPGA's strength is predictability: its time per shot is fixed by
the shot's work, and the CPU's maximum varied from run to run. Closing the rest of the
gap is the purpose of the plan in section 7.

## 3. How the speed was gained

The decoder is a sequence of short loops per growth iteration: member lists, frontier,
growth, merges, compaction, then peeling and cleanup. The time per shot is

    cycles ≈ Σ over loops (iterations × II) + Σ over loop starts (pipeline depth) + control overhead

so there are three levers: fewer iterations, lower II, and fewer loop starts.

### 3.1 Fewer iterations (v2 to v4)

- **Work proportional to the shot, not to the graph.** v1 scanned all 6718 edges in every growth step. From v2 on, each cluster keeps a linked list of its members, and only the active clusters' members and their edges (the frontier) are visited.
- **Incremental reset.** After a shot, only the nodes and edges that the shot touched are cleared.
- **No root search.** `parent[]` always points straight at the cluster root (a merge relabels the smaller cluster), so no loop has to search for roots.
- **Fixed edge slots.** Every detector's edges sit in 12 fixed slots, so the frontier scan is a plain loop nest that HLS pipelines at one edge per clock (v3).
- **Cheaper I/O (v4).** Detector bits are unpacked at one detector per cycle, and shots are read and written 64 at a time in bursts.

### 3.2 Lower II (v3 to v5)

- `#pragma HLS PIPELINE II=1` on every inner loop.
- `#pragma HLS DEPENDENCE ... inter false`, only where it is provably safe. For example, the growth step writes `support[e]` for distinct edges, since every edge is in the frontier at most once.
- Read-modify-write recurrences restructured. For example, cluster compaction became two passes: one writes the owner positions backwards, the second only reads them.

### 3.3 Parallel lanes (v7 to v9c)

- **v7 and v8 (synthesis only).**
  - Every detector's 12 edge slots are read in the same clock cycle. The slot table is partitioned so each slot is its own memory (`ARRAY_PARTITION dim=2 complete`).
  - The state those lanes read (support, parent, active) is kept in 6 identical copies, each a true dual-port RAM (`BIND_STORAGE ram_t2p`). That gives 12 independent read ports, one per lane. Every write updates all 6 copies at once.
  - Synthesis showed this did not reach II = 1. The frontier loop came out at **II = 12**: its 12 lanes appended to one list at positions known only at run time, so HLS serialized the writes. v8's co-simulation took more cycles than v6's.
- **v9 / v9c.**
  - **Frontier banks.** The frontier list is split into 12 banks, one memory each. Every bank gets exactly one write per step, at a bank fixed in hardware, and a small multiplexer picks which lane's edge goes there. Result: frontier **II 12 → 1**.
  - **Growth step.** It takes two edges per step. The append-only lists it writes get `DEPENDENCE inter false`, and their counters are updated with one add each. Result: **II 2 → 1**.
  - **Peeling search.** Each queued node's grown-edge mask is computed when it is queued, and `grown_mask` is inlined. Result: **II 8 → 6**. The remaining II comes from a real dependence: a node queued in one step can be taken from the queue in the next.
  - **Clock fix.** v9 first had a 13.7 ns combinational path: a 12-deep chain of compares, on a value carried from one step to the next, inside an II = 1 loop. v9c computes that minimum as a tree within the step, which brings the estimate back to 2.85 ns, the same as v8.
  - **The cost** is area: the replicated state and the lane multiplexers roughly double the LUT estimate (v6 27.8k → v9c 59.7k LUTs, about 4.6% of the U280's 1.3M) and double the BRAM.

C/RTL co-simulation, the same 200 d = 3 shots in one call (`results/hw/synth_v7_v9/`):

| Version | Co-simulation cycles | vs v6 | HLS clock estimate | LUT / FF / BRAM_18K / DSP / URAM (HLS estimate) |
|---|---|---|---|---|
| v6 | 84,589 | — | 2.47 ns | 27,848 / 16,872 / 183 / 6 / 8 |
| v7 | 88,441 | +4.6% | 2.55 ns | 66,103 / 51,071 / 377 / 10 / 7 |
| v8 | 96,481 | +14.1% | 2.85 ns | 72,319 / 57,370 / 377 / 14 / 10 |
| v9b | 61,681 | −27.1% | 13.67 ns (fails timing) | 57,876 / 20,686 / 381 / 6 / 10 |
| **v9c** | **63,865** | **−24.5%** | 2.85 ns | 59,654 / 26,664 / 381 / 6 / 10 |

d = 3 shots are small, so these cycle counts are dominated by fixed per-loop costs.
Section 5.1 gives the d = 11 numbers measured on the card.

### 3.4 Clock

v8 was synthesized at 300, 350 and 400 MHz (3.33, 2.86 and 2.5 ns), and v9 at 300 and
350 MHz. The lane designs' critical path stays at 2.85 ns whatever the target, so
**about 350 MHz is the ceiling** for them without deeper restructuring, and 400 MHz is
out of reach. The tighter targets also raise some loops' II (v8's peeling search goes
from II 7 to 9 at 2.5 ns). v6's path is shorter, at 2.47 ns. Every bitstream so far was built at
300 MHz, and all closed timing: worst negative slack +0.016 ns (v1), +0.006 (v2),
+0.014 (v3), +0.001 (v4), +0.011 (v5), +0.007 (v6) and +0.016 (v9c).

v9c was also built at 350 MHz. Routing missed by 0.012 ns on one path, and `v++`
lowered the kernel clock automatically to **348.5 MHz**, where the bitstream meets
timing. It passes all 43 cases on the card. At d = 11, p = 0.001 it takes 6.87 µs per
shot (0.62 µs per round), 8% less than at 300 MHz rather than the 14% the clock alone
would give. The likely reason is that the tighter HLS target deepened some loop
pipelines or raised their II. Routed: 21,930 LUTs, 208 BRAM, the same as at 300 MHz.

## 4. Showing the computation on the FPGA: on-board stage counters

To show where the time goes *on the card*, from v6 on the kernel counts the loop
iterations of each decoder stage for every shot. It returns them with the prediction,
in the info record (`src/info_layout.h`):

| Word | Name | Meaning |
|---|---|---|
| 0 | status | (status << 16) \| growth iterations; status 0 OK, 1 cap hit, 2 invalid correction |
| 1 | work | total inner-loop iterations of the shot |
| 2 | defects | detectors that fired |
| 3–11 | members, frontier, apply, merges, relabel, compact, peel_scan, peel_walk, cleanup | iterations of each stage's loop |

Each counter is added once per loop, outside the loop body, so counting costs no cycles
inside the pipelines. `python/stage_breakdown.py` multiplies each count by that loop's
II, adds a pipeline fill per loop start (II and depth from the HLS synthesis log), and
compares the total with the time measured on the card. For v5 at d = 11, p = 0.001,
the stages account for 2,979 of the 4,008 measured cycles (74%). Frontier (808),
peeling search (712) and growth (613) dominate. The remaining 26% is control overhead
between loops, which HLS does not report per loop. v6, which has the counters on
the card, measures the same: 2,981 of 4,023 cycles (74%). For v9c see section 5.1.

## 5. Input, output and how to read them

**Inputs (one directory per case):**

- `graph.txt`: the decoder graph from Stim's detector error model. One line per edge: endpoints, integer weight (4 to 16) and whether the edge flips the logical observable. The boundary is a virtual node.
- `dets.bin`: one packed bit vector per shot, one bit per detector, set when that detector fired.
- `obs.bin`: the true logical flip for each shot, from Stim. Used only for scoring.
- `pymatching_pred.bin`: PyMatching's predictions on the same shots. Used only for scoring.
- `expected_pred.bin`: **the golden output**, the reference decoder's prediction for every shot.

**Outputs:**

- **Predictions**, one bit per shot: does the decoder think the logical qubit flipped? The host compares them with `expected_pred.bin`.
- **Console output:**
  - `Kernel time: T us for N shots` is the measured kernel time on the card, launch included, for one call that decodes all N shots.
  - `Per shot, counted on the card: ...` gives the mean of each stage counter.
  - `PASS: FPGA output vs golden` means every one of the N predictions equals the golden output.
  - A failing run prints the number of shots that differ.
- **Per-shot CSV** (`results/work_<tag>/<case>.csv`): one row per shot with the growth iterations, the work and the stage counters.

**Scoring:** the logical error rate is the fraction of shots where the prediction differs
from `obs.bin`, computed the same way for PyMatching's predictions.

### 5.1 Hardware results for this update

v6 and v9c were built into bitstreams at 300 MHz and run on the U280: the 40-case sweep
(10,000 shots each) plus the three 200,000-shot cases. **Both pass 43 / 43**: every
prediction from the card equals the golden output. Logs and per-shot CSVs are in
`results/hw/bit_v6_v9c/`.

**Build results (routed, kernel only):**

| Version | WNS at 300 MHz | LUT | Registers | BRAM | URAM | DSP |
|---|---|---|---|---|---|---|
| v6 | +0.007 ns | 12,104 (1.03%) | 13,551 | 114 (6.3%) | 1 | 6 |
| v9c | +0.016 ns | 22,033 (1.87%) | 18,214 | 208 (11.4%) | 1 | 6 |

After routing the design is much smaller than the HLS estimates in section 3.3. v9c
uses under 2% of the chip's LUTs; BRAM, at 11%, is what limits how many copies fit.

**Surface code, time per shot measured on the card** (`results/hw/hw_sweep_v5.csv`,
`results/hw/bit_v6_v9c/hw_v6.csv`, `hw_v9c.csv`). Per round = per shot / d. The p99 and
p99.9 columns are estimated from each shot's work × the case's measured cycles per unit
of work:

| d | p | v5 µs/shot | v6 µs/shot | **v9c µs/shot** | v9c µs/round | v9c vs v5 | v9c p99 µs | v9c p99.9 µs |
|---|---|---|---|---|---|---|---|---|
| 3 | 0.001 | 0.21 | 0.25 | **0.22** | 0.07 | 0.97× | 2.3 | 3.8 |
| 5 | 0.001 | 1.00 | 1.04 | **0.75** | 0.15 | 1.34× | 4.3 | 7.3 |
| 7 | 0.001 | 3.03 | 3.04 | **1.96** | 0.28 | 1.54× | 7.1 | 11.3 |
| 9 | 0.001 | 6.95 | 6.95 | **4.12** | 0.46 | 1.69× | 11.0 | 16.8 |
| 11 | 0.001 | 13.45 | 13.41 | **7.57** | 0.69 | 1.78× | 16.0 | 21.4 |
| 3 | 0.003 | 0.49 | 0.53 | **0.42** | 0.14 | 1.17× | 2.8 | 4.7 |
| 5 | 0.003 | 2.88 | 2.89 | **1.86** | 0.37 | 1.55× | 7.1 | 10.9 |
| 7 | 0.003 | 9.29 | 9.27 | **5.30** | 0.76 | 1.75× | 14.5 | 20.2 |
| 9 | 0.003 | 21.03 | 20.95 | **11.37** | 1.26 | 1.85× | 24.2 | 33.0 |
| 11 | 0.003 | 39.82 | 39.64 | **20.95** | 1.90 | 1.90× | 38.6 | 50.5 |

The full table, including p = 0.005, p = 0.01 and the repetition code, is in the CSVs.

How to read it:

- **v6 equals v5**, as it should: the counters cost no time.
- **v9c gains 1.5–1.9× from d = 5 up**, more at larger d and p, where the per-step loops it parallelized dominate. At d = 3 the shots are too small to gain.
- **Against the 1 µs/round budget, the mean is met** at p = 0.001 for every d up to 11 (0.69 µs/round at d = 11) and at p = 0.003 up to d = 7.
- **The tail is not met yet.** At d = 11, p = 0.001 the p99.9 estimate is about 1.9 µs/round. At p ≥ 0.003 and d ≥ 9 even the mean is over budget.

**Where v9c's time goes, measured on the card** (d = 11, p = 0.001, 200,000 shots,
`python/stage_breakdown.py --version v9c`):

| Stage | Cycles/shot | Share |
|---|---|---|
| peeling search (II 6) | 473 | 25% |
| growth step | 342 | 18% |
| member lists (II 2) | 255 | 14% |
| peel walk | 188 | 10% |
| compaction | 179 | 10% |
| cleanup | 164 | 9% |
| frontier (12 lanes) | 130 | 7% |
| merges + relabel | 127 | 7% |
| unpack | 24 | 1% |
| **total of the loops** | **1,883** | |
| **measured on the card** | **2,241** (7.47 µs) | loops = 84% |

Compared with v5/v6 (4,023 measured cycles):

- **The frontier fell from 809 to 130 cycles** and the growth step from 613 to 342.
- **Control overhead between loops fell from about 1,040 to 360 cycles**, because v9c has fewer, longer loops.
- **The peeling search is now the largest stage.** Its II of 6 comes from a real dependence, so it is the next target (section 7).

At d = 11, p = 0.003 the loops account for 5,634 of 6,270 measured cycles (90%).

### 5.2 Larger codes (d = 13 to 17) and the published FPGA decoders

The 300 MHz kernel takes up to 2048 detectors, enough for d = 11. For d = 13 to 17 (up to
4,896 detectors and 26,398 edges) v9c was rebuilt with limits of 5,120 detectors and
28,672 edges. BRAM rose from 208 to 691 blocks (38%). Routing then missed 300 MHz
(WNS −1.135 ns), and `v++` lowered the clock to **223.8 MHz**. LUTs are unchanged
(22,559, 1.9%).

**All 18 cases pass**: d = 13, 15 and 17 at p = 0.001, 0.003, 0.005 and 0.01 (10,000
shots), two 200,000-shot cases, and four 100,000-shot cases under a second noise model
(below). Every prediction equals the golden output (`results/hw/bit_v6_v9c/hw_v9c_big.csv`).

| d | p | µs/shot at 223.8 MHz | µs/round, scaled to 300 MHz | p99.9 µs/round, scaled |
|---|---|---|---|---|
| 13 | 0.001 | 16.90 | 0.97 | 2.17 |
| 15 | 0.001 | 26.12 | 1.30 | 2.48 |
| 17 | 0.001 | 37.29 | 1.64 | 2.85 |
| 13 | 0.003 | 47.81 | 2.74 | 5.65 |
| 17 | 0.003 | 107.87 | 4.73 | 7.89 |
| 17 | 0.01 | 472.54 | 20.74 | 28.31 |

"Scaled" multiplies by 223.8/300. That slightly flatters the scaled numbers, since memory
latency does not shrink with the clock. Time per shot grows about as d³ at fixed p:
the decoder handles defects and edges one at a time, and both grow with d² × d rounds.

**Against Collision Clustering [13].** CC reports mean time per round under a milder
circuit noise: two-qubit gates and measurements at p, single-qubit gates, resets and
idling at p/10. `python/gen_data.py --noise cc` generates it. At p = 0.001:

| d | Ours (v9c), µs/round | CC [13], µs/round | Gap | Ours, LUTs | CC, LUTs |
|---|---|---|---|---|---|
| 11 | 0.49 (300 MHz bitstream) | 0.11 | 4.5× | 22,033 | 4,600 |
| 13 | 0.69 (scaled) | 0.16 | 4.3× | 22,559 | 5,914 |
| 15 | 0.91 (scaled) | 0.25 | 3.6× | 22,559 | 7,793 |
| 17 | 1.16 (scaled) | 0.37 | 3.1× | 22,559 | 10,446 |

We are 3–4.5× slower. The gap narrows with d, because CC's time also grows faster than
linearly. CC builds its graph into the hardware for one d; ours is loaded at run time,
so one bitstream decodes every d up to 17 and any noise model.

**Against Helios [12] and Micro Blossom [14].** Helios reports 23.7 ns per round at
d = 17. Micro Blossom decodes a whole d = 13 block in 0.8 µs; ours takes 16.9 µs at 223.8 MHz (about 12.6 µs scaled to 300 MHz). Both
build one processing unit per node and per edge (Helios 340,084 LUTs at d = 13, Micro
Blossom 553,000). A decoder that handles defects one at a time cannot reach them. That
is why the parallel design (section 5.3) is the main item in the plan.

### 5.3 First step to the parallel design (HLS prototype)

`src/par/` is a prototype of the parallel design from the proposal. It has one unit
per node and per edge, and the graph is wired in at synthesis time by
`python/gen_par_graph.py`. Each node holds a cluster label (the smallest node ID in its
cluster), and labels flood along grown edges. Growth is the same weighted growth as the
reference, so the clusters are the same.

- **Cluster parity.** It is passed up a forest rooted at each label and back down,
  using only wires between neighbours.
- **Boundary.** It is split into virtual nodes of 8 edges each, so no node has more than
  12 inputs.
- **Accuracy.** Predictions differ from the reference only in which spanning forest is
  peeled, and the logical error rate is the same: for example 369 vs 385 errors at
  d = 5, p = 0.003, and 16,534 vs 16,482 at d = 11, p = 0.01, on the same shots.
  Its golden output, `par_expected.bin`, comes from `tools/par_model.cpp`.

Vitis HLS 2023.1, C simulation on 100,000 shots and C/RTL co-simulation on 200, all PASS
(reports in `results/par/`). Cycles per shot are from co-simulation and include reading
the detectors and writing the results:

| Version | d | p | Cycles/shot | Clock estimate | µs/shot | v9c on the card | LUT (HLS estimate) |
|---|---|---|---|---|---|---|---|
| five loops per iteration | 3 | 0.01 | 67.2 | 169 MHz | 0.40 | 1.04 | 56,401 |
| five loops per iteration | 5 | 0.003 | 114.4 | 153 MHz | 0.75 | 1.86 | 269,130 |
| one state machine | 3 | 0.01 | 36.4 | 212 MHz | 0.17 | 1.04 | 39,628 |
| one state machine | 5 | 0.003 | 58.3 | 210 MHz | 0.28 | 1.86 | 226,948 |
| **one state machine, routed bitstream** | 3 | 0.01 | 36.4 | **300 MHz (met)** | **0.12** | 1.04 | **10,739 (routed)** |
| **one state machine, routed bitstream** | 5 | 0.003 | 58.3 | **213.7 MHz** | **0.27** | 1.86 | **52,628 (routed)** |
| **registered level OR, routed bitstreams** | 3 | 0.01 | (30.2 state machine) | **346.5 MHz** | **0.087** (state machine) | 1.04 | **10,465 (routed)** |
| **registered level OR, routed bitstreams** | 5 | 0.003 | (46.7 state machine) | **250 MHz (met)** | **0.19** (state machine) | 1.86 | **52,456 (routed)** |

**Bitstream (d = 3).** `hls/build_par_hw.sh` builds the state-machine version for the
U280 (`results/hw/par_d3/`). It meets timing at the requested 300 MHz (worst slack
+0.003 ns; `v++` reports 311.9 MHz achievable), and the routed kernel uses 10,739 LUTs
(0.91% of the chip), 5,615 registers, 2 BRAM and no DSPs. The HLS estimates were 3.7×
too high on LUTs and 30% too low on the clock. At 300 MHz the co-simulation cycles give
0.12 µs per shot including I/O, and the state machine alone 27.7 cycles = 0.092 µs:
**8.6× faster than v9c**, in fewer LUTs than v9c (22,559). For scale, Helios uses 11,128
LUTs at d = 5. The d = 5 bitstream (`results/hw/par_d5/`) uses 52,628 LUTs (4.45%),
4.3× fewer than estimated, but missed 300 MHz, and `v++` lowered the clock to 213.7 MHz:
0.20 µs per shot for the state machine (43.2 cycles), 9.2× faster than v9c. Every
failing path runs from the forest's level counter through the chip-wide "did any node
join this level?" OR back to the counter (9 logic levels, 84% of the delay in routing).
That OR is now registered and acted on one cycle later, as the flood's exit test already
was (one more cycle per forest; 46.7 instead of 43.2 cycles at d = 5, every prediction
unchanged). Rebuilt with that change, d = 3 reaches 346.5 MHz (asked for 350;
`results/hw/par_d3b/`, 10,465 LUTs): 30.2 cycles = 0.087 µs per shot for the state
machine. At d = 5 the router gave up at 300 MHz after four hours (overlapping nets it
could not resolve); relinked from the same HLS output, it meets 250 MHz (255.1 MHz
achievable; `results/hw/par_d5c250/`, 52,456 LUTs): 46.7 cycles = 0.19 µs per shot,
10× faster than v9c. A 275 MHz request fell back to 250.6 MHz (`results/hw/par_d5c275/`).
These are routed clocks and simulated cycle counts; running on the card is waiting for a
working FPGA node.

The first version compared every label with every node and estimated 539k LUTs at
113 MHz for d = 5. The local parity halved that, and splitting the boundary raised the
clock. The five-loop version then took 2.3× the cycles of a cycle model of the same
algorithm (`tools/par_model.cpp`), because each short loop costs cycles to enter and
leave. The whole decode is now one loop at II = 1 with a state register, and the
per-edge step minimum is pipelined over 4 cycles, which had set the clock at 6.75 ns.
The state machine takes 27.7 cycles per shot at d = 3 and 43.2 at d = 5, slightly fewer
than the model (29.5, 48.0). That is **6–6.7× faster than v9c** at d = 3 and 5. The same generated code, run in g++, takes 97.5 cycles at d = 11 (p = 0.001) and 128.9 at d = 17 under cc noise, with every prediction matching the model. The model gives
0.041 µs/round at d = 11 and 0.046 at d = 17 (p = 0.001, 250 MHz), against CC's 0.11 and
0.37 (section 5.2). Whether d = 11 to 17 fits on the U280, and at what clock, is the open
question. Details are in `docs/beat_the_papers.md`.

## 6. Metrics

| Metric | Definition | Target |
|---|---|---|
| Bit-exactness | Shots where the card's prediction ≠ golden prediction | 0 |
| Invalid corrections / cap hits | Corrections that do not reproduce the syndrome / shots that hit the growth-iteration cap | 0 |
| Logical error rate (LER) ratio | union-find LER / PyMatching LER, same shots | close to 1 |
| Latency per shot | measured kernel time / shots, plus the tail (p99, p99.9, max) from per-shot work × the case's measured cycles per unit of work | — |
| Latency per round | latency per shot / d | ≤ 1 µs |
| Cycles per stage | on-card loop counts × II + pipeline fills, against measured cycles | explains where the time goes |
| Resources | routed LUT, FF, BRAM, URAM, DSP of the kernel | fits; % of chip |
| Clock | target frequency and worst negative slack after routing | slack ≥ 0 |

## 7. Plan for the final update

1. **Peeling search (25% of v9c's cycles).** Its II of 6 comes from taking a node off the queue in the step after it was put on. Peel several independent trees at once, or process a node's 12 edges over fewer steps.
2. **Clock (done for d ≤ 11).** v9c runs at 348.5 MHz (section 3.4). The d ≤ 17 build ran at 223.8 MHz because of its larger BRAM (section 5.2); splitting its arrays or using URAM should recover the clock.
3. **Cut the remaining 10–16% control overhead between loops.** Merge more per-iteration loops into one state-machine loop, as v9 did for the peeling search.
4. **Many shots in flight.** Today one shot is decoded at a time. A dataflow pipeline (load → decode → write back), or several decoder copies, raises throughput. v9c uses 1.9% of the chip's LUTs and 11% of its BRAM, so about 8 copies fit.
5. **The parallel design (section 5.3).** Bitstreams are routed at d = 3 (346.5 MHz) and d = 5 (250 MHz) but have not run on the card: the FPGA node of the last reservation never detected its card. Next: run all four bitstreams on the card and measure time per shot; widen the detector input port and run the state machine across shots so throughput matches the decode cycles; replace the straight-line generated code with a per-node module instantiated in a loop, since HLS could not get through the d = 11 code (64k lines) in five hours although it passes in g++ (229 cycles per shot at p = 0.003); and, for d = 17 (about 2.8M LUTs at today's size), a tiled design (`docs/research_ideas.md`).
6. **Matching accuracy without a CPU (stretch goal).** Union-find is less accurate than minimum-weight perfect matching (MWPM). Micro Blossom [14] reaches MWPM accuracy in hardware by splitting the blossom algorithm: the dual phase (growing regions around defects and detecting collisions) runs on parallel FPGA units, and the primal phase (forming blossoms, keeping the matching consistent) runs in software on an ARM core on the same chip (VMK180). The U280 has no on-chip CPU, and its host is across PCIe, where one round trip costs microseconds, more than our whole decode. The plan is a CPU-free version of the same split: the parallel design above already does the dual phase's growth; add a sequential primal unit in HLS next to it (built like the v9c kernel, which is already a sequential graph engine), connected on-chip, and resolve the common case (defects that pair off at once) in the parallel part so the sequential unit only sees the rare hard cases. The first step is to measure on our test cases how often a shot needs anything beyond pairwise matching.
7. **Final report:** accuracy against PyMatching, CPU vs FPGA latency against the 1 µs/round budget, area vs d, and what limits the design.

## 8. Code not written by me

All decoder, kernel, host, testbench and analysis code in this repository was written for
this project. It uses these external tools and libraries, unmodified:

- **Stim** [9]: generates the circuits, the detector error models and the samples (`python/gen_data.py`).
- **PyMatching** [7, 8]: the accuracy reference.
- **NumPy and Matplotlib**: analysis and figures.
- **AMD Vitis HLS / Vitis 2023.1** and **XRT 2.15**: the host follows the standard XRT native-API pattern (`xrt::device`, `xrt::kernel`, `xrt::bo`).
- The U280 platform `xilinx_u280_gen3x16_xdma_1_202211_1`.

## 9. Use of AI

I used Claude in Project Update 2. It helped me with setup, debugging, explaining
concepts and checking my work. I planned the designs, wrote the observations, and
decided what to try.

## 10. References

[1] A. G. Fowler, M. Mariantoni, J. M. Martinis, A. N. Cleland, "Surface codes: Towards practical large-scale quantum computation," *Phys. Rev. A* 86, 032324 (2012).
[2] B. M. Terhal, "Quantum error correction for quantum memories," *Rev. Mod. Phys.* 87, 307 (2015).
[3] F. Battistel et al., "Real-time decoding for fault-tolerant quantum computing: progress, challenges and outlook," *Nano Futures* 7, 032003 (2023), arXiv:2303.00054.
[4] L. Skoric, D. E. Browne, K. M. Barnes, N. I. Gillespie, E. T. Campbell, "Parallel window decoding enables scalable fault tolerant quantum computation," *Nature Communications* 14, 7040 (2023).
[5] N. Delfosse, N. H. Nickerson, "Almost-linear time decoding algorithm for topological codes," *Quantum* 5, 595 (2021), arXiv:1709.06218.
[6] S. Huang, M. Newman, K. R. Brown, "Fault-tolerant weighted union-find decoding on the toric code," *Phys. Rev. A* 102, 012419 (2020).
[7] O. Higgott, "PyMatching: A Python package for decoding quantum codes with minimum-weight perfect matching," arXiv:2105.13082 (2021).
[8] O. Higgott, C. Gidney, "Sparse blossom: correcting a million errors per core second with minimum-weight matching," *Quantum* 9, 1600 (2025), arXiv:2303.15933.
[9] C. Gidney, "Stim: A fast stabilizer circuit simulator," *Quantum* 5, 497 (2021), arXiv:2103.02202.
[10] P. Das et al., "AFS: Accurate, fast, and scalable error-decoding for fault-tolerant quantum computers," *HPCA* (2022), pp. 259–273.
[11] N. Liyanage, Y. Wu, A. Deters, L. Zhong, "Scalable quantum error correction for surface codes using FPGA," *IEEE QCE* (2023), pp. 916–927.
[12] N. Liyanage, Y. Wu, S. Tagare, L. Zhong, "FPGA-based distributed union-find decoder for surface codes," *IEEE Trans. Quantum Eng.* 5 (2024).
[13] B. Barber et al., "A real-time, scalable, fast and resource-efficient decoder for a quantum computer," *Nature Electronics* 8, 84 (2025).
[14] Y. Wu, N. Liyanage, L. Zhong, "Micro Blossom: Accelerated minimum-weight perfect matching decoding for quantum error correction," *ASPLOS* (2025), pp. 639–654.
[15] X. Yang et al., "Real-time surface-code error correction using an FPGA-based neural-network decoder," arXiv:2605.04892 (2026).
[16] AMD, *Vitis High-Level Synthesis User Guide* (UG1399), v2023.1.
