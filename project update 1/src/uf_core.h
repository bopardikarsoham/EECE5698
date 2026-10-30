// uf_core.h - weighted union-find (UF) decoder for graph-like QEC decoding graphs.
//
// Algorithm (Delfosse & Nickerson, "Almost-linear time decoding algorithm for
// topological codes", 2021, with integer edge weights):
//   1. Every flagged detector (defect) starts as its own cluster. A cluster is
//      "active" while it holds an odd number of defects and does not touch the
//      boundary.
//   2. Growth: every active cluster grows along all edges leaving it. An edge of
//      weight w is fully grown when its support reaches 2*w (each side of an edge
//      contributes one unit per step, so a weight-w edge is covered in w steps from
//      both sides or 2*w steps from one). Instead of stepping one unit at a time we
//      jump straight to the next moment an edge becomes fully grown, which gives the
//      same result in far fewer iterations.
//   3. Fully grown edges merge the clusters at their two ends. Repeat until no
//      cluster is active.
//   4. Peeling: build a spanning forest of the grown edges (trees containing the
//      boundary are rooted at the boundary), then walk it from the leaves inwards.
//      A leaf that still holds a defect puts its tree edge into the correction and
//      passes the defect up to its parent.
//   5. The predicted logical flip is the XOR of the observable masks of the edges
//      in the correction.
//
// Everything uses fixed-size arrays and no dynamic memory, so the same code can be
// moved into the HLS kernel later.

#ifndef UF_CORE_H
#define UF_CORE_H

#include <cstdint>

namespace uf {

// Sized for the rotated surface code up to d = 11 with d rounds (1320 detectors,
// 6718 edges). The HLS kernel will use limits sized for one specific graph.
constexpr int MAX_DET   = 4096;          // max detectors (graph nodes, excluding boundary)
constexpr int MAX_NODES = MAX_DET + 1;   // detectors + one virtual boundary node
constexpr int MAX_EDGES = 16384;
// Every growth iteration fully grows at least one edge, so MAX_EDGES iterations is
// always enough. Hitting the cap means something is wrong with the graph.
constexpr int MAX_GROW_ITERS = MAX_EDGES;

struct Graph {
    int num_det = 0;            // detectors are nodes 0..num_det-1, boundary is node num_det
    int num_obs = 0;
    int num_edges = 0;
    int eu[MAX_EDGES];          // edge endpoints (ev == num_det means boundary)
    int ev[MAX_EDGES];
    uint32_t w[MAX_EDGES];      // integer edge weight, >= 1
    uint32_t obs[MAX_EDGES];    // bit k set: this edge flips observable k
    // Adjacency in CSR form (filled by build_adjacency), used by peeling.
    int adj_off[MAX_NODES + 1];
    int adj_edge[2 * MAX_EDGES];
};

// Fill g.adj_off / g.adj_edge from the edge list. Call once after loading.
inline void build_adjacency(Graph& g) {
    const int n = g.num_det + 1;
    for (int i = 0; i <= n; ++i) g.adj_off[i] = 0;
    for (int e = 0; e < g.num_edges; ++e) {
        g.adj_off[g.eu[e] + 1]++;
        g.adj_off[g.ev[e] + 1]++;
    }
    for (int i = 0; i < n; ++i) g.adj_off[i + 1] += g.adj_off[i];
    int fill[MAX_NODES];
    for (int i = 0; i < n; ++i) fill[i] = g.adj_off[i];
    for (int e = 0; e < g.num_edges; ++e) {
        g.adj_edge[fill[g.eu[e]]++] = e;
        g.adj_edge[fill[g.ev[e]]++] = e;
    }
}

enum Status { OK = 0, CAP_HIT = 1, INVALID = 2 };

struct Result {
    uint32_t obs_flip = 0;   // predicted observable flips (bit k = observable k)
    int status = OK;
    int grow_iters = 0;      // growth iterations used for this shot
    int corr_edges = 0;      // number of edges in the correction
};

// Scratch space for one decode. Kept outside the function so it is allocated once.
struct Workspace {
    int parent[MAX_NODES];
    int size[MAX_NODES];
    uint8_t odd[MAX_NODES];       // defect parity of the cluster (valid at roots)
    uint8_t bnd[MAX_NODES];       // cluster touches the boundary (valid at roots)
    uint32_t support[MAX_EDGES];
    uint8_t grown[MAX_EDGES];
    uint8_t rate[MAX_EDGES];      // 0, 1 or 2 active sides this iteration
    uint8_t just_grown[MAX_EDGES];
    // peeling
    uint8_t visited[MAX_NODES];
    int par_edge[MAX_NODES];
    int order[MAX_NODES];
    uint8_t residual[MAX_NODES];
    uint8_t check[MAX_NODES];
};

inline int find_root(int* parent, int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];   // path halving
        x = parent[x];
    }
    return x;
}

inline void merge(Workspace& ws, int a, int b) {
    a = find_root(ws.parent, a);
    b = find_root(ws.parent, b);
    if (a == b) return;
    if (ws.size[a] < ws.size[b]) { int t = a; a = b; b = t; }
    ws.parent[b] = a;
    ws.size[a] += ws.size[b];
    ws.odd[a] ^= ws.odd[b];
    ws.bnd[a] |= ws.bnd[b];
}

inline bool is_active(const Workspace& ws, int root) {
    return ws.odd[root] && !ws.bnd[root];
}

// Decode one shot. defect[i] is 1 if detector i fired.
inline Result decode(const Graph& g, const uint8_t* defect, Workspace& ws) {
    Result res;
    const int B = g.num_det;   // boundary node
    const int n = B + 1;
    const int E = g.num_edges;

    int num_defects = 0;
    for (int i = 0; i < n; ++i) {
        ws.parent[i] = i;
        ws.size[i] = 1;
        ws.odd[i] = (i < B) ? defect[i] : 0;
        ws.bnd[i] = (i == B);
        num_defects += ws.odd[i];
    }
    if (num_defects == 0) return res;   // nothing to correct

    for (int e = 0; e < E; ++e) {
        ws.support[e] = 0;
        ws.grown[e] = 0;
    }

    // ---- growth + merging ----
    int iter = 0;
    for (; iter < MAX_GROW_ITERS; ++iter) {
        bool any_active = false;
        for (int i = 0; i < n; ++i)
            if (ws.parent[i] == i && is_active(ws, i)) any_active = true;
        if (!any_active) break;

        // Rate of every edge, and the smallest jump that fully grows some edge.
        uint32_t step = 0xFFFFFFFFu;
        for (int e = 0; e < E; ++e) {
            ws.rate[e] = 0;
            if (ws.grown[e]) continue;
            const int ru = find_root(ws.parent, g.eu[e]);
            const int rv = find_root(ws.parent, g.ev[e]);
            if (ru == rv) continue;   // internal edge, never needed
            const uint8_t r = (uint8_t)(is_active(ws, ru) + is_active(ws, rv));
            ws.rate[e] = r;
            if (r) {
                const uint32_t rem = 2 * g.w[e] - ws.support[e];
                const uint32_t s = (rem + r - 1) / r;
                if (s < step) step = s;
            }
        }
        if (step == 0xFFFFFFFFu) {   // active cluster with nowhere to grow
            res.status = INVALID;
            res.grow_iters = iter;
            return res;
        }

        for (int e = 0; e < E; ++e) {
            ws.just_grown[e] = 0;
            if (!ws.rate[e]) continue;
            ws.support[e] += ws.rate[e] * step;
            if (ws.support[e] >= 2 * g.w[e]) {
                ws.grown[e] = 1;
                ws.just_grown[e] = 1;
            }
        }
        for (int e = 0; e < E; ++e)
            if (ws.just_grown[e]) merge(ws, g.eu[e], g.ev[e]);
    }
    res.grow_iters = iter;
    if (iter == MAX_GROW_ITERS) {
        res.status = CAP_HIT;
        return res;
    }

    // ---- peeling: spanning forest of grown edges, boundary trees rooted at B ----
    for (int i = 0; i < n; ++i) {
        ws.visited[i] = 0;
        ws.par_edge[i] = -1;
        ws.residual[i] = (i < B) ? defect[i] : 0;
        ws.check[i] = 0;
    }
    int len = 0;
    for (int k = -1; k < B; ++k) {
        const int start = (k < 0) ? B : k;   // boundary first, then each defect
        if (ws.visited[start]) continue;
        if (k >= 0 && !defect[k]) continue;
        ws.visited[start] = 1;
        int head = len;
        ws.order[len++] = start;
        while (head < len) {
            const int v = ws.order[head++];
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
        if (g.eu[e] != B) ws.check[g.eu[e]] ^= 1;
        if (g.ev[e] != B) ws.check[g.ev[e]] ^= 1;
    }

    // The correction is valid only if its syndrome equals the input syndrome.
    for (int i = 0; i < B; ++i)
        if (ws.check[i] != defect[i]) res.status = INVALID;
    return res;
}

}  // namespace uf

#endif  // UF_CORE_H
