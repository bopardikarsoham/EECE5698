// case_io.h - load a test case directory into the word buffers the FPGA kernel takes.
// Shared by the kernel testbench (src/kernel/tb.cpp) and the XRT host program
// (src/host/host.cpp). Host-side only: uses std::vector, never synthesized.
//
// Reads from <dir>:
//   graph.txt          decoding graph (same format as src/golden.cpp reads)
//   dets.bin           detection events, ceil(num_det/8) bytes per shot
//   expected_pred.bin  golden predictions, ceil(num_obs/8) bytes per shot
//                      (falls back to sw_pred.bin for data/ directories)

#ifndef CASE_IO_H
#define CASE_IO_H

#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

struct Case {
    uint32_t num_det = 0, num_obs = 0, num_edges = 0;
    size_t shots = 0;
    uint32_t det_words = 0;           // 32-bit words per shot in dets
    std::vector<uint32_t> graph;      // kernel graph buffer
    std::vector<uint32_t> dets;       // kernel detection-event buffer
    std::vector<uint32_t> expected;   // golden prediction per shot
};

static bool read_bytes(const std::string& path, std::vector<uint8_t>& data) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// Returns an empty string on success, otherwise what went wrong.
static std::string load_case(const std::string& dir, Case& c) {
    std::ifstream in(dir + "/graph.txt");
    if (!in) return "cannot open " + dir + "/graph.txt";
    std::string line;
    bool have_header = false;
    uint32_t edges_read = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        if (!have_header) {
            ss >> c.num_det >> c.num_obs >> c.num_edges;
            if (!ss || c.num_det < 1 || c.num_obs < 1 || c.num_obs > 32 || c.num_edges < 1)
                return "bad graph header in " + dir + "/graph.txt";
            c.graph = {c.num_det, c.num_obs, c.num_edges};
            have_header = true;
            continue;
        }
        long long u, v, w;
        unsigned long obs;
        ss >> u >> v >> w >> obs;
        if (v < 0) v = c.num_det;   // -1 in the file means the boundary
        if (!ss || edges_read >= c.num_edges || u < 0 || u >= c.num_det || v > c.num_det ||
            u == v || w < 1)
            return "bad edge line in " + dir + "/graph.txt: " + line;
        c.graph.insert(c.graph.end(), {(uint32_t)u, (uint32_t)v, (uint32_t)w, (uint32_t)obs});
        ++edges_read;
    }
    if (!have_header || edges_read != c.num_edges)
        return "wrong number of edges in " + dir + "/graph.txt";

    std::vector<uint8_t> raw;
    if (!read_bytes(dir + "/dets.bin", raw)) return "cannot open " + dir + "/dets.bin";
    const size_t det_bytes = (c.num_det + 7) / 8;
    if (raw.size() % det_bytes != 0) return "dets.bin size does not match num_det";
    c.shots = raw.size() / det_bytes;
    c.det_words = (c.num_det + 31) / 32;
    c.dets.assign(c.shots * c.det_words, 0);
    for (size_t s = 0; s < c.shots; ++s)
        for (size_t b = 0; b < det_bytes; ++b)
            c.dets[s * c.det_words + b / 4] |= (uint32_t)raw[s * det_bytes + b] << (8 * (b % 4));

    if (!read_bytes(dir + "/expected_pred.bin", raw) && !read_bytes(dir + "/sw_pred.bin", raw))
        return "no expected_pred.bin or sw_pred.bin in " + dir;
    const size_t obs_bytes = (c.num_obs + 7) / 8;
    if (raw.size() != c.shots * obs_bytes) return "golden prediction file has the wrong size";
    c.expected.assign(c.shots, 0);
    for (size_t s = 0; s < c.shots; ++s)
        for (size_t b = 0; b < obs_bytes; ++b)
            c.expected[s] |= (uint32_t)raw[s * obs_bytes + b] << (8 * b);
    return "";
}

#endif  // CASE_IO_H
