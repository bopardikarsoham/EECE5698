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
//   - every detector's edges sit in MAX_DEG fixed slots that also hold the static
//     edge data (other end, weight), so the frontier scan handles all MAX_DEG edges
//     of a detector in one step (one detector per clock cycle in HLS), and the
//     peeling search reads which of a detector's edges are grown in one step;
//   - the state those parallel lanes read at random addresses (edge support, node
//     root, root active flag) is kept in NREP identical copies, two lanes per copy,
//     since a memory bank only has two ports. Every write goes to all copies;
//   - the frontier saves each new frontier edge's support, so the growth step only
//     writes and handles two edges per step (the two ports of every copy);
//   - after a shot only the nodes that joined a cluster (defects and the ends of
//     fully grown edges) are reset, not the ends of every touched edge.
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
constexpr int NREP = (MAX_DEG + 1) / 2;   // copies of the lane-read state, 2 lanes each
constexpr int FB = 2 * NREP;                       // frontier list banks (even, >= MAX_DEG)
constexpr int FROWS = (MAX_EDGES + FB - 1) / FB;   // rows per bank
static_assert(FB >= MAX_DEG && FB % 2 == 0, "frontier banks: one per lane, even");
static_assert(MAX_DEG <= 16, "grown-slot masks are 16 bits");

// Loop iterations of each decoder stage in one decode_fast call. The kernel returns
// them with every shot, so the hardware reports where its time went (multiply by a
// stage's II to get cycles). Each is added once per loop, outside the loop body.
struct StageCounts {
    uint32_t members;     // member-list entries built (active clusters' nodes)
    uint32_t frontier;    // frontier loop steps (one active-cluster member each)
    uint32_t apply;       // frontier edges grown
    uint32_t merges;      // fully grown edges merged
    uint32_t relabel;     // nodes relabelled by merges
    uint32_t compact;     // active-list entries compacted
    uint32_t peel_scan;   // peeling search steps (forest nodes, grown edges, starts)
    uint32_t peel_walk;   // forest nodes walked (and checked)
    uint32_t cleanup;     // reset steps after the shot (edge pairs, grown edges, defects)
};

// One edge slot of a detector x: the edge, its other end y, twice its weight (the
// support at which it is fully grown), and whether x is the edge's eu end.
struct Slot {
    int e;                // -1 = unused slot
    int y;
    uint16_t w2;
    uint8_t x_is_eu;
};

struct FastWorkspace {
    Slot slot[MAX_DET][MAX_DEG];
    // Lane-read state, NREP identical copies (copy 0 is read everywhere else).
    // support[e] >= 2*w[e] means edge e is fully grown.
    uint16_t support[NREP][MAX_EDGES];
    int parent[NREP][MAX_NODES];
    uint8_t active[NREP][MAX_NODES];   // at a root: odd && !bnd
    int size[MAX_NODES];
    uint8_t odd[MAX_NODES];
    uint8_t bnd[MAX_NODES];
    int head[MAX_NODES];          // member list of a cluster: head/tail valid at roots
    int tail[MAX_NODES];
    int next[MAX_NODES];          // -1 ends the list
    int act[MAX_DET];             // roots of the active clusters
    int vl[MAX_DET];              // members of the active clusters (frontier scan)
    int vr[MAX_DET];              // and their roots
    int cand[MAX_DET];            // merged root of each old active cluster
    int owner[MAX_NODES];         // first position of a root in cand
    // Frontier edges of this iteration, their rates (1 or 2) and their support when
    // found. Entry i is in bank i % FB, row i / FB: the frontier writes one entry per
    // bank per step, each bank its own memory.
    int fe[FB][FROWS];
    uint8_t fr[FB][FROWS];
    uint16_t fs[FB][FROWS];
    int jg[MAX_EDGES];            // fully grown edges, in the order they grew
    int touched[MAX_EDGES];       // edges with nonzero support (reset after the shot)
    int bl[MAX_EDGES];            // grown edges that touch the boundary
    // peeling
    uint8_t visited[MAX_NODES];
    int par_edge[MAX_NODES];
    int order[MAX_NODES];
    uint16_t ordm[MAX_NODES];     // grown-slot mask of order[i], set when it is queued
    uint8_t residual[MAX_NODES];
    uint32_t work;
    StageCounts sc;
};

inline void set_support(FastWorkspace& ws, int e, uint16_t v) {
    for (int c = 0; c < NREP; ++c) {
#pragma HLS UNROLL
        ws.support[c][e] = v;
    }
}

inline void set_parent(FastWorkspace& ws, int i, int v) {
    for (int c = 0; c < NREP; ++c) {
#pragma HLS UNROLL
        ws.parent[c][i] = v;
    }
}

inline void set_active(FastWorkspace& ws, int i, uint8_t v) {
    for (int c = 0; c < NREP; ++c) {
#pragma HLS UNROLL
        ws.active[c][i] = v;
    }
}

// Bit k set: slot k of detector v is a fully grown edge.
inline uint16_t grown_mask(const FastWorkspace& ws, int v) {
#pragma HLS INLINE
    uint16_t m = 0;
    for (int k = 0; k < MAX_DEG; ++k) {
#pragma HLS UNROLL
        const Slot sl = ws.slot[v][k];
        if (sl.e >= 0 && ws.support[k / 2][sl.e] >= sl.w2) m |= (uint16_t)(1u << k);
    }
    return m;
}

inline void reset_node(FastWorkspace& ws, int i, int B) {
    set_parent(ws, i, i);
    set_active(ws, i, 0);
    ws.size[i] = 1;
    ws.odd[i] = 0;
    ws.bnd[i] = (i == B);
    ws.head[i] = i;
    ws.tail[i] = i;
    ws.next[i] = -1;
}

// Call once after the graph is loaded (after build_adjacency); decode_fast leaves the
// workspace clean. Returns false if a detector has more than MAX_DEG edges or an edge
// weight does not fit the support counters.
inline bool init_fast(const Graph& g, FastWorkspace& ws) {
    const int B = g.num_det;
    for (int e = 0; e < g.num_edges; ++e)
        if (g.w[e] > 0x7FFE) return false;
    for (int v = 0; v < B; ++v) {
        const int deg = g.adj_off[v + 1] - g.adj_off[v];
        if (deg > MAX_DEG) return false;
        for (int k = 0; k < MAX_DEG; ++k) {
            Slot& s = ws.slot[v][k];
            if (k < deg) {
                const int e = g.adj_edge[g.adj_off[v] + k];
                s.e = e;
                s.y = (g.eu[e] == v) ? g.ev[e] : g.eu[e];
                s.w2 = (uint16_t)(2 * g.w[e]);
                s.x_is_eu = (g.eu[e] == v);
            } else {
                s.e = -1;
                s.y = 0;
                s.w2 = 0;
                s.x_is_eu = 0;
            }
        }
    }
    for (int i = 0; i <= B; ++i) {
        reset_node(ws, i, B);
        ws.owner[i] = 0;
        ws.visited[i] = 0;
        ws.par_edge[i] = -1;
        ws.residual[i] = 0;
    }
    for (int e = 0; e < g.num_edges; ++e) set_support(ws, e, 0);
    return true;
}

inline bool is_active(const FastWorkspace& ws, int root) {
    return ws.odd[root] && !ws.bnd[root];
}

inline void merge_fast(FastWorkspace& ws, int a, int b) {
    a = ws.parent[0][a];
    b = ws.parent[0][b];
    if (a == b) return;
    if (ws.size[a] < ws.size[b]) { int t = a; a = b; b = t; }
    for (int x = ws.head[b]; x >= 0; x = ws.next[x]) set_parent(ws, x, a);   // relabel
    ws.work += (uint32_t)ws.size[b];
    ws.sc.relabel += (uint32_t)ws.size[b];
    ws.size[a] += ws.size[b];
    ws.odd[a] ^= ws.odd[b];
    ws.bnd[a] |= ws.bnd[b];
    set_active(ws, a, ws.odd[a] && !ws.bnd[a]);
    ws.next[ws.tail[a]] = ws.head[b];
    ws.tail[a] = ws.tail[b];
}

// Undo everything a shot changed in the growth arrays: the support of every touched
// edge, and the nodes that joined a cluster. Node state only changes in merges (both
// ends of a fully grown edge) and for defects; the far end of an edge that never
// fully grew was only read.
inline void cleanup_growth(const Graph& g, FastWorkspace& ws, const int* dl, int nd,
                           int ntouched, int ngrown) {
    const int B = g.num_det;
    for (int t = 0; t < ntouched; t += 2) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=4096
#pragma HLS PIPELINE II=1
#pragma HLS DEPENDENCE variable=ws.support inter false
        set_support(ws, ws.touched[t], 0);
        if (t + 1 < ntouched) set_support(ws, ws.touched[t + 1], 0);
    }
    for (int t = 0; t < ngrown; ++t) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=8192
#pragma HLS PIPELINE II=1
#pragma HLS DEPENDENCE variable=ws.parent inter false
#pragma HLS DEPENDENCE variable=ws.active inter false
#pragma HLS DEPENDENCE variable=ws.size inter false
#pragma HLS DEPENDENCE variable=ws.odd inter false
#pragma HLS DEPENDENCE variable=ws.bnd inter false
#pragma HLS DEPENDENCE variable=ws.head inter false
#pragma HLS DEPENDENCE variable=ws.tail inter false
#pragma HLS DEPENDENCE variable=ws.next inter false
        const int e = ws.jg[t];
        reset_node(ws, g.eu[e], B);
        reset_node(ws, g.ev[e], B);
    }
    for (int k = 0; k < nd; ++k) reset_node(ws, dl[k], B);
    const uint32_t steps = (uint32_t)(ntouched + 1) / 2 + (uint32_t)ngrown + (uint32_t)nd;
    ws.work += steps;
    ws.sc.cleanup += steps;
}

// Decode one shot. dl holds the nd fired detectors in increasing order.
inline Result decode_fast(const Graph& g, const int* dl, int nd, FastWorkspace& ws) {
// Lane-parallel access: every copy of the lane-read state is its own memory with two
// ports, every detector's slots are read at once, and the frontier list takes up to
// MAX_DEG new entries per step (consecutive entries are in different banks).
#pragma HLS ARRAY_PARTITION variable=ws.slot dim=2 complete
#pragma HLS ARRAY_PARTITION variable=ws.support dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ws.parent dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ws.active dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ws.fe dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ws.fr dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ws.fs dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ws.touched cyclic factor=2
#pragma HLS ARRAY_PARTITION variable=ws.jg cyclic factor=2
#pragma HLS ARRAY_PARTITION variable=ws.bl cyclic factor=2
#pragma HLS BIND_STORAGE variable=ws.support type=ram_t2p
#pragma HLS BIND_STORAGE variable=ws.parent type=ram_t2p
#pragma HLS BIND_STORAGE variable=ws.active type=ram_t2p
    Result res;
    const int B = g.num_det;   // boundary node
    ws.work = 0;
    ws.sc = StageCounts();
    if (nd == 0) return res;   // nothing to correct

    for (int k = 0; k < nd; ++k) {
        ws.odd[dl[k]] = 1;
        set_active(ws, dl[k], 1);
        ws.act[k] = dl[k];     // every defect starts as an active cluster
    }
    int nact = nd;
    int ntouched = 0;
    int ngrown = 0;            // fully grown edges (in jg)
    int nbl = 0;               // grown edges that touch the boundary
    ws.work += (uint32_t)nd;

    // ---- growth + merging ----
    int iter = 0;
    for (; iter < MAX_GROW_ITERS; ++iter) {
        if (nact == 0) break;

        // Members of the active clusters, with their roots: one loop over all the
        // member lists, each step either takes a member or moves to the next cluster.
        int nv = 0;
        int ci = 0;
        int r = ws.act[0];
        int x = ws.head[r];
    members:
        while (ci < nact) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=4096
#pragma HLS PIPELINE II=1
            if (x >= 0) {
                ws.vl[nv] = x;
                ws.vr[nv] = r;
                ++nv;
                x = ws.next[x];
            } else if (++ci < nact) {
                r = ws.act[ci];
                x = ws.head[r];
            }
        }
        ws.work += (uint32_t)nact + (uint32_t)nv;
        ws.sc.members += (uint32_t)nv;

        // Frontier: every not-yet-grown edge leaving an active cluster, once. An edge
        // between two active clusters is taken from its eu side only. One member x per
        // step; its MAX_DEG slots are checked in parallel lanes and the new frontier
        // edges are appended in slot order.
        int nf = 0;
        int nf_row = 0;            // nf = nf_row * FB + nf_col
        int nf_col = 0;
        uint32_t step = 0xFFFFFFFFu;
    frontier:
        for (int j = 0; j < nv; ++j) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048
#pragma HLS PIPELINE II=1
#pragma HLS DEPENDENCE variable=ws.fe inter false
#pragma HLS DEPENDENCE variable=ws.fr inter false
#pragma HLS DEPENDENCE variable=ws.fs inter false
            const int x = ws.vl[j];
            const int rx = ws.vr[j];
            bool take[MAX_DEG];
            uint8_t rate[MAX_DEG];
            uint16_t found[MAX_DEG];
            uint32_t s[MAX_DEG];
            for (int k = 0; k < MAX_DEG; ++k) {
#pragma HLS UNROLL
                const Slot sl = ws.slot[x][k];
                const int c = k / 2;
                take[k] = false;
                rate[k] = 1;
                found[k] = 0;
                s[k] = 0xFFFFFFFFu;
                if (sl.e < 0) continue;
                const uint32_t sup = ws.support[c][sl.e];
                if (sup >= sl.w2) continue;                   // already grown
                const int ry = ws.parent[c][sl.y];
                if (ry == rx) continue;                       // internal edge, never needed
                const bool y_active = ws.active[c][ry];
                if (y_active && !sl.x_is_eu) continue;
                take[k] = true;
                rate[k] = (uint8_t)(1 + y_active);
                found[k] = (uint16_t)sup;
                const uint32_t rem = sl.w2 - sup;
                s[k] = y_active ? (rem + 1) >> 1 : rem;
            }
            // Lane k's entry goes to list position nf + pos[k]. Bank b takes the entry
            // at position nf + jb (jb = (b - nf_col) mod FB): one write per bank, at
            // a bank fixed in hardware, the lane picked by a small mux.
            int pos[MAX_DEG];
            int cnt = 0;
            for (int k = 0; k < MAX_DEG; ++k) {
#pragma HLS UNROLL
                pos[k] = cnt;
                cnt += take[k] ? 1 : 0;
                if (s[k] < step) step = s[k];
            }
            for (int b = 0; b < FB; ++b) {
#pragma HLS UNROLL
                const int jb = (b >= nf_col) ? b - nf_col : b - nf_col + FB;
                const int row = nf_row + ((b < nf_col) ? 1 : 0);
                bool hit = false;
                int e = 0;
                uint8_t rt = 0;
                uint16_t fd = 0;
                for (int k = 0; k < MAX_DEG; ++k) {
#pragma HLS UNROLL
                    if (take[k] && pos[k] == jb) {
                        hit = true;
                        e = ws.slot[x][k].e;
                        rt = rate[k];
                        fd = found[k];
                    }
                }
                if (hit) {
                    ws.fe[b][row] = e;
                    ws.fr[b][row] = rt;
                    ws.fs[b][row] = fd;
                }
            }
            nf += cnt;
            nf_col += cnt;
            if (nf_col >= FB) {
                nf_col -= FB;
                ++nf_row;
            }
        }
        ws.work += (uint32_t)nv;
        ws.sc.frontier += (uint32_t)nv;
        if (step == 0xFFFFFFFFu) {   // active cluster with nowhere to grow
            res.status = INVALID;
            res.grow_iters = iter;
            cleanup_growth(g, ws, dl, nd, ntouched, ngrown);
            res.work = ws.work;
            return res;
        }

        // Grow the frontier edges, two per step. The support each edge had when the
        // frontier found it is still current (nothing writes support in between, and
        // every edge is in fe at most once), so this loop only writes support.
        // touched, jg and bl are only appended to (each position written once), so
        // there is no dependence between steps through them.
        const int jg0 = ngrown;
        int a_row = 0;             // k = a_row * FB + a_col, a_col even (FB is even)
        int a_col = 0;
    apply_step:
        for (int k = 0; k < nf; k += 2) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=4096
#pragma HLS PIPELINE II=1
#pragma HLS DEPENDENCE variable=ws.support inter false
#pragma HLS DEPENDENCE variable=ws.touched inter false
#pragma HLS DEPENDENCE variable=ws.jg inter false
#pragma HLS DEPENDENCE variable=ws.bl inter false
            for (int u = 0; u < 2; ++u) {
#pragma HLS UNROLL
                if (k + u >= nf) continue;
                const int e = ws.fe[a_col + u][a_row];
                const uint32_t sup = ws.fs[a_col + u][a_row];
                if (sup == 0) ws.touched[ntouched++] = e;
                const uint32_t nsup = sup + ws.fr[a_col + u][a_row] * step;
                set_support(ws, e, (uint16_t)nsup);
                if (nsup >= 2 * g.w[e]) {
                    ws.jg[ngrown++] = e;
                    if (g.eu[e] == B || g.ev[e] == B) ws.bl[nbl++] = e;
                }
            }
            a_col += 2;
            if (a_col == FB) {
                a_col = 0;
                ++a_row;
            }
        }
        const int nj = ngrown - jg0;
        for (int k = jg0; k < ngrown; ++k) merge_fast(ws, g.eu[ws.jg[k]], g.ev[ws.jg[k]]);
        ws.work += (uint32_t)(nf + 1) / 2 + (uint32_t)nj;
        ws.sc.apply += (uint32_t)nf;
        ws.sc.merges += (uint32_t)nj;

        // New active clusters: the merged results of the old ones that are still
        // active, each once, in order of first appearance. owner[rr] ends up as the
        // first position of rr (written backwards), so the second pass has no
        // read-after-write between iterations.
        for (int i = nact - 1; i >= 0; --i) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048
#pragma HLS PIPELINE II=1
            const int rr = ws.parent[0][ws.act[i]];
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
        cleanup_growth(g, ws, dl, nd, ntouched, ngrown);
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
    for (int k = 0; k < nd; ++k) ws.residual[dl[k]] = 1;
    int len = 0;
    ws.visited[B] = 1;
    ws.order[len++] = B;
    for (int t = 0; t < nbl; ++t) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=64
#pragma HLS PIPELINE II=1
        const int e = ws.bl[t];
        const int u = (g.eu[e] == B) ? g.ev[e] : g.eu[e];
        if (ws.visited[u]) continue;
        ws.visited[u] = 1;
        ws.par_edge[u] = e;
        ws.ordm[len] = grown_mask(ws, u);
        ws.order[len++] = u;
    }

    // Breadth-first search over the grown edges as one loop: each step either takes
    // the next grown edge of the current node (a bit mask of its grown slots), or
    // moves to the next queued node, or starts a new tree at the next defect not
    // reached yet. A node's mask is computed when it is queued (ordm), so moving to
    // the next node is one read.
    int head = 1;              // order[0] is the boundary, already expanded
    int next_start = 0;
    int cur = 0;               // node whose grown edges are in mask
    uint32_t mask = 0;
    uint32_t steps = 0;
peel_search:
    while (true) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=4096
#pragma HLS PIPELINE
        ++steps;
        if (mask) {
            const int k = __builtin_ctz(mask);
            mask &= mask - 1;
            const Slot sl = ws.slot[cur][k];
            if (!ws.visited[sl.y]) {
                ws.visited[sl.y] = 1;
                ws.par_edge[sl.y] = sl.e;
                ws.ordm[len] = grown_mask(ws, sl.y);
                ws.order[len++] = sl.y;
            }
        } else if (head < len) {
            cur = ws.order[head];
            mask = ws.ordm[head];
            ++head;
        } else if (next_start < nd) {
            const int start = dl[next_start++];
            if (!ws.visited[start]) {
                ws.visited[start] = 1;
                ws.ordm[len] = grown_mask(ws, start);
                ws.order[len++] = start;
            }
        } else {
            break;
        }
    }
    ws.work += (uint32_t)nbl + steps;
    ws.sc.peel_scan += (uint32_t)nbl + steps;

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
    cleanup_growth(g, ws, dl, nd, ntouched, ngrown);
    res.work = ws.work;
    return res;
}

}  // namespace uf

#endif  // UF_FAST_H
