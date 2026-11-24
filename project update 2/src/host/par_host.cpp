// par_host.cpp - XRT host program for the parallel prototype (src/par/, built with
// -DPAR_FUSED). Runs one test case on the U280, checks every shot against
// <case>/par_expected.bin, and reports the per-shot state-machine cycle counts the
// kernel writes back, plus the wall time of the whole run.
//
// Usage: par_host <xclbin> <case_dir> [mhz]
//        mhz: the kernel clock the bitstream reached (from the v++ link summary),
//             used only to turn cycles into microseconds
//
// Build: g++ -std=c++17 -O2 -I/opt/xilinx/xrt/include src/host/par_host.cpp \
//            -o build/par_host -L/opt/xilinx/xrt/lib -lxrt_coreutil -lpthread
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#include "case_io.h"

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: par_host <xclbin> <case_dir> [mhz]\n"); return 2; }
    const std::string xclbin = argv[1], dir = argv[2];
    const double mhz = argc > 3 ? atof(argv[3]) : 300.0;
    Case c;
    const std::string err = load_case(dir, c);
    if (!err.empty()) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
    std::vector<uint8_t> exp;
    if (!read_bytes(dir + "/par_expected.bin", exp) || exp.size() != c.shots) {
        fprintf(stderr, "missing or wrong-size %s/par_expected.bin\n", dir.c_str());
        return 2;
    }
    const size_t n = c.shots;

    auto device = xrt::device(0);
    auto uuid = device.load_xclbin(xclbin);
    auto kernel = xrt::kernel(device, uuid, "par_decoder");
    auto bo_dets = xrt::bo(device, c.dets.size() * sizeof(uint32_t), kernel.group_id(0));
    auto bo_out = xrt::bo(device, 2 * n * sizeof(uint32_t), kernel.group_id(1));
    bo_dets.write(c.dets.data());
    bo_dets.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    const auto t0 = std::chrono::high_resolution_clock::now();
    auto run = kernel(bo_dets, bo_out, (int)n);
    run.wait();
    const auto t1 = std::chrono::high_resolution_clock::now();
    const double wall_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::vector<uint32_t> out(2 * n);
    bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_out.read(out.data());

    size_t bad = 0, invalid = 0;
    double iters = 0, floods = 0;
    std::vector<uint32_t> cyc(n);
    for (size_t s = 0; s < n; ++s) {
        if ((out[2 * s] & 0xFF) != exp[s]) ++bad;
        if ((out[2 * s] >> 8) & 0xFF) ++invalid;
        iters += out[2 * s + 1] & 0xFF;
        floods += (out[2 * s + 1] >> 8) & 0xFFF;
        cyc[s] = out[2 * s + 1] >> 20;
    }
    double mean = 0;
    for (auto x : cyc) mean += x;
    mean /= n;
    std::vector<uint32_t> srt = cyc;
    std::sort(srt.begin(), srt.end());
    auto pct = [&](double q) { return srt[std::min(n - 1, (size_t)(q * n))]; };
    printf("%s: %zu shots, %zu mismatches, %zu not OK\n", dir.c_str(), n, bad, invalid);
    printf("growth iterations %.2f, flood steps %.2f per shot\n", iters / n, floods / n);
    printf("state-machine cycles per shot: mean %.2f, p99 %u, p99.9 %u, max %u\n", mean,
           pct(0.99), pct(0.999), srt[n - 1]);
    printf("at %.1f MHz: mean %.4f us, p99.9 %.4f us per shot (decode latency)\n", mhz,
           mean / mhz, pct(0.999) / mhz);
    printf("wall time %.1f us for the run: %.4f us per shot (throughput, includes launch and I/O)\n",
           wall_us, wall_us / n);
    printf(bad || invalid ? "FAIL\n" : "PASS\n");
    return bad || invalid ? 1 : 0;
}
