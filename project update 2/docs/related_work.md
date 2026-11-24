# Related work: hardware decoders for the surface code

Notes for framing the final report like a research paper. Read in November 2026.
Numbers are as the authors report them; they use different noise models, distances
and hardware, so they are not directly comparable. Each comparison below says which
setting it is.

## 1. The papers closest to this project

| Work | Venue | Algorithm | Hardware | Noise model | Headline result |
|---|---|---|---|---|---|
| **Helios** (Liyanage, Wu, Tagare, Zhong) | arXiv 2301.08419 (2023); IEEE TQE 2024 (arXiv 2406.08491) | Distributed union-find, one processing element per vertex, RTL | Xilinx VCU129 | Phenomenological, circuit-level, erasure | 11.5 ns avg per round at d = 21 (phenomenological, p = 0.1%); 23.7 ns at d = 17 (circuit-level); d = 51 at 544 ns per round. About 340K LUTs at d = 13 |
| **Collision Clustering** (Barber et al., Riverlane) | Nature Electronics 2025 (arXiv 2309.05558) | Union-find variant with compact cluster data structures | Xilinx UltraScale+ XCVU3P FPGA, and ASIC | Circuit-level, p = 0.1% | Per-round avg 0.16 µs at d = 13 with 5,914 LUTs; 0.81 µs at d = 21; ~400 MHz. Threshold 0.78%. ASIC: 0.06 mm², 8 mW |
| **Micro Blossom** (Wu, Liyanage, Zhong) | ASPLOS 2025 (arXiv 2502.14787) | Exact MWPM, software + FPGA accelerator | FPGA | Circuit-level, p = 0.1% | 0.8 µs avg latency at d = 13 at 62 MHz; "first publicly known sub-µs" MWPM |
| **QUEKUF** (Valentino, Branchini, Conficconi, Sciuto, Santambrogio) | IPDPSW/RAW 2024; ACM TRETS 2025 | Union-find, dataflow, **HLS testbench, Vitis/XRT** | **AMD Alveo U55C** | Code capacity (toric code) | d up to 10; up to 20.14× (RAW) / 7.30× (TRETS) faster than their C++ version. Open source |
| **Snowflake** (Tahaab, Chan, Kirkman, Benjamin) | arXiv 2608.10576 (Aug 2026) | Streaming union-find | AMD XCKU5P, 200 MHz, room and cryogenic temperature | Circuit-level, p = 0.1% | Unrotated surface code up to d = 9; adds decoder confidence scores at negligible cost |
| **AFS** (Das et al.) | HPCA 2022 (arXiv 2001.06598) | Pipelined union-find (3 stages) | Modelled hardware | Circuit-level | ~42 ns average latency; also syndrome compression for bandwidth |
| **Google Willow** (Google Quantum AI) | Nature 2024/2025 | Parallel sparse blossom (software) | CPU workstation | Real device | 63 µs avg latency at d = 5 with a 1.1 µs cycle; decoding keeps up but latency spans ~50 cycles |
| **Temporal-parallel UF** (Itoh, Kasamura, Irie, Kadomoto) | arXiv 2607.27930 (Jul 2026) | Union-find with sandwich decoding | ASIC (synthesis + simulation) | Phenomenological | 35% lower average latency than batch UF at d = 21 |

Context papers:
- **Real-Time QEC System Stack** (Chen et al., arXiv 2605.30765, May 2026): survey.
  Evaluates decoders on six axes: accuracy, sustained throughput *and p99 tail
  latency*, scalability with d, hardware mapping efficiency, code generality, and use
  of extra data (erasures, confidence). Argues that tail latency, not average, decides
  real-time readiness. Notes PyMatching meets 1 µs for most shots at d ≤ 13,
  p = 0.1%, but rare heavy syndromes spike well beyond it.
- **Decoder switching** (Toshio et al., Fujitsu, arXiv 2510.25222): fast "weak"
  decoder with soft output, switch to a slow accurate decoder only on low-confidence
  windows. Accuracy close to the strong decoder at the weak decoder's average speed.
- **Impacts of decoder latency** (Khalid et al., 1QBit / Quantum Machines, arXiv
  2511.10633): models the *reaction time* (decoder + controller) and shows how it
  drives the cost of a utility-scale machine.
- Counterpoint in Micro Blossom §8: for a pure memory experiment, the effective logical
  error rate depends only on *average* latency. Worst-case matters once the decoder is
  in a feedback loop (feed-forward, magic-state injection), which is the reaction-time
  argument above.

## 2. How these papers are structured

The same flow appears in Helios, Collision Clustering, Micro Blossom and Snowflake:

1. **Introduction.** QEC needs a decoder that keeps up with the hardware: one round of
   syndromes per ~1 µs on superconducting qubits. A slow decoder builds a backlog that
   grows exponentially with computation depth (CC cites Terhal, Rev. Mod. Phys. 87,
   307, 2015). Software decoders
   are accurate but too slow or unpredictable; prior hardware decoders trade accuracy,
   resources or scalability. Ends with a **3-bullet contributions list**:
   (i) algorithm/architecture idea, (ii) its FPGA implementation, (iii) measured
   results with one headline number. Then "open source at ...".
2. **Background.** Surface code, decoding graph (vertices = detectors, edges = error
   mechanisms, weights = log((1-p)/p)), the decoding algorithm (UF or MWPM).
3. **Design.** The algorithm change that makes it hardware-friendly, with a figure.
4. **Implementation.** Board, clock, resource usage, what limits the size.
5. **Evaluation.**
   - Methodology: noise model (circuit-level at **p = 0.1%** is the common reference
     point), simulator, shots per point (10⁶–10⁸), how time is measured (clock cycles,
     normalised **per measurement round** = divided by d).
   - Accuracy: logical error rate vs p for several d (threshold plot), compared with
     MWPM.
   - Latency per round vs d, at fixed p.
   - Resources vs d (Fmax, LUT, FF, BRAM): CC's Table I is the cleanest template.
   - **Comparison table** against prior hardware decoders (Helios Table 2, Snowflake
     Table 2): device, algorithm, code, max d, average time at a common d.
6. **Related work**, **Limitations**, **Conclusion**.

## 3. Where this project stands and where it can stand out

What this project already does that matches the field's standard:
- Stim circuit-level noise, rotated surface code, d = 3–11, MWPM (PyMatching)
  comparison, time normalised per round, p = 0.001 included in the sweeps.

Where it can realistically go beyond the published work (each is something none of
the papers above reports, as far as these notes found; check again before claiming
"first"):

1. **Tail and worst-case latency as a headline result, not just the average.** The
   headline numbers and comparison tables in Helios, CC and Snowflake are averages
   per round. Distributions do appear (Micro Blossom plots latency distributions,
   Snowflake plots emulated growth-depth histograms, Helios analyses worst-case
   complexity), but none of them states a hard per-shot deadline and reports
   measured p99 / max cycles against it on hardware. The 2026 survey says p99 tail
   latency is what decides real-time readiness. Measuring the full per-shot cycle
   distribution on the U280 (mean, p99, p99.9, max) and giving a guaranteed bound
   (cap on growth iterations × cycles per iteration) would be a distinct
   contribution. The software already reports this distribution (README 6.3).
2. **One bitstream for any code: the decoding graph is a runtime input.** Helios, CC
   and Snowflake hard-wire the code's geometry. This kernel loads any Stim
   detector-error-model graph at run time (repetition, rotated and unrotated surface
   code, X or Z) up to a size limit. Generality across codes is one of the survey's
   six axes.
3. **HLS on a datacenter Alveo card with circuit-level weighted graphs.** QUEKUF is
   the only HLS/Alveo UF decoder found, and it targets the toric code under code
   capacity noise with unit weights. This project uses weighted circuit-level graphs
   from Stim. A clear HLS design-space study (sequential → pipelined → parallel, with
   cycles, Fmax and area at each step) is useful to others building on Vitis.
4. **Honest accuracy accounting.** The weight-rounding experiment (README 6.6) shows
   the gap to MWPM is algorithmic, not rounding. Most hardware papers report only
   their own threshold.

What it should *not* try to beat in one semester: Helios's absolute latency (fully
parallel RTL on a much larger FPGA) or CC's area efficiency. Compare against them in
a table with the setting stated, and position this work on worst-case guarantees
and generality instead.

## 4. Changes to the plan this suggests

- Add a **cycle counter** to the kernel and return cycles per shot, so the U280 run
  gives a per-shot latency distribution (not only total batch time).
- Use **circuit-level p = 0.001** as the headline setting, as the field does, and
  keep p = 0.003–0.01 as the stress test.
- Report a **CC-style table** per d: Fmax, average and p99/max cycles per round,
  LUT, FF, BRAM.
- Build a **comparison table** with Helios, CC, Micro Blossom, QUEKUF and Snowflake.
- Shape the final report as: Introduction (with 3 contributions) → Background →
  Design → Implementation → Evaluation → Related work → Limitations.

## Sources

- Helios: https://arxiv.org/abs/2301.08419 and https://arxiv.org/abs/2406.08491
- Collision Clustering: https://www.nature.com/articles/s41928-024-01319-5 and https://arxiv.org/abs/2309.05558
- Micro Blossom: https://arxiv.org/abs/2502.14787 (code: https://github.com/taugoust/micro-blossom)
- QUEKUF: https://dl.acm.org/doi/10.1145/3733239 (code: https://github.com/necst/QUEKUF)
- Snowflake: https://arxiv.org/abs/2608.10576
- AFS: https://arxiv.org/abs/2001.06598 (HPCA 2022 version: https://memlab.ece.gatech.edu/papers/HPCA_2022_2.pdf)
- Temporal-parallel UF: https://arxiv.org/abs/2607.27930
- Real-Time QEC System Stack survey: https://arxiv.org/abs/2605.30765
- Decoder switching: https://arxiv.org/abs/2510.25222
- Impacts of decoder latency: https://arxiv.org/abs/2511.10633
- Google real-time decoding: https://meetings-archive.aps.org/smt/2025/mar-b16/4

## 5. Numbers checked against the primary sources (22 Nov 2026)

Read from the full texts of 2309.05558, 2301.08419, 2406.08491, 2502.14787 and
2608.10576. QUEKUF (ACM TRETS) could not be fetched (403); its row is from secondary
sources. Corrections to section 1: Helios TQE authors are Liyanage, Wu, Tagare, Zhong;
Helios names only the VCU129 board and Micro Blossom only the VMK180 board.

| | Hardware / clock | Noise, code, d | Latency metric | Numbers at p = 0.001 | Resources | Graph hard-wired? |
|---|---|---|---|---|---|---|
| Collision Clustering | XCVU3P, 401–449 MHz (Table 1); 12 nm ASIC 2 GHz | Circuit-level, SI1000-like (2q gates p; 1q, reset, idle p/10), rotated, d = 3–23 | Whole block decoded after all rounds, mean over 1e5 shots, divided by d (Table 1) | µs/round: d11 0.11, d13 0.16, d15 0.25, d17 0.37, d21 0.81, d23 1.18 | LUT/FF/BRAM: d11 4600/3591/0+1; d13 5914/4136/0+1; d15 7793/5432/1+1; d17 10446/7184/1+1 | Not stated; App. D: the distance function encodes the graph |
| Helios (TQE) | VCU129; 100 MHz target, 75 MHz at circuit-level d17 | Phenomenological + simple circuit-level (X errors only), rotated, circuit-level d ≤ 17 | Mean decoding time per round; worst case 920 ns at d13 (phenomenological) | Circuit-level d17: 23.7 ns/round (abstract) / 21.3 ns (§I); phenomenological d21 11.5 ns/round | Circuit-level LUT/reg: d13 340,084/74,927; d17 888,854/177,284 | Yes, one PE per vertex, synthesized per d |
| Micro Blossom | VMK180; d11 77 MHz, d13 62 MHz, d15 43 MHz | Circuit-level, weighted, d = 3–15 | Mean time from syndrome ready to correction ready, incl. CPU–FPGA I/O; streams rounds | 0.8 µs whole decode at d13 (not per round) | LUT: d11 314k, d13 553k, d15 867k | Yes: graph → generated Verilog, new bitstream per graph (§3.1, §7) |
| Snowflake | XCKU5P, XC7A35T, 200 MHz | Circuit-level, unrotated, d ≤ 9 on hardware | Streaming throughput per round over 1024 rounds | d9: 0.32 µs/round | LUT/FF: d7 110k/126k; d9 238k/297k | Yes, PE per node |
| QUEKUF (secondary) | Alveo U55C, ~247.5 MHz at d10 | Toric code, code capacity, d ≤ 10 | — | 7.3× (TRETS) / 20.1× (RAW) over their C++ | — | Yes, distance is a build argument |

Like-for-like for this project: Collision Clustering's Table 1 (same block-after-all-rounds
metric, mean, divided by d). At d = 11: 0.11 µs/round vs this project's 0.68. Note its
noise model puts most locations at p/10, so its shots have fewer defects than a
uniform-p Stim circuit at the same p.
