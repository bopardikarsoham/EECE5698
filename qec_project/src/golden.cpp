// golden.cpp - software (CPU) version of the QEC union-find decoder.
//
// Usage: golden <case_dir> [--out <pred.bin>] [--stats <stats.txt>]
//
// Reads from <case_dir>:
//   graph.txt  decoding graph written by python/gen_data.py or python/make_toy.py
//   dets.bin   detection events, one shot per ceil(num_det/8) bytes, bit i of
//              the shot = detector i (little-endian bit order, same as Stim's b8)
// Writes:
//   sw_pred.bin   predicted observable flips, one shot per ceil(num_obs/8) bytes
//   sw_stats.txt  shot count, timing, invalid corrections and cap hits

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "uf_core.h"

static bool load_graph(const std::string& path, uf::Graph& g) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "golden: cannot open " << path << "\n";
        return false;
    }
    std::string line;
    bool have_header = false;
    int edges_read = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        if (!have_header) {
            ss >> g.num_det >> g.num_obs >> g.num_edges;
            if (!ss || g.num_det < 1 || g.num_det > uf::MAX_DET || g.num_obs < 1 ||
                g.num_obs > 32 || g.num_edges < 1 || g.num_edges > uf::MAX_EDGES) {
                std::cerr << "golden: bad or too large graph header in " << path << "\n";
                return false;
            }
            have_header = true;
            continue;
        }
        int u, v;
        long long w;
        unsigned long obs;
        ss >> u >> v >> w >> obs;
        if (!ss || edges_read >= g.num_edges) {
            std::cerr << "golden: bad edge line in " << path << ": " << line << "\n";
            return false;
        }
        if (v < 0) v = g.num_det;   // -1 in the file means the boundary
        if (u < 0 || u >= g.num_det || v > g.num_det || u == v || w < 1) {
            std::cerr << "golden: bad edge in " << path << ": " << line << "\n";
            return false;
        }
        g.eu[edges_read] = u;
        g.ev[edges_read] = v;
        g.w[edges_read] = (uint32_t)w;
        g.obs[edges_read] = (uint32_t)obs;
        ++edges_read;
    }
    if (!have_header || edges_read != g.num_edges) {
        std::cerr << "golden: expected " << g.num_edges << " edges in " << path << ", got "
                  << edges_read << "\n";
        return false;
    }
    uf::build_adjacency(g);
    return true;
}

static bool read_file(const std::string& path, std::vector<uint8_t>& data) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "golden: cannot open " << path << "\n";
        return false;
    }
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: golden <case_dir> [--out <pred.bin>] [--stats <stats.txt>]\n";
        return 2;
    }
    const std::string dir = argv[1];
    std::string out_path = dir + "/sw_pred.bin";
    std::string stats_path = dir + "/sw_stats.txt";
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        if (flag == "--out") out_path = argv[i + 1];
        else if (flag == "--stats") stats_path = argv[i + 1];
        else {
            std::cerr << "golden: unknown option " << flag << "\n";
            return 2;
        }
    }

    // Graph and workspace are large fixed-size structs, so keep them off the stack.
    auto g = std::make_unique<uf::Graph>();
    auto ws = std::make_unique<uf::Workspace>();
    if (!load_graph(dir + "/graph.txt", *g)) return 1;

    std::vector<uint8_t> dets;
    if (!read_file(dir + "/dets.bin", dets)) return 1;
    const size_t det_bytes = (g->num_det + 7) / 8;
    const size_t obs_bytes = (g->num_obs + 7) / 8;
    if (dets.size() % det_bytes != 0) {
        std::cerr << "golden: dets.bin size " << dets.size() << " is not a multiple of "
                  << det_bytes << " bytes per shot\n";
        return 1;
    }
    const size_t shots = dets.size() / det_bytes;

    std::vector<uint8_t> pred(shots * obs_bytes, 0);
    std::vector<uint8_t> defect(g->num_det);
    size_t invalid = 0, cap_hits = 0, nontrivial = 0;
    long long total_iters = 0;
    int max_iters = 0;
    // Histogram of growth iterations per shot, for the tail (worst-case) statistics.
    std::vector<size_t> iter_hist(uf::MAX_GROW_ITERS + 1, 0);

    const auto t0 = std::chrono::steady_clock::now();
    for (size_t s = 0; s < shots; ++s) {
        const uint8_t* row = &dets[s * det_bytes];
        for (int i = 0; i < g->num_det; ++i) defect[i] = (row[i >> 3] >> (i & 7)) & 1;
        const uf::Result r = uf::decode(*g, defect.data(), *ws);
        if (r.status == uf::INVALID) ++invalid;
        if (r.status == uf::CAP_HIT) ++cap_hits;
        if (r.grow_iters > 0) ++nontrivial;
        total_iters += r.grow_iters;
        if (r.grow_iters > max_iters) max_iters = r.grow_iters;
        ++iter_hist[r.grow_iters];
        for (int k = 0; k < g->num_obs; ++k)
            if ((r.obs_flip >> k) & 1) pred[s * obs_bytes + (k >> 3)] |= (uint8_t)(1u << (k & 7));
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double us_per_shot = shots ? ms * 1000.0 / shots : 0.0;

    // Smallest iteration count that at least a fraction q of the shots stay within.
    auto iter_percentile = [&](double q) {
        const double need = q * (double)shots;
        size_t seen = 0;
        for (size_t k = 0; k < iter_hist.size(); ++k) {
            seen += iter_hist[k];
            if ((double)seen >= need) return (int)k;
        }
        return max_iters;
    };
    const double mean_iters = shots ? (double)total_iters / shots : 0.0;
    const int p50 = iter_percentile(0.5), p99 = iter_percentile(0.99),
              p999 = iter_percentile(0.999);

    std::ofstream out(out_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(pred.data()), (std::streamsize)pred.size());
    if (!out) {
        std::cerr << "golden: cannot write " << out_path << "\n";
        return 1;
    }

    char timing[128];
    std::snprintf(timing, sizeof timing, "%.3f ms (%.3f us/shot)", ms, us_per_shot);
    std::cout << "golden: case            = " << dir << "\n"
              << "golden: detectors       = " << g->num_det << ", observables = " << g->num_obs
              << ", edges = " << g->num_edges << "\n"
              << "golden: shots           = " << shots << " (" << nontrivial
              << " with at least one defect)\n"
              << "golden: decode time     = " << timing << "\n"
              << "golden: growth iters    = mean " << mean_iters << ", median " << p50
              << ", p99 " << p99 << ", p99.9 " << p999 << ", max " << max_iters << "\n"
              << "golden: invalid corrections = " << invalid << "\n"
              << "golden: cap hits        = " << cap_hits << "\n"
              << "golden: wrote " << out_path << "\n";

    std::ofstream st(stats_path);
    st << "shots=" << shots << "\n"
       << "nontrivial_shots=" << nontrivial << "\n"
       << "decode_ms=" << ms << "\n"
       << "us_per_shot=" << us_per_shot << "\n"
       << "mean_grow_iters=" << mean_iters << "\n"
       << "p50_grow_iters=" << p50 << "\n"
       << "p99_grow_iters=" << p99 << "\n"
       << "p999_grow_iters=" << p999 << "\n"
       << "max_grow_iters=" << max_iters << "\n"
       << "invalid=" << invalid << "\n"
       << "cap_hits=" << cap_hits << "\n";
    return (invalid || cap_hits) ? 3 : 0;
}
