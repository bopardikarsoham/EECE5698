// host.cpp - XRT host program: runs the union-find kernel on the U280 for one test
// case and checks every shot against the golden output.
//
// Usage: host [<xclbin>] [<case_dir>] [<work_csv>]
//        defaults: build/qec_decoder.xclbin, tests/cases/surf_z_d3_p0.01_n100000
//        work_csv: optional, one line per shot: shot,grow_iters,work, then the
//                  defects and the per-stage loop counts the kernel reports

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#include "case_io.h"
#include "../info_layout.h"

int main(int argc, char** argv) {
    const std::string xclbin_path = argc > 1 ? argv[1] : "build/qec_decoder.xclbin";
    const std::string case_dir = argc > 2 ? argv[2] : "tests/cases/surf_z_d3_p0.01_n100000";
    std::cout << "Starting QEC FPGA Host Application...\n";

    // 1. Load the test case: graph, detection events and golden predictions
    Case c;
    const std::string err = load_case(case_dir, c);
    if (!err.empty()) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }
    std::cout << "Case " << case_dir << ": " << c.num_det << " detectors, " << c.num_edges
              << " edges, " << c.shots << " shots\n";

    // 2. Load the FPGA device (Index 0) and the compiled bitstream
    auto device = xrt::device(0);
    std::cout << "Connected to device: " << device.get_info<xrt::info::device::name>() << "\n";
    std::cout << "Loading bitstream: " << xclbin_path << "\n";
    auto uuid = device.load_xclbin(xclbin_path);
    auto kernel = xrt::kernel(device, uuid, "qec_decoder");

    // 3. Allocate device buffers in the memory banks the kernel arguments map to
    const size_t out_bytes = c.shots * sizeof(uint32_t);
    auto bo_graph = xrt::bo(device, c.graph.size() * sizeof(uint32_t), kernel.group_id(0));
    auto bo_dets  = xrt::bo(device, c.dets.size() * sizeof(uint32_t), kernel.group_id(1));
    auto bo_pred  = xrt::bo(device, out_bytes, kernel.group_id(2));
    auto bo_info  = xrt::bo(device, INFO_WORDS * out_bytes, kernel.group_id(3));

    // 4. Copy the inputs to the card (not included in the decode time)
    bo_graph.write(c.graph.data());
    bo_graph.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_dets.write(c.dets.data());
    bo_dets.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // 5. Run the kernel: (graph, dets, pred, info, num_shots, load_graph)
    std::cout << "Running the decoder kernel...\n";
    const auto t0 = std::chrono::high_resolution_clock::now();
    auto run = kernel(bo_graph, bo_dets, bo_pred, bo_info, (int)c.shots, 1);
    run.wait();
    const auto t1 = std::chrono::high_resolution_clock::now();
    const double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    // 6. Read the results back and compare with the golden output
    std::vector<uint32_t> pred(c.shots), info(INFO_WORDS * c.shots);
    bo_pred.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_pred.read(pred.data());
    bo_info.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_info.read(info.data());

    if (c.shots && info[0] == 0xFFFFFFFFu) {
        std::cerr << "Error: graph exceeds the kernel limits\n";
        return 1;
    }
    size_t mismatches = 0, invalid = 0, cap_hits = 0;
    unsigned max_iters = 0;
    double total_work = 0;
    double stage_total[INFO_WORDS] = {};
    for (size_t s = 0; s < c.shots; ++s) {
        const uint32_t* rec = &info[INFO_WORDS * s];
        const unsigned status = rec[INFO_STATUS] >> 16, iters = rec[INFO_STATUS] & 0xFFFF;
        total_work += rec[INFO_WORK];
        for (int k = INFO_DEFECTS; k < INFO_WORDS; ++k) stage_total[k] += rec[k];
        if (status == 1) ++cap_hits;
        if (status == 2) ++invalid;
        if (iters > max_iters) max_iters = iters;
        if (pred[s] != c.expected[s]) ++mismatches;
    }
    if (argc > 3) {
        std::ofstream csv(argv[3]);
        csv << "shot,grow_iters,work";
        for (const char* name : INFO_STAGE_NAMES) csv << "," << name;
        csv << "\n";
        for (size_t s = 0; s < c.shots; ++s) {
            const uint32_t* rec = &info[INFO_WORDS * s];
            csv << s << "," << (rec[INFO_STATUS] & 0xFFFF) << "," << rec[INFO_WORK];
            for (int k = INFO_DEFECTS; k < INFO_WORDS; ++k) csv << "," << rec[k];
            csv << "\n";
        }
    }

    std::cout << "Kernel time: " << us << " us for " << c.shots << " shots ("
              << us / c.shots << " us/shot, includes kernel launch)\n"
              << "Max growth iterations: " << max_iters << "\n"
              << "Total work: " << total_work << " inner-loop iterations ("
              << total_work / us << " per us; compare with the clock rate)\n"
              << "Per shot, counted on the card:";
    for (int k = INFO_DEFECTS; k < INFO_WORDS; ++k)
        std::cout << " " << INFO_STAGE_NAMES[k - INFO_DEFECTS] << "="
                  << stage_total[k] / (c.shots ? (double)c.shots : 1.0);
    std::cout << "\n"
              << "Invalid corrections: " << invalid << ", cap hits: " << cap_hits << "\n"
              << "Shots that differ from golden: " << mismatches << " / " << c.shots << "\n";
    const bool ok = !mismatches && !invalid && !cap_hits;
    std::cout << (ok ? "PASS" : "FAIL") << ": FPGA output vs golden\n";
    return ok ? 0 : 1;
}
