// qec_decoder.cpp - FPGA kernel: the union-find decoder, sequential version. Uses
// uf::decode_fast (uf_fast.h), which gives the same results as uf::decode but only
// touches the clusters and their frontier edges. Detection words and results move in
// bursts of SHOT_BLOCK shots. The same source compiles with plain
// g++, so the testbench (tb.cpp) can be run without the Xilinx tools.
//
// Arguments (all buffers in device memory, 32-bit words):
//   graph       decoding graph: num_det, num_obs, num_edges, then 4 words per edge
//               (u, v, weight, obs_mask) with v == num_det meaning the boundary
//   dets        detection events, ceil(num_det/32) words per shot,
//               bit i of the shot = word i/32, bit i%32
//   pred        out: predicted observable flips, one word per shot (bit k = observable k)
//   info        out: two words per shot:
//                 info[2s]   = (status << 16) | growth iterations;
//                              status 0 = OK, 1 = cap hit, 2 = invalid correction
//                 info[2s+1] = work: inner-loop iterations the shot needed (cycle proxy)
//               info[2s] = 0xFFFFFFFF means no usable graph is loaded (too large, or
//               never loaded).
//   num_shots   number of shots in dets (0 is allowed: only load the graph)
//   load_graph  nonzero: copy the graph into on-chip memory first. The graph stays
//               loaded between calls, so later calls can pass 0 and skip the copy.
//               This lets the testbench decode one shot per call and get the exact
//               clock cycles of each shot from C/RTL co-simulation.

// Kernel limits: rotated surface code up to d = 11 with d rounds (1320 detectors,
// 6718 edges). Must be defined before uf_core.h is included.
#define UF_MAX_DET 2048
#define UF_MAX_EDGES 8192

#include <cstdint>

#include "../uf_fast.h"

static const uint32_t NO_GRAPH = 0xFFFFFFFFu;
static const int SHOT_BLOCK = 64;                      // shots per burst
static const int MAX_DET_WORDS = (uf::MAX_DET + 31) / 32;

extern "C" {
void qec_decoder(const uint32_t* graph, const uint32_t* dets, uint32_t* pred,
                 uint32_t* info, int num_shots, int load_graph) {
// depth= only sizes the memories in C/RTL co-simulation (up to 256 d = 3 shots).
#pragma HLS INTERFACE m_axi port=graph bundle=gmem0 offset=slave depth=32771
#pragma HLS INTERFACE m_axi port=dets  bundle=gmem1 offset=slave depth=4096
#pragma HLS INTERFACE m_axi port=pred  bundle=gmem2 offset=slave depth=256
#pragma HLS INTERFACE m_axi port=info  bundle=gmem3 offset=slave depth=512
#pragma HLS INTERFACE s_axilite port=graph      bundle=control
#pragma HLS INTERFACE s_axilite port=dets       bundle=control
#pragma HLS INTERFACE s_axilite port=pred       bundle=control
#pragma HLS INTERFACE s_axilite port=info       bundle=control
#pragma HLS INTERFACE s_axilite port=num_shots  bundle=control
#pragma HLS INTERFACE s_axilite port=load_graph bundle=control
#pragma HLS INTERFACE s_axilite port=return     bundle=control

    // On-chip copies of the graph and the decoder's scratch space. Static, so the
    // graph persists between calls.
    static uf::Graph g;
    static uf::FastWorkspace ws;
    static uint8_t defect[uf::MAX_DET];   // all 0 between shots
    static int dl[uf::MAX_DET];           // fired detectors of the current shot
    static bool graph_ok = false;

    if (load_graph) {
        const int num_det = (int)graph[0];
        const int num_obs = (int)graph[1];
        const int num_edges = (int)graph[2];
        graph_ok = num_det >= 1 && num_det <= uf::MAX_DET && num_obs >= 1 && num_obs <= 32 &&
                   num_edges >= 1 && num_edges <= uf::MAX_EDGES;
        if (graph_ok) {
            g.num_det = num_det;
            g.num_obs = num_obs;
            g.num_edges = num_edges;
        load_edges:
            for (int e = 0; e < num_edges; ++e) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=8192
                g.eu[e] = (int)graph[3 + 4 * e];
                g.ev[e] = (int)graph[4 + 4 * e];
                g.w[e] = graph[5 + 4 * e];
                g.obs[e] = graph[6 + 4 * e];
            }
            uf::build_adjacency(g);
            graph_ok = uf::init_fast(g, ws);
        }
    }

    if (!graph_ok) {
        for (int s = 0; s < num_shots; ++s) {
            pred[s] = 0;
            info[2 * s] = NO_GRAPH;
            info[2 * s + 1] = 0;
        }
        return;
    }

    // Shots are processed in blocks: one burst read brings in a block's detection
    // words, and the block's results go back in one burst per output.
    const int det_words = (g.num_det + 31) / 32;
    static uint32_t dbuf[SHOT_BLOCK * MAX_DET_WORDS];
    static uint32_t pbuf[SHOT_BLOCK];
    static uint32_t ibuf[2 * SHOT_BLOCK];
decode_blocks:
    for (int s0 = 0; s0 < num_shots; s0 += SHOT_BLOCK) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=1563
        const int nb = (num_shots - s0 < SHOT_BLOCK) ? num_shots - s0 : SHOT_BLOCK;
    read_dets:
        for (int i = 0; i < nb * det_words; ++i) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=4096
#pragma HLS PIPELINE II=1
            dbuf[i] = dets[s0 * det_words + i];
        }
    decode_shots:
        for (int t = 0; t < nb; ++t) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
            // Unpack the fired detectors: one step per set bit.
            int nd = 0;
        unpack:
            for (int wd = 0; wd < det_words; ++wd) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
                uint32_t word = dbuf[t * det_words + wd];
            unpack_bits:
                while (word) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=32
#pragma HLS PIPELINE II=1
                    const int i = 32 * wd + __builtin_ctz(word);
                    word &= word - 1;
                    if (i < g.num_det) {
                        defect[i] = 1;
                        dl[nd++] = i;
                    }
                }
            }
            const uf::Result r = uf::decode_fast(g, defect, dl, nd, ws);
        clear_defects:
            for (int k = 0; k < nd; ++k) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=2048
                defect[dl[k]] = 0;
            }
            pbuf[t] = r.obs_flip;
            ibuf[2 * t] = ((uint32_t)r.status << 16) | (uint32_t)r.grow_iters;
            ibuf[2 * t + 1] = r.work + (uint32_t)det_words + 2 * (uint32_t)nd;
        }
    write_pred:
        for (int t = 0; t < nb; ++t) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
#pragma HLS PIPELINE II=1
            pred[s0 + t] = pbuf[t];
        }
    write_info:
        for (int t = 0; t < 2 * nb; ++t) {
#pragma HLS LOOP_TRIPCOUNT min=2 max=128
#pragma HLS PIPELINE II=1
            info[2 * s0 + t] = ibuf[t];
        }
    }
}
}
