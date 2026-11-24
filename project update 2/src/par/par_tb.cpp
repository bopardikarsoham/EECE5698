// par_tb.cpp - testbench for the parallel prototype (g++, Vitis C simulation and
// C/RTL co-simulation). Checks every shot against <case>/par_expected.bin, written by
// tools/par_model.cpp --write-par-expected.
//
//   par_tb <case_dir> [max_shots]
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "../host/case_io.h"
#include "par_graph.h"

extern "C" void par_decoder(const uint32_t* dets, uint32_t* out, int num_shots);

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: par_tb <case_dir> [max_shots]\n"); return 2; }
    const std::string dir = argv[1];
    Case c;
    const std::string err = load_case(dir, c);
    if (!err.empty()) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
    if ((int)c.num_det != par::PDET || (int)c.num_edges != par::PE) {
        fprintf(stderr, "case graph (%u det, %u edges) is not the wired-in graph (%d, %d)\n",
                c.num_det, c.num_edges, par::PDET, par::PE);
        return 2;
    }
    std::vector<uint8_t> exp;
    if (!read_bytes(dir + "/par_expected.bin", exp) || exp.size() != c.shots) {
        fprintf(stderr, "missing or wrong-size %s/par_expected.bin\n", dir.c_str());
        return 2;
    }
    size_t n = c.shots;
    if (argc > 2) n = std::min(n, (size_t)atol(argv[2]));
    // at least the m_axi depth (1024 words): co-simulation copies that many words back
    std::vector<uint32_t> out(std::max<size_t>(2 * n, 1024));
    par_decoder(c.dets.data(), out.data(), (int)n);
    size_t bad = 0, invalid = 0;
    double iters = 0, floods = 0, csteps = 0;
    for (size_t s = 0; s < n; ++s) {
        if ((out[2 * s] & 0xFF) != exp[s]) ++bad;
        if ((out[2 * s] >> 8) & 0xFF) ++invalid;
        iters += out[2 * s + 1] & 0xFF;
        floods += (out[2 * s + 1] >> 8) & 0xFFF;
        csteps += out[2 * s + 1] >> 20;
    }
    printf("%s: %zu shots, %zu mismatches, %zu not OK, %.2f growth iterations, %.2f flood steps, %.2f %s per shot\n",
           dir.c_str(), n, bad, invalid, iters / n, floods / n, csteps / n,
#ifdef PAR_FUSED
           "state-machine cycles"
#else
           "parity-forest steps"
#endif
           );
    printf(bad || invalid ? "FAIL\n" : "PASS\n");
    return bad || invalid ? 1 : 0;
}
