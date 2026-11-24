// uf_fast.h - the union-find decoder of uf_core.h, reorganized so that a shot only
// touches the part of the graph near its defects.
//
// uf::decode scans every node and every edge in every growth iteration, and resets
// every array at the start of every shot, so its work grows with the size of the code
// (about d^3 per iteration) even when a shot has only a few small clusters. Here:
//   - each cluster keeps a linked list of its members, and only the active clusters'
//     members and their edges (the frontier) are visited in a growth iteration;
//   - the list of active clusters is carried from one iteration to the next (only an
//     active cluster can grow and merge, so new active clusters come from old ones);
//   - after a shot, only the nodes and edges it touched are reset;
//   - parent[] always points straight at the cluster root (merging relabels the
//     smaller cluster's members), so no root search is needed inside the loops;
//   - every detector's edges sit in MAX_DEG fixed slots, so the frontier scan is a
//     plain loop nest that HLS can pipeline at one edge per clock cycle.
// The growth, merges and peeling order are the same as uf::decode, so the result
// (prediction, status and growth iterations) is identical; tb.cpp checks this
// against the golden output.

#ifndef UF_FAST_H
#define UF_FAST_H

#include "uf_core.h"

namespace uf {

// Edge slots per detector. Rotated and unrotated surface codes (and the repetition
// code) need at most 12; graphs with more are rejected by init_fast.
#ifndef UF_MAX_DEG
#define UF_MAX_DEG 12
#endif
constexpr int MAX_DEG = UF_MAX_DEG;

// Loop iterations of each decoder stage in one decode_fast call. The kernel returns
// them with every shot, so the hardware reports where its time went (multiply by a
// stage's II to get cycles). Each is added once per loop, outside the loop body.
struct StageCounts {
    uint32_t members;     // member-list entries built (active clusters' nodes)
    uint32_t frontier;    // edge slots scanned in the frontier loop
    uint32_t apply;       // frontier edges grown
    uint32_t merges;      // fully grown edges merged
    uint32_t relabel;     // nodes relabelled by merges
    uint32_t compact;     // active-list entries compacted
    uint32_t peel_scan;   // edges scanned by the peeling search
    uint32_t peel_walk;   // forest nodes walked (and checked)
    uint32_t cleanup;     // edges and defects reset after the shot
};

struct FastWorkspace {
    int adjp[MAX_DET * MAX_DEG];  // edges of detector v in slots v*MAX_DEG.., -1 = unused
    int parent[MAX_NODES];
    int size[MAX_NODES];
    uint8_t odd[MAX_NODES];
    uint8_t bnd[MAX_NODES];
    int head[MAX_NODES];          // member list of a cluster: head/tail valid at roots
    int tail[MAX_NODES];
    int next[MAX_NODES];          // -1 ends the list
    int act[MAX_DET];             // roots of the active clusters
    int vl[MAX_DET];              // members of the active clusters (frontier scan)
    int cand[MAX_DET];            // merged root of each old active cluster
    int owner[MAX_NODES];         // first position of a root in cand
    uint32_t support[MAX_EDGES];
    uint8_t grown[MAX_EDGES];
    int fe[MAX_EDGES];            // frontier edges of this iteration
    uint8_t fr[MAX_EDGES];        // and their rates (1 or 2)
    int jg[MAX_EDGES];            // edges fully grown in this iteration
    int touched[MAX_EDGES];       // edges with nonzero support (reset after the shot)
    int bl[MAX_EDGES];            // grown edges that touch the boundary
    // peeling
    uint8_t visited[MAX_NODES];
    int par_edge[MAX_NODES];
    int order[MAX_NODES];
    uint8_t residual[MAX_NODES];
    uint32_t work;
    StageCounts sc;
};

inline void reset_node(FastWorkspace& ws, int i, int B) {
    ws.parent[i] = i;
    ws.size[i] = 1;
    ws.odd[i] = 0;
    ws.bnd[i] = (i == B);
    ws.head[i] = i;
    ws.tail[i] = i;
    ws.next[i] = -1;
}

// Call once after the graph is loaded (after build_adjacency); decode_fast leaves the
// workspace clean. Returns false if a detector has more than MAX_DEG edges.
inline bool init_fast(const Graph& g, FastWorkspace& ws) {
    const int B = g.num_det;
    for (int v = 0; v < B; ++v) {
        const int deg = g.adj_off[v + 1] - g.adj_off[v];
        if (deg > MAX_DEG) return false;
        for (int k = 0; k < MAX_DEG; ++k)
            ws.adjp[v * MAX_DEG + k] = (k < deg) ? g.adj_edge[g.adj_off[v] + k] : -1;
    }
    for (int i = 0; i <= B; ++i) {
        reset_node(ws, i, B);
        ws.owner[i] = 0;
        ws.visited[i] = 0;
        ws.par_edge[i] = -1;
        ws.residual[i] = 0;
    }
    for (int e = 0; e < g.num_edges; ++e) {
        ws.support[e] = 0;
        ws.grown[e] = 0;
    }
    return true;
}

inline bool is_active(const FastWorkspace& ws, int root) {
    return ws.odd[root] && !ws.bnd[root];
}

inline void merge_fast(FastWorkspace& ws, int a, int b) {
    a = ws.parent[a];
    b = ws.parent[b];
    if (a == b) return;
    if (ws.size[a] < ws.size[b]) { int t = a; a = b; b = t; }
    for (int x = ws.head[b]; x >= 0; x = ws.next[x]) ws.parent[x] = a;   // relabel
    ws.work += (uint32_t)ws.size[b];
    ws.sc.relabel += (uint32_t)ws.size[b];
    ws.size[a] += ws.size[b];
    ws.odd[a] ^= ws.odd[b];
    ws.bnd[a] |= ws.bnd[b];
    ws.next[ws.tail[a]] = ws.head[b];
    ws.tail[a] = ws.tail[b];
}

// Undo everything a shot changed in the growth arrays.
inline void cleanup_growth(const Graph& g, FastWorkspace& ws, const int* dl, int nd, int ntouched) {
    const int B = g.num_det;
    for (int t = 0; t < ntouched; ++t) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=8192
#pragma HLS PIPELINE II=1
#pragma HLS DEPENDENCE variable=ws.parent inter false
#pragma HLS DEPENDENCE variable=ws.size inter false
#pragma HLS DEPENDENCE variable=ws.odd inter false
#pragma HLS DEPENDENCE variable=ws.bnd inter false
#pragma HLS DEPENDENCE variable=ws.head inter false
#pragma HLS DEPENDENCE variable=ws.tail inter false
#pragma HLS DEPENDENCE variable=ws.next inter false
        const int e = ws.touched[t];
        ws.support[e] = 0;
        ws.grown[e] = 0;
        reset_node(ws, g.eu[e], B);
        reset_node(ws, g.ev[e], B);
    }
    for (int k = 0; k < nd; ++k) reset_node(ws, dl[k], B);
    ws.work += (uint32_t)ntouched + (uint32_t)nd;
    ws.sc.cleanup += (uint32_t)ntouched + (uint32_t)nd;
}

// Decode one shot. dl holds the nd fired detectors in increasing order.
inline Result decode_fast(const Graph& g, const int* dl, int nd, FastWorkspace& ws) {
    Result res;
    const int B = g.num_det;   // boundary node
    ws.work = 0;
    ws.sc = StageCounts();
    if (nd == 0) return res;   // nothing to correct

    for (int k = 0; k < nd; ++k) {
        ws.odd[dl[k]] = 1;
        ws.act[k] = dl[k];     // every defect starts as an active cluster
    }
    int nact = nd;
    int ntouched = 0;
    int nbl = 0;               // grown edges that touch the boundary
    ws.work += (uint32_t)nd;

    // ---- growth + merging ----
    int iter = 0;
    for (; iter < MAX_GROW_ITERS; ++iter) {
        if (nact == 0) break;

        // Members of the active clusters.
        int nv = 0;
        for (int i = 0; i < nact; ++i)
            for (int x = ws.head[ws.act[i]]; x >= 0; x = ws.next[x]) ws.vl[nv++] = x;
        ws.work += (uint32_t)nact + (uint32_t)nv;
        ws.sc.members += (uint32_t)nv;

        // Frontier: every not-yet-grown edge leaving an active cluster, once. An edge
        // between two active clusters is taken from its eu side only.
        int nf = 0;
        uint32_t step = 0xFFFFFFFFu;
    frontier:
        for (int j = 0; j < nv; ++j) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048
            for (int k = 0; k < MAX_DEG; ++k) {
#pragma HLS PIPELINE II=1
                const int x = ws.vl[j];
                const int e = ws.adjp[x * MAX_DEG + k];
                if (e < 0 || ws.grown[e]) continue;
                const int y = (g.eu[e] == x) ? g.ev[e] : g.eu[e];
                const int ry = ws.parent[y];
                if (ry == ws.parent[x]) continue;   // internal edge, never needed
                const bool y_active = is_active(ws, ry);
                if (y_active && x != g.eu[e]) continue;
                ws.fe[nf] = e;
                ws.fr[nf] = (uint8_t)(1 + y_active);
                ++nf;
                const uint32_t rem = 2 * g.w[e] - ws.support[e];
                const uint32_t s = y_active ? (rem + 1) >> 1 : rem;
                if (s < step) step = s;
            }
        }
        ws.work += (uint32_t)nv * MAX_DEG;
        ws.sc.frontier += (uint32_t)nv * MAX_DEG;
        if (step == 0xFFFFFFFFu) {   // active cluster with nowhere to grow
            res.status = INVALID;
            res.grow_iters = iter;
            cleanup_growth(g, ws, dl, nd, ntouched);
            res.work = ws.work;
            return res;
        }

        int nj = 0;
    apply_step:
        for (int k = 0; k < nf; ++k) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=8192
#pragma HLS PIPELINE II=1
// Each edge is in fe at most once, so iterations never touch the same edge.
#pragma HLS DEPENDENCE variable=ws.support inter false
#pragma HLS DEPENDENCE variable=ws.grown inter false
            const int e = ws.fe[k];
            if (ws.support[e] == 0) ws.touched[ntouched++] = e;
            ws.support[e] += ws.fr[k] * step;
            if (ws.support[e] >= 2 * g.w[e]) {
                ws.grown[e] = 1;
                ws.jg[nj++] = e;
                if (g.eu[e] == B || g.ev[e] == B) ws.bl[nbl++] = e;
            }
        }
        for (int k = 0; k < nj; ++k) merge_fast(ws, g.eu[ws.jg[k]], g.ev[ws.jg[k]]);
        ws.work += (uint32_t)nf + (uint32_t)nj;
        ws.sc.apply += (uint32_t)nf;
        ws.sc.merges += (uint32_t)nj;

        // New active clusters: the merged results of the old ones that are still
        // active, each once, in order of first appearance. owner[rr] ends up as the
        // first position of rr (written backwards), so the second pass has no
        // read-after-write between iterations.
        for (int i = nact - 1; i >= 0; --i) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048
#pragma HLS PIPELINE II=1
            const int rr = ws.parent[ws.act[i]];
            ws.cand[i] = rr;
            ws.owner[rr] = i;
        }
        int k = 0;
        for (int i = 0; i < nact; ++i) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048
#pragma HLS PIPELINE II=1
            const int rr = ws.cand[i];
            if (ws.owner[rr] == i && is_active(ws, rr)) ws.act[k++] = rr;
        }
        ws.work += (uint32_t)nact + (uint32_t)k;
        ws.sc.compact += (uint32_t)nact;
        nact = k;
    }
    res.grow_iters = iter;
    if (iter == MAX_GROW_ITERS) {
        res.status = CAP_HIT;
        cleanup_growth(g, ws, dl, nd, ntouched);
        res.work = ws.work;
        return res;
    }

    // ---- peeling: same forest and order as uf::decode (boundary first, then each
    // defect in increasing order). The boundary's grown edges are in bl; sorted, they
    // are visited in the same order as a scan of the boundary's adjacency.
    for (int i = 1; i < nbl; ++i) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=64
        const int e = ws.bl[i];
        int j = i - 1;
        while (j >= 0 && ws.bl[j] > e) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=64
            ws.bl[j + 1] = ws.bl[j];
            --j;
        }
        ws.bl[j + 1] = e;
    }
    ws.work += (uint32_t)nbl;
    ws.sc.peel_scan += (uint32_t)nbl;
    for (int k = 0; k < nd; ++k) ws.residual[dl[k]] = 1;
    int len = 0;
    for (int k = -1; k < nd; ++k) {
        const int start = (k < 0) ? B : dl[k];
        if (ws.visited[start]) continue;
        ws.visited[start] = 1;
        int head = len;
        ws.order[len++] = start;
        while (head < len) {
            const int v = ws.order[head++];
            if (v == B) {
                for (int t = 0; t < nbl; ++t) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=64
#pragma HLS PIPELINE II=1
                    const int e = ws.bl[t];
                    const int u = (g.eu[e] == B) ? g.ev[e] : g.eu[e];
                    if (ws.visited[u]) continue;
                    ws.visited[u] = 1;
                    ws.par_edge[u] = e;
                    ws.order[len++] = u;
                }
                continue;
            }
            ws.work += (uint32_t)(g.adj_off[v + 1] - g.adj_off[v]);
            ws.sc.peel_scan += (uint32_t)(g.adj_off[v + 1] - g.adj_off[v]);
            for (int a = g.adj_off[v]; a < g.adj_off[v + 1]; ++a) {
                const int e = g.adj_edge[a];
                if (!ws.grown[e]) continue;
                const int u = (g.eu[e] == v) ? g.ev[e] : g.eu[e];
                if (ws.visited[u]) continue;
                ws.visited[u] = 1;
                ws.par_edge[u] = e;
                ws.order[len++] = u;
            }
        }
    }

    for (int k = len - 1; k >= 0; --k) {
        const int v = ws.order[k];
        const int e = ws.par_edge[v];
        if (e < 0 || !ws.residual[v]) continue;   // tree root, or nothing to pass up
        const int p = (g.eu[e] == v) ? g.ev[e] : g.eu[e];
        ws.residual[v] = 0;
        if (p != B) ws.residual[p] ^= 1;
        res.obs_flip ^= g.obs[e];
        res.corr_edges++;
    }

    // The correction is valid only if its syndrome equals the input syndrome. Every
    // non-root node passes up whatever its correction edges leave over, so its
    // syndrome always matches; a tree root other than the boundary matches only if
    // nothing is left at it. Nodes outside the forest are untouched and not defects.
    for (int k = 0; k < len; ++k) {
        const int v = ws.order[k];
        if (v != B && ws.par_edge[v] < 0 && ws.residual[v]) res.status = INVALID;
        ws.visited[v] = 0;
        ws.par_edge[v] = -1;
        ws.residual[v] = 0;
    }
    ws.work += 2 * (uint32_t)len;
    ws.sc.peel_walk += (uint32_t)len;
    cleanup_growth(g, ws, dl, nd, ntouched);
    res.work = ws.work;
    return res;
}

}  // namespace uf

#endif  // UF_FAST_H
