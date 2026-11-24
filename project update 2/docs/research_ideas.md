# Research survey: ideas for the parallel decoder

Survey of 22 Nov 2026: hardware decoders for the surface code, decoding algorithms, and
FPGA graph processing outside quantum computing. Numbers are as stated in each paper
(abstracts, text and tables); a few come only from figures and are marked. Where a
paper is the main source for a claim we will put in the report, read it again before
citing it.

## 1. Where we stand

| Decoder | Algorithm | Board | d | LUTs | Clock | Latency (mean) | Graph |
|---|---|---|---|---|---|---|---|
| **Ours, parallel (routed d=3; g++ cycles)** | weighted UF, unit per node and edge | U280 | 3 / 5 | 10.7k routed / 227k HLS est. (~61k at the d=3 ratio) | 300 MHz routed (d=3) | 0.092 µs/shot at d=3; ~0.03 µs/round at d=11–17 (cycles at 300 MHz) | fixed |
| Ours, v9c (card) | weighted UF, sequential | U280 | ≤17 | 22k | 300–349 MHz | 0.49 µs/round at d=11, cc noise | runtime |
| Helios [1] | UF, one PE per vertex, Verilog | VCU129 | 5 / 13 / 17 | 11.1k / 340k / 889k | 130 / 100 / 75 MHz | 23.7 ns/round at d=17 (circuit) | fixed |
| LCD, Riverlane [2] | UF, d/2 vertices per PE, unweighted | VU19P | 17 | 252k | 285 MHz | 0.67 µs/round | fixed (+ pre-grown edges at runtime) |
| Collision Clustering [3] | UF over defects, no stored graph | XCVU3P | 11 / 17 | 4.6k / 10.4k | 400–449 MHz | 0.11 / 0.37 µs/round | surface code only |
| Micro Blossom [4] | exact MWPM, FPGA + ARM CPU | VMK180 | 13 | 553k | 62 MHz | 0.8 µs per block | fixed |
| Astrea [5] | brute-force MWPM, Hamming weight ≤ 10 | Zynq US+ | ≤ 7 | 5.6% | 250 MHz | max 456 ns | weights in BRAM |
| Promatch [6] | greedy predecoder + Astrea | Kintex US+ | 11, 13 | ~3% + tables | 250 MHz | 526 ns avg, 960 max (d=13, p=1e-4) | tables |
| QUEKUF [7] | UF dataflow (toric) | Alveo U55C | ≤ 10 | — | 350 MHz | ~11 ns (table entry) | fixed |

Two things stand out:

- **Our cycles are good; size is the question.** Per round we are in Helios's range.
  Size looked like a 20× gap (227k HLS estimate at d = 5 against Helios's 11.1k), but
  the d = 3 bitstream routed in 10.7k LUTs against an HLS estimate of 39.6k, at 300 MHz
  against an estimated 212 MHz. At that ratio d = 5 would be ~61k, ~5× Helios; the d = 5
  bitstream will tell. Helios, LCD and Micro Blossom all generate HDL from the graph;
  none uses HLS.
- **Helios has no global minimum** (but see section 3: for our weights this does not pay). Each edge grows by a fixed amount per cycle,
  `growth = min(growth + odd_u + odd_v, w)`, so the controller only needs an OR of two
  bits (busy, odd) from every PE. Our design computes the smallest step that completes
  some edge, a minimum over every edge: the RATE/STEP pipeline, an 8-bit comparator per
  edge, and 4–5 cycles per growth iteration. This is probably the largest single cost
  in our LUTs.

Nobody has built exact MWPM on an FPGA without a CPU. Micro Blossom is the closest, and
its "parallel primal" speedup (4.2×) comes from handling conflicts and pre-matching in
hardware, not from a hardware primal phase; moving simple alternating trees into
hardware is its stated future work.

## 2. Ideas, ranked

Each row says which problem it addresses: **S** size (LUTs, and HLS time),
**L** latency, **A** accuracy, **C** clock.

| # | Idea | From | Fixes | Effort | Notes |
|---|---|---|---|---|---|
| 1 | **Fixed-increment growth instead of the global step minimum** | Helios [1], Micro Blossom (4-bit weights) | S, C | low | **Tested (section 3): not a win.** Same clusters, but with our weights (7–16) it is 35–50% slower; with weights halved it breaks even on cycles and costs 3–13% more logical errors. |
| 2 | **Generate RTL, or one HLS module per node/edge with INLINE off** | Helios, LCD, Micro Blossom; AMD UG1399; TAPA [8] | S, C | medium | Straight-line HLS takes hours at d = 5 and never finished at d = 7. TAPA/AutoBridge floorplans across the U280's three SLRs (147 → 297 MHz average over 43 designs). |
| 3 | **Coarse-grain the PEs: a tile of nodes per PE, state in LUTRAM** | LCD (d/2 vertices per PE, resources ~d²), Helios-n (LUTRAM contexts, 1-cycle switch, d = 51), ForeGraph/Dalorex [9] | S | medium–high | The way to fit d = 11–17. Costs cycles per round unless combined with idea 4. |
| 4 | **Two-level labelling: resolve inside a tile, then flood between tiles** | FPGA image labelling with merge tables (Bailey, Klaiber [10]), Afforest [11] | L | medium | Flood hops drop from the lattice diameter to the tile-graph diameter. Natural partner of idea 3. |
| 5 | **Overlap flood, parity up and active down** | Helios (merging and checking run together, 4-defect cluster in 8 cycles) | L | low | Our forest is built level by level after the flood ends. Helios sets the parent when a node adopts a smaller ID, so the forest forms during the flood. |
| 6 | **Pre-match isolated defect pairs in the node units** | Promatch step 1 (last step needed in 99.6–99.8% of samples at p = 1e-4), Micro Blossom pre-matching, Clique | L (mean) | low–medium | Must only commit unambiguous pairs: Promatch shows Clique-style naive rules hurt accuracy at d = 13. |
| 7 | **Streaming: decode rounds as they arrive, commit the oldest layer** | Snowflake [12], Micro Blossom round-wise fusion, CAYG | L (big) | medium | Latency after the last round then covers a window of ~d layers or less, not the whole block. This is how Micro Blossom gets 0.8 µs. |
| 8 | **Pipelined or bit-serial 12-input minimum in the flood** | graph survey | C | low | Registered 4→3→1 tree, or MSB-first wired-AND argmin. Targets > 300 MHz. |
| 9 | **Pointer jumping for labels inside a tile** | Shiloach–Vishkin, ConnectIt | L (tail) | medium | Diameter hops → log₂(cluster size). Unpublished for QEC decoders. Only cheap in tiles (local read); fully spatial it needs a large mux. |
| 10 | **Belief propagation before UF** | belief-find [13], BP partial decoding [14], IBM Relay-BP on FPGA (24 ns per iteration) | A | high | Threshold 0.795% (weighted UF) → 0.937% (belief-find), above MWPM's 0.817%. Adds the BP iterations to latency. |
| 11 | **Hardware primal unit for MWPM (no CPU)** | Micro Blossom future work | A | high | Already in REPORT section 7, item 6. |
| 12 | **CC-style defect-indexed engine as a fallback** | Collision Clustering | S | medium | ~10k LUTs at d = 17, 0.37 µs/round; replicate per patch. |

Not worth it for us: HBM graph frameworks (ThunderGP, GraphLily, ReGraph), which are
built for bandwidth, not sub-µs latency (HBM access alone is ~100 ns); auction or
Hungarian matching on FPGA (µs-scale); lookup tables (LILLIPUT, d ≤ 5).

## 3. Tested: fixed-increment growth (idea 1)

Growing by a fixed amount per cycle gives exactly the same clusters as the step
minimum: the minimum only skips growth units in which no edge completes, and nothing
changes in those. So the test is cycles. `tools/par_model.cpp --quantize K` divides every
weight by K (rounded, at least 1) and counts *growth units* per shot (the sum of the
steps); the state-machine cycles are from the generated decoder (`-DPAR_FUSED`, g++,
100,000 shots, every prediction equal to the model's).

Our weights are 7–16, and an edge needs twice its weight to complete, so a shot needs
5× more growth units than growth iterations:

| Case | K | Max weight | Iterations | Growth units | State machine now (step min) | Fixed increment (est.) |
|---|---|---|---|---|---|---|
| d = 5, p = 0.003 | 1 | 14 | 2.63 | 13.65 | 43.2 | ~66 |
| | 2 | 7 | 2.11 | 7.14 | 36.1 | ~43 |
| | 3 | 5 | 1.95 | 4.60 | 35.3 | ~35 |
| cc noise, d = 11 | 1 | 16 | 4.36 | 19.99 | 75.4 | ~101 |
| | 2 | 8 | 3.66 | 10.56 | 66.0 | ~68 |
| | 3 | 5 | 2.88 | 6.83 | 53.8 | ~51 |

The estimate removes the step pipeline (PSTAGES cycles per iteration: 4 at d = 5, 5 at
d = 11) and charges 3 cycles (grow, then the flood's two-cycle "nothing changed") for
each growth unit in which no edge completes.

Coarser weights cost accuracy, more at larger d (logical errors, 100,000 shots):

| Case | K = 1 | K = 2 | K = 3 | K = 4 |
|---|---|---|---|---|
| d = 5, p = 0.003 | 369 | 379 | 396 | 441 |
| d = 7, p = 0.005 | 1,119 | 1,183 (+6%) | 1,288 (+15%) | 1,265 |
| d = 7, p = 0.01 | 11,222 | 11,509 (+3%) | 12,404 (+11%) | 15,186 |
| d = 11, p = 0.003 | 23 | 24 | 30 | 32 |
| d = 11, p = 0.005 | 545 | 616 (+13%) | 752 (+38%) | 667 |

**Result.**

- With our full weights, fixed-increment growth is 35–50% slower than the step minimum.
- At K = 2 it is about even on cycles, saves the step pipeline's per-edge comparators and
  narrows the growth counters, and costs 3–13% more logical errors.
- Halving the weights in the *current* design alone cuts 12–16% of the cycles for the
  same accuracy cost.

So neither is free, and Helios's fixed growth is not an accuracy-neutral way to remove the
step minimum for weighted graphs. Keep the step minimum. Fixed growth only makes sense if
routing shows the step pipeline is a large share of the LUTs, which needs the d = 3 and
d = 5 bitstreams. The d = 3 bitstream (10.7k LUTs routed, against 39.6k estimated) shows
most of the apparent gap to Helios was the HLS estimate, not the step minimum.

## 4. Accuracy: weighted UF against MWPM

No paper gives the logical-error-rate ratio at circuit noise, p = 0.001, d = 11–17.
Thresholds: 0.795% (weighted UF) vs 0.817% (MWPM) on the rotated code [13]; 0.62% vs
0.72% on the toric code [15]. Scaling as (p/p_th)^((d+1)/2) gives a ratio of roughly
1.3× to 3.8× at d = 17, which is an extrapolation, not a measurement. We can measure it
directly (Stim + PyMatching against our model), and that number would be new.

## 5. Suggested order

1. ~~Idea 1~~: tested, keep the step minimum (section 3).
2. Idea 8 and idea 5 in the current HLS design, measured at d = 3 and 5. (a day)
3. Idea 2 then 3 + 4: a tiled, module-per-tile design that synthesizes at d = 11–17.
   This is the final project's main work.
4. Idea 7 (streaming) once the tiled design runs.
5. Accuracy: measure weighted UF vs MWPM; then idea 10 or 11 as the stretch goal.

## References

1. Helios: N. Liyanage, Y. Wu, S. Tagare, L. Zhong, arXiv:2301.08419; IEEE TQE 2024, arXiv:2406.08491.
2. Local Clustering Decoder (Riverlane), arXiv:2411.10343.
3. Collision Clustering (Riverlane), arXiv:2309.05558; Nature Electronics 2025.
4. Micro Blossom: Y. Wu, N. Liyanage, L. Zhong, ASPLOS 2025, arXiv:2502.14787.
5. Astrea: S. Vittal, P. Das, M. Qureshi, ISCA 2023, doi:10.1145/3579371.3589037.
6. Promatch: N. Alavi et al., arXiv:2404.03136.
7. QUEKUF, ACM TRETS, doi:10.1145/3733239; https://github.com/necst/QUEKUF.
8. TAPA / AutoBridge, arXiv:2209.02663; AMD UG1399 "Improving Synthesis Runtime and Capacity".
9. ForeGraph (FPGA 2017); Dalorex (HPCA 2023, arXiv:2207.13219).
10. D. Bailey, M. Klaiber, connected-component labelling on FPGA, J. Imaging 2019, 2022.
11. Afforest, IPDPS 2018; ConnectIt, VLDB 2020.
12. Snowflake, arXiv:2406.01701.
13. Belief-matching / belief-find: O. Higgott et al., PRX 2023, arXiv:2203.04948.
14. BP as a partial decoder (Riverlane), arXiv:2306.17142.
15. Weighted UF: S. Huang, M. Newman, K. Brown, arXiv:2004.04693.
