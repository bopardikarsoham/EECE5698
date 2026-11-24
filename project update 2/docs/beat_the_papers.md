# How far we are from the published FPGA decoders, and what would close the gap

Work from 21–22 Nov 2026. Published numbers are from the primary sources (see
`docs/related_work.md` section 5). Our numbers are card measurements unless marked
"model" or "HLS estimate".

## 1. Large codes on the card (d = 13 to 17)

The v9c kernel was rebuilt with larger limits (5120 detectors, 28,672 edges;
`KDEFS="UF_MAX_DET=5120 UF_MAX_EDGES=28672"`). The larger arrays raise BRAM from 208 to
691 blocks (38%), and routing at 300 MHz failed (WNS −1.135 ns), so `v++` lowered the
clock to **223.8 MHz**. LUTs barely change (22,559, 1.9%).

All 18 cases (d = 13, 15, 17 at four noise rates, two 200,000-shot cases, four
circuit-noise "cc" cases) **match the golden outputs on every shot**
(`results/hw/bit_v6_v9c/results/hw_v9c_big.log`).

Mean µs per shot at 223.8 MHz, and µs per round scaled to 300 MHz (× 223.8/300, then
÷ d; the scaling ignores memory latency, which does not scale with the clock):

| d | p | µs/shot (223.8 MHz) | µs/round at 300 MHz | p99.9 µs/round at 300 MHz |
|---|---|---|---|---|
| 13 | 0.001 | 16.90 | 0.97 | 2.17 |
| 15 | 0.001 | 26.12 | 1.30 | 2.48 |
| 17 | 0.001 | 37.29 | 1.64 | 2.85 |
| 13 | 0.003 | 47.81 | 2.74 | 5.65 |
| 15 | 0.003 | 73.56 | 3.66 | 6.86 |
| 17 | 0.003 | 107.87 | 4.73 | 7.89 |
| 13 | 0.01 | 194.28 | 11.15 | 16.91 |
| 17 | 0.01 | 472.54 | 20.74 | 28.31 |

(The d = 13 and 17 rows at p = 0.001 are the 200,000-shot cases.) Time per shot grows
about as d³ at fixed p, as expected for a decoder that touches every defect and every
grown edge one at a time.

## 2. Like-for-like with Collision Clustering

Collision Clustering (CC) reports µs per round under a milder circuit noise than our
uniform model. `python/gen_data.py --noise cc` approximates it: two-qubit gates and
measurements at p, single-qubit gates, resets and idling at p/10. At p = 0.001:

| d | Ours, µs/round (cc noise, 300 MHz) | CC, µs/round | Gap |
|---|---|---|---|
| 11 | 0.49 (measured, 300 MHz bitstream) | 0.11 | 4.5× |
| 13 | 0.69 (scaled from 223.8 MHz) | 0.16 | 4.3× |
| 15 | 0.91 (scaled) | 0.25 | 3.6× |
| 17 | 1.16 (scaled) | 0.37 | 3.1× |

The gap narrows with d because CC's time also grows faster than linearly. CC also
uses fewer LUTs (4,600 at d = 11 to 10,446 at d = 17, vs our 22,000) and runs at
401–449 MHz; but its graph is fixed in the hardware, and ours is loaded at run time.

Micro Blossom decodes a whole d = 13 block in 0.8 µs (with streaming), against our
12.6 µs; Helios reports 23.7 ns per round at d = 17. Both build one processing unit per
node and edge: Helios uses 340,084 LUTs at d = 13, Micro Blossom 553,000. No sequential
decoder, ours included, gets near those numbers; the parallel design in section 3 is
the way there.

## 3. Parallel prototype (one unit per node and per edge)

`src/par/`: the graph is wired in at synthesis time (`python/gen_par_graph.py`). Each
node keeps a cluster label (the smallest node ID in its cluster), and labels flood along
grown edges. Growth is the same weighted growth as `uf::decode`, so the clusters are the
same. Two changes made it scale:

- **Local cluster parity.** The first version compared every label with every node
  (N² comparators: 14,641 at d = 5). Now parity is passed up a forest rooted at each
  label, one level per cycle, and the "active" flag back down. Only neighbours are wired
  together, and it costs about 3 × (cluster depth) cycles per growth iteration. The
  earlier version is kept behind `-DPAR_CROSSBAR_PARITY`.
- **Split boundary.** The single boundary node has a huge fan-in (24 edges at d = 3, 72
  at d = 5, 144 at d = 7), and its one-cycle min tree set the clock. It is now split
  into virtual nodes of 8 boundary edges each, numbered first (0..G−1). A cluster
  touches the boundary exactly when its label is a virtual node. Growth is unchanged,
  because a cluster touching the boundary is inactive however it touches it. Only the
  peeling forest changes. Largest fan-in is now 12.

Predictions differ from the sequential reference only through the peeling forest.
Accuracy is the same, on the same shots:

| Case | Logical errors, prototype | Logical errors, reference |
|---|---|---|
| d = 3, p = 0.01 | 5,964 | 5,996 |
| d = 5, p = 0.003 | 369 | 385 |
| d = 11, p = 0.003 | 23 | 25 |
| d = 11, p = 0.01 | 16,534 | 16,482 |
| d = 17, p = 0.01 (10,000 shots) | 2,672 | 2,693 |

Each case's golden output is `par_expected.bin`, from `tools/par_model.cpp --local-parity
--split-boundary 8 --write-par-expected`.

**HLS results** (Vitis HLS 2023.1, target 3.33 ns, U280 part; co-simulation of 200
shots, d = 3 at p = 0.01, d = 5 at p = 0.003; every version passes C simulation on
100,000 shots and co-simulation; reports in `results/par/`):

| Version | d | Cycles/shot (cosim) | Clock estimate | LUT (HLS est.) | FF (HLS est.) |
|---|---|---|---|---|---|
| all-pairs parity, one boundary | 3 | 38.0 | 6.68 ns (150 MHz) | 51,928 | 8,247 |
| all-pairs parity, one boundary | 5 | 76.5 | 8.82 ns (113 MHz) | 538,884 | 34,118 |
| local parity, one boundary | 3 | 69.4 | 6.68 ns (150 MHz) | 60,428 | 14,584 |
| local parity, one boundary | 5 | 121.7 | 8.82 ns (113 MHz) | 274,678 | 57,943 |
| local parity, split boundary | 3 | 67.2 | 5.92 ns (169 MHz) | 56,401 | 12,413 |
| local parity, split boundary | 5 | 114.4 | 6.53 ns (153 MHz) | 269,130 | 46,568 |
| one state machine, step min in 2 cycles | 3 | 38.3 | 6.75 ns (148 MHz) | 37,817 | 5,269 |
| one state machine, step min in 2 cycles | 5 | 65.7 | 6.75 ns (148 MHz) | 211,937 | 16,243 |
| one state machine, pipelined step min | 3 | 36.4 | 4.76 ns (210 MHz) | 44,366 | 5,981 |
| one state machine, pipelined step min | 5 | 58.3 | 4.76 ns (210 MHz) | 251,769 | 20,692 |
| **same, parent kept as one bit per edge end** | 3 | 36.4 | 4.72 ns (212 MHz) | 39,628 | 5,489 |
| **same, parent kept as one bit per edge end** | 5 | 58.3 | 4.76 ns (210 MHz) | 226,948 | 18,600 |

The co-simulation cycles include reading the detectors and writing the results (64
shots per burst), so they are throughput figures. The decode itself (the state
machine) takes fewer cycles; see below.

**Removing the loop overhead.** The split-boundary version ran each growth iteration as
five short pipelined loops (parity forest levels, parity up, active down, flood, step),
and each loop cost a few cycles to enter and leave: 2.3× the model's cycle count. The
whole decode is now one loop at II = 1 with a state register (`decode_fused`,
`-DPAR_FUSED`). One forest serves both the parity passes and the peeling. Three further
changes:

- The flood stops when the labels did not change in the *previous* cycle, a comparison
  of registers. That takes the "anything changed?" OR off the flood's path, for one
  extra flood cycle.
- The step minimum (smallest step that fully grows some edge, a minimum over every
  edge) is a pipeline: one cycle computes each edge's remaining step, then each cycle
  takes minima of 8 (4 cycles at d = 3 and 5, 5 at d = 7). In one or two cycles it set
  the clock at 6.75 ns.
- At the start every node is its own cluster, so the first parity pass is skipped (a
  node is active when it is a defect), and a shot without defects ends at once. The
  first stage of the minimum shares the cycle that computes the edge rates, and the
  parity forest is reset in the flood's last cycle.

State-machine cycles per shot (g++, 100,000 shots, every prediction equal to the model's):

| Case | Model | Loops (split boundary, HLS) | One state machine |
|---|---|---|---|
| d = 3, p = 0.01 | 29.5 | 67.2 (cosim) | 27.7 |
| d = 5, p = 0.003 | 48.0 | 114.4 (cosim) | 43.2 |
| d = 7, p = 0.001 | 48.6 | | 39.7 |
| d = 11, p = 0.001 (20,000 shots) | 112.6 | | 97.5 |
| d = 11, p = 0.003 (100,000 shots, with the registered level OR) | 218.2 | | 229.2 |
| d = 17, p = 0.001 (10,000 shots) | 197.7 | | 167.7 |
| cc noise, d = 11 (10,000 shots) | 90.7 | | 74.9 |
| cc noise, d = 17 (10,000 shots) | 160.2 | | 128.9 |

(At d = 11 and 17 these are g++ runs of the same generated code. A d = 11 bitstream
build at 250 MHz was started on 23 Nov 2026 to test whether HLS can synthesize a graph
that large in this form; the generated header is 64k lines, against 5k at d = 5. It was
still in the HLS front end (clang) after 41 minutes and had not produced a kernel when
the reservation ended about 5 hours later; its log was lost with the build machine.)
The d = 11, p = 0.003 golden output must be written with `--split-boundary 8`; without
it the peeling forest differs and 6 of 100,000 shots disagree. An earlier version that stored each node's parent as a
16-bit edge ID failed 273 of 10,000 shots at cc noise, d = 17, for a reason not found;
the current version keeps the parent as one bit per edge end and passes.

The state machine is now *below* the model at every size, and the clock estimate is
4.76 ns (210 MHz): **0.17 µs per shot at d = 3, p = 0.01** and **0.28 µs at d = 5,
p = 0.003**, including I/O, against 1.04 and 1.86 µs for v9c on the card (6–6.7×). At
210 MHz the state-machine counts give 0.042 µs/round at d = 11 and, under cc noise,
0.032 at d = 11 and 0.036 at d = 17: 3.4× and 10× faster than CC (0.11, 0.37), if a
design that large fits. The critical path is now the flood: each node's new label is the
minimum over up to 12 neighbours.

**Cycle model, local parity and split boundary** (`results/par_model_local_split.csv`,
250 MHz):

| Case | Mean cycles | µs/round (mean) |
|---|---|---|
| d = 11, p = 0.001 | 112.6 | 0.041 |
| d = 13, p = 0.001 | 137.7 | 0.042 |
| d = 17, p = 0.001 | 196.5 | 0.046 |
| d = 11, p = 0.003 | 218.2 | 0.079 |
| cc noise, d = 11 | 90.7 | 0.033 |
| cc noise, d = 17 | 160.2 | 0.038 |

Against CC under its noise (0.11 µs/round at d = 11, 0.37 at d = 17), the model is 3.3×
faster at d = 11 and 10× faster at d = 17 at 250 MHz. The state machine now matches the
model's cycle counts, so the open question is the clock and the size at large d, not
the cycles. At 210 MHz (the d = 3 estimate) the d = 11 model figure becomes 0.049
µs/round, still 2.2× ahead of CC.

## 4. What is left

1. **A bitstream (d = 3, done).** Routed at the requested 300 MHz (worst slack +0.003 ns,
   311.9 MHz achievable) in 10,739 LUTs, 5,615 registers, 2 BRAM
   (`results/hw/par_d3/`). HLS estimated 39,628 LUTs and 212 MHz: 3.7× too high and 30%
   too slow. At 300 MHz: 0.092 µs per shot for the state machine, 0.12 µs with I/O, 8.6×
   faster than v9c. **d = 5**: 52,628 LUTs (4.45%), but 213.7 MHz: the forest's "did any
   node join this level?" OR fed the level counter in the same cycle. That OR is now
   registered (one more cycle per forest, 46.7 cycles at d = 5). Rebuilt: d = 3 at
   346.5 MHz (0.087 µs per shot, `results/hw/par_d3b/`); d = 5 failed to route at
   300 MHz and meets 250 MHz when relinked (0.19 µs per shot, 52,456 LUTs,
   `results/hw/par_d5c250/`). Card runs wait for a working node.
2. **Size.** Routed LUTs: 10.7k at d = 3, 52.6k at d = 5 (HLS: 39.6k and 227k). Helios
   uses 11.1k at d = 5 and 340k at d = 13, so ours is about 5× Helios at d = 5. From
   d = 3 (78 edges) to d = 5 (502 edges) LUTs grow 4.9× for 6.4× more edges: 138 then
   105 LUTs per edge. At ~105 per edge, d = 7 (1,558 edges) is ~160k, d = 11 (6,718)
   ~700k, which fits the U280's 1.3M, and d = 17 (26,398) ~2.8M, which does not: d = 17
   needs the tiled design in docs/research_ideas.md. `gen_par_graph.py` writes straight-line code, and HLS takes about 2 hours at
   d = 5; the d = 7 synthesis of the loop version had not finished scheduling after 3
   hours and was stopped. Larger d needs a per-node module instantiated in a loop.
3. **Clock.** The flood's minimum over up to 12 neighbours (8-bit labels at d ≤ 5) sets
   4.76 ns in the HLS estimate. Routing will tell whether it really limits the clock.
4. **I/O.** Detectors are read 32 bits at a time; a 512-bit port and a decode loop that
   runs across shots would bring co-simulation cycles down to the state-machine count.
5. **The sequential kernel.** Several shots in flight (v9c uses 1.9% of the LUTs) raises
   throughput but not latency.
