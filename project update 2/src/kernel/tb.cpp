// tb.cpp - testbench for the FPGA kernel (src/kernel/qec_decoder.cpp).
//
// Usage: kernel_tb [--per-shot N | --batch N] [--time] [--work-csv <file>] <case_dir> ...
//
// For each case it calls qec_decoder() exactly as the FPGA would run it, then
// requires: predictions identical to the golden output for every shot, 0 invalid
// corrections and 0 cap hits. It also prints the distribution of per-shot work
// (inner-loop iterations, the kernel's cycle proxy).
//
//   --per-shot N      load the graph in one call, then decode only the first N shots,
//                     one shot per call. Under C/RTL co-simulation every call is one
//                     transaction, so the co-simulation report gives the exact clock
//                     cycles per shot (used to calibrate work -> cycles).
//   --batch N         decode only the first N shots, all in one call (how the host runs
//                     the FPGA). Under co-simulation, total cycles / N is the cost per
//                     shot including the burst reads and writes.
//   --time            CPU baseline: time the decode on this machine. The graph is loaded
//                     in its own call first, so only decoding is timed (no file I/O).
//                     Batch: one timed call for all shots (mean only). With --per-shot,
//                     every call is timed, which also gives p50 / p99 / p99.9 / max.
//   --work-csv <file> write one line per shot: case,shot,grow_iters,work, then the
//                     defects and the per-stage loop counts (info_layout.h)
//
// Runs as a plain g++ program (make kernel_csim) or as the Vitis HLS C simulation and
// co-simulation testbench (make hls_csim, hls_synth, hls_cosim).

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../host/case_io.h"
#include "../info_layout.h"

extern "C" void qec_decoder(const uint32_t* graph, const uint32_t* dets, uint32_t* pred,
                            uint32_t* info, int num_shots, int load_graph);

// Value that at least a fraction q of the sorted values stay within.
static unsigned percentile(const std::vector<unsigned>& sorted, double q) {
    if (sorted.empty()) return 0;
    size_t k = (size_t)(q * (double)sorted.size());
    if (k > 0 && (double)k == q * (double)sorted.size()) --k;
    return sorted[std::min(k, sorted.size() - 1)];
}

int main(int argc, char** argv) {
    long per_shot = -1, batch = -1;
    bool timing = false;
    const char* csv_path = nullptr;
    std::vector<std::string> dirs;
    for (int a = 1; a < argc; ++a) {
        const std::string arg = argv[a];
        if (arg == "--per-shot" && a + 1 < argc) per_shot = std::atol(argv[++a]);
        else if (arg == "--batch" && a + 1 < argc) batch = std::atol(argv[++a]);
        else if (arg == "--time") timing = true;
        else if (arg == "--work-csv" && a + 1 < argc) csv_path = argv[++a];
        else dirs.push_back(arg);
    }
    if (dirs.empty()) {
        std::fprintf(stderr, "usage: kernel_tb [--per-shot N | --batch N] [--time] [--work-csv <file>] <case_dir> ...\n");
        return 2;
    }
    FILE* csv = nullptr;
    if (csv_path) {
        csv = std::fopen(csv_path, "w");
        if (!csv) {
            std::fprintf(stderr, "kernel_tb: cannot write %s\n", csv_path);
            return 2;
        }
        std::fprintf(csv, "case,shot,grow_iters,work");
        for (const char* name : INFO_STAGE_NAMES) std::fprintf(csv, ",%s", name);
        std::fprintf(csv, "\n");
    }

    int passed = 0;
    for (std::string dir : dirs) {
        while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
        Case c;
        const std::string err = load_case(dir, c);
        if (!err.empty()) {
            std::printf("FAIL  %s: %s\n", dir.c_str(), err.c_str());
            continue;
        }

        const long limit = per_shot >= 0 ? per_shot : batch;
        const size_t shots = limit >= 0 ? std::min((size_t)limit, c.shots) : c.shots;
        // C/RTL co-simulation copies every buffer at its full interface depth (see the
        // INTERFACE pragmas in qec_decoder.cpp), so the buffers are padded to at least
        // that size.
        std::vector<uint32_t> graph(c.graph);
        graph.resize(std::max(graph.size(), (size_t)32771), 0);
        std::vector<uint32_t> pred(std::max(shots, (size_t)256), 0);
        std::vector<uint32_t> info(std::max(INFO_WORDS * shots, (size_t)INFO_WORDS * 256), 0);
        using clock = std::chrono::steady_clock;
        std::vector<double> shot_ns;   // --time --per-shot: time of every call
        double batch_ns = 0;
        if (per_shot >= 0) {
            qec_decoder(graph.data(), c.dets.data(), pred.data(), info.data(), 0, 1);
            if (timing) shot_ns.resize(shots);
            for (size_t s = 0; s < shots; ++s) {
                const auto t0 = clock::now();
                qec_decoder(graph.data(), &c.dets[s * c.det_words], &pred[s], &info[INFO_WORDS * s], 1, 0);
                if (timing) shot_ns[s] = std::chrono::duration<double, std::nano>(clock::now() - t0).count();
            }
        } else if (timing) {
            qec_decoder(graph.data(), c.dets.data(), pred.data(), info.data(), 0, 1);
            const auto t0 = clock::now();
            qec_decoder(graph.data(), c.dets.data(), pred.data(), info.data(), (int)shots, 0);
            batch_ns = std::chrono::duration<double, std::nano>(clock::now() - t0).count();
        } else {
            qec_decoder(graph.data(), c.dets.data(), pred.data(), info.data(), (int)shots, 1);
        }

        size_t mismatches = 0, invalid = 0, cap_hits = 0, no_graph = 0;
        std::vector<unsigned> iters(shots), work(shots);
        for (size_t s = 0; s < shots; ++s) {
            const uint32_t* rec = &info[INFO_WORDS * s];
            if (rec[INFO_STATUS] == 0xFFFFFFFFu) { ++no_graph; continue; }
            const unsigned status = rec[INFO_STATUS] >> 16;
            iters[s] = rec[INFO_STATUS] & 0xFFFF;
            work[s] = rec[INFO_WORK];
            if (status == 1) ++cap_hits;
            if (status == 2) ++invalid;
            if (pred[s] != c.expected[s]) ++mismatches;
            if (csv) {
                std::fprintf(csv, "%s,%zu,%u,%u", dir.c_str(), s, iters[s], work[s]);
                for (int k = INFO_DEFECTS; k < INFO_WORDS; ++k) std::fprintf(csv, ",%u", rec[k]);
                std::fprintf(csv, "\n");
            }
        }
        if (no_graph) {
            std::printf("FAIL  %s: graph (%u detectors, %u edges) exceeds the kernel limits\n",
                        dir.c_str(), c.num_det, c.num_edges);
            continue;
        }
        if (mismatches || invalid || cap_hits) {
            std::printf("FAIL  %s: %zu / %zu shots differ from golden, %zu invalid, %zu cap hits\n",
                        dir.c_str(), mismatches, shots, invalid, cap_hits);
            continue;
        }
        double mean = 0;
        for (unsigned w : work) mean += w;
        mean /= shots ? (double)shots : 1.0;
        std::sort(work.begin(), work.end());
        std::printf("PASS  %s (%zu shots%s match golden, 0 invalid, 0 cap hits, max %u growth iterations)\n"
                    "      work per shot: mean %.1f, p50 %u, p99 %u, p99.9 %u, max %u\n",
                    dir.c_str(), shots, per_shot >= 0 ? ", one per call," : "",
                    *std::max_element(iters.begin(), iters.end()), mean,
                    percentile(work, 0.5), percentile(work, 0.99), percentile(work, 0.999),
                    work.empty() ? 0u : work.back());
        if (timing && !shot_ns.empty()) {
            std::sort(shot_ns.begin(), shot_ns.end());
            double sum = 0;
            for (double t : shot_ns) sum += t;
            auto pct = [&](double q) {
                return shot_ns[std::min((size_t)(q * (double)shot_ns.size()), shot_ns.size() - 1)];
            };
            std::printf("      CPU time per shot (one call each): mean %.3f us, p50 %.3f, p99 %.3f, "
                        "p99.9 %.3f, max %.3f\n", sum / shot_ns.size() / 1e3, pct(0.5) / 1e3,
                        pct(0.99) / 1e3, pct(0.999) / 1e3, shot_ns.back() / 1e3);
        } else if (timing) {
            std::printf("      CPU time per shot (one batch call): mean %.3f us\n",
                        shots ? batch_ns / (double)shots / 1e3 : 0.0);
        }
        ++passed;
    }
    if (csv) std::fclose(csv);
    std::printf("kernel_tb: %d / %zu cases PASS\n", passed, dirs.size());
    return passed == (int)dirs.size() ? 0 : 1;
}
