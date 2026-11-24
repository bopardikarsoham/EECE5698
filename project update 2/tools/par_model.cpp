// par_model.cpp - cycle model of a fully parallel union-find decoder (one processing
// element per detector and per edge, as in Helios [Liyanage et al.]) on real shots.
//
// It replays the reference decoder's growth (same weighted growth steps and merges as
// uf::decode in src/uf_core.h, so the clusters are identical) and charges each step
// what a parallel design would spend on it:
//
//   per growth iteration:  1           grow every boundary edge of every active cluster
//                          + L_red     global min of the step (reduction tree)
//                          + 2 * ecc   relabel merged clusters by min-ID flooding along
//                                      grown edges, then their parity/boundary flags back
//                                      to the new root; ecc = largest hop distance from
//                                      the new label's node in any cluster merged this
//                                      iteration (0 if nothing merged)
//                                      (--local-parity: 3 * ecc, adding the "active" flag
//                                      sent back down from the root to every member)
//                          + L_red     "any cluster still active?" (OR tree)
//   per shot:              load + L_red + peel + 2
//                          load = ceil(N / 512) + 1 (syndrome in 512-bit words)
//                          peel = 2 * depth + 2: level-synchronous BFS over the grown
//                                 forest, then parities passed up one level per cycle
//
// L_red = ceil(log2(N) / 2): reduction tree with two levels per clock.
// N = detectors + boundary. This is a model, not a design: it ignores routing limits and
// assumes the clock (default 250 MHz) holds with thousands of processing elements.
//
// Build: g++ -std=c++17 -O2 -DUF_MAX_DET=5120 -DUF_MAX_EDGES=28672 tools/par_model.cpp -o build/sw/par_model
// Run:   build/sw/par_model [--mhz 250] [--csv out.csv] [--write-par-expected] <case_dir> ...
//        --write-par-expected writes <case_dir>/par_expected.bin: the predictions with the
//        parallel-friendly peeling forest (golden output for src/par/).
//        --split-boundary K: the forest of a decoder whose boundary is split into virtual
//        nodes of K edges each (gen_par_graph.py --split-boundary K); 0 = one boundary.
//        --quantize K: divide every weight by K (rounded, at least 1) before decoding, to
//        study coarser weights; also prints growth units per shot (the sum of the growth
//        steps, i.e. the growth cycles of a design that grows by a fixed amount per cycle).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "../src/uf_core.h"
#include "../src/host/case_io.h"

namespace {

int g_ecc_passes = 2;
int g_quant = 1;        // --quantize K: weights become max(1, round(w / K))
int g_split = 0;        // --split-boundary K: K boundary edges per virtual boundary node   // --local-parity: 3 (flood, parity up the tree, "active" back down)

struct Cost {
    uint32_t alt_obs = 0;   // prediction with the parallel-friendly peeling forest
    uint64_t cycles = 0;
    int iters = 0;
    int ecc_sum = 0;
    int depth = 0;
    uint64_t units = 0;     // growth units (sum of steps): cycles of a fixed-increment design
};

int find(std::vector<int>& p, int x) {
    while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; }
    return x;
}

// Largest BFS hop distance from src over grown edges (within its cluster).
int ecc_from(const uf::Graph& g, const std::vector<uint8_t>& grown, int src,
             std::vector<int>& dist, std::vector<int>& q) {
    q.clear();
    q.push_back(src);
    dist[src] = 0;
    int best = 0;
    for (size_t h = 0; h < q.size(); ++h) {
        const int v = q[h];
        for (int a = g.adj_off[v]; a < g.adj_off[v + 1]; ++a) {
            const int e = g.adj_edge[a];
            if (!grown[e]) continue;
            const int u = (g.eu[e] == v) ? g.ev[e] : g.eu[e];
            if (dist[u] >= 0) continue;
            dist[u] = dist[v] + 1;
            best = std::max(best, dist[u]);
            q.push_back(u);
        }
    }
    for (int v : q) dist[v] = -1;
    return best;
}

Cost model_shot(const uf::Graph& g, const uint8_t* defect, int L_red) {
    const int B = g.num_det, n = B + 1, E = g.num_edges;
    Cost c;
    const int load = (n + 511) / 512 + 1;
    std::vector<int> parent(n), size(n, 1);
    std::vector<uint8_t> odd(n), bnd(n), grown(E, 0), rate(E), just(E);
    std::vector<uint32_t> support(E, 0);
    int nd = 0;
    for (int i = 0; i < n; ++i) {
        parent[i] = i;
        odd[i] = (i < B) ? defect[i] : 0;
        bnd[i] = (i == B);
        nd += odd[i];
    }
    if (nd == 0) { c.cycles = load + L_red + 2; return c; }
    auto active = [&](int r) { return odd[r] && !bnd[r]; };
    std::vector<int> dist(n, -1), q;
    q.reserve(n);
    uint64_t cyc = load;
    for (;;) {
        bool any = false;
        for (int i = 0; i < n; ++i) if (parent[i] == i && active(i)) { any = true; break; }
        cyc += L_red;                       // OR tree: anything still active?
        if (!any) break;
        uint32_t step = 0xFFFFFFFFu;
        for (int e = 0; e < E; ++e) {
            rate[e] = 0;
            if (grown[e]) continue;
            const int ru = find(parent, g.eu[e]), rv = find(parent, g.ev[e]);
            if (ru == rv) continue;
            const uint8_t r = (uint8_t)(active(ru) + active(rv));
            rate[e] = r;
            if (r) {
                const uint32_t rem = 2 * g.w[e] - support[e];
                step = std::min(step, (rem + r - 1) / r);
            }
        }
        if (step == 0xFFFFFFFFu) break;     // invalid; does not happen on these data
        c.units += step;
        std::vector<int> touched;
        for (int e = 0; e < E; ++e) {
            just[e] = 0;
            if (!rate[e]) continue;
            support[e] += rate[e] * step;
            if (support[e] >= 2 * g.w[e]) { grown[e] = 1; just[e] = 1; }
        }
        for (int e = 0; e < E; ++e) {
            if (!just[e]) continue;
            int a = find(parent, g.eu[e]), b = find(parent, g.ev[e]);
            touched.push_back(g.eu[e]);
            if (a == b) continue;
            if (size[a] < size[b]) std::swap(a, b);
            parent[b] = a; size[a] += size[b]; odd[a] ^= odd[b]; bnd[a] |= bnd[b];
        }
        // Flooding cost: from the smallest node ID of each merged cluster.
        int ecc = 0;
        std::vector<int> seen_roots;
        for (int v : touched) {
            const int r = find(parent, v);
            if (std::find(seen_roots.begin(), seen_roots.end(), r) != seen_roots.end()) continue;
            seen_roots.push_back(r);
            int mn = n;
            for (int i = 0; i < n; ++i) if (find(parent, i) == r) { mn = i; break; }
            ecc = std::max(ecc, ecc_from(g, grown, mn, dist, q));
        }
        cyc += 1 + L_red + g_ecc_passes * ecc;
        c.ecc_sum += ecc;
        ++c.iters;
    }
    // Forest depth: BFS levels from the boundary and from each defect, as uf::decode.
    std::vector<int> level(n, -1);
    int depth = 0;
    for (int k = -1; k < B; ++k) {
        const int s = (k < 0) ? B : k;
        if (level[s] >= 0 || (k >= 0 && !defect[k])) continue;
        q.clear(); q.push_back(s); level[s] = 0;
        for (size_t h = 0; h < q.size(); ++h) {
            const int v = q[h];
            for (int a = g.adj_off[v]; a < g.adj_off[v + 1]; ++a) {
                const int e = g.adj_edge[a];
                if (!grown[e]) continue;
                const int u = (g.eu[e] == v) ? g.ev[e] : g.eu[e];
                if (level[u] >= 0) continue;
                level[u] = level[v] + 1;
                depth = std::max(depth, level[u]);
                q.push_back(u);
            }
        }
    }
    c.depth = depth;
    cyc += 2 * depth + 2 + 2;
    // Parallel-friendly forest: roots are the boundary, else the cluster's smallest
    // node ID, all trees grown level by level at once; a node's parent is
    // its smallest-ID neighbour on the level above (smallest edge ID on ties).
    if (!g_split) {
        std::vector<int> lv(n, -1), pe(n, -1), cur, nxt;
        lv[B] = 0; cur.push_back(B);
        // root of a cluster without the boundary = its smallest node ID (its label in
        // the parallel design); only clusters with a defect have grown edges
        std::vector<int> cl_min(n, n);
        std::vector<uint8_t> has_def(n, 0);
        for (int i = 0; i < B; ++i) if (defect[i]) has_def[find(parent, i)] = 1;
        for (int i = 0; i < B; ++i) { int r = find(parent, i); if (has_def[r]) cl_min[r] = std::min(cl_min[r], i); }
        const int rB = find(parent, B);
        for (int r = 0; r < n; ++r)
            if (cl_min[r] < n && r != rB) { lv[cl_min[r]] = 0; cur.push_back(cl_min[r]); }
        std::vector<int> levels_order;
        for (int L = 0; !cur.empty(); ++L) {
            for (int v : cur) levels_order.push_back(v);
            nxt.clear();
            for (int v : cur)
                for (int a = g.adj_off[v]; a < g.adj_off[v + 1]; ++a) {
                    const int e = g.adj_edge[a];
                    if (!grown[e]) continue;
                    const int u = (g.eu[e] == v) ? g.ev[e] : g.eu[e];
                    if (lv[u] >= 0 && lv[u] <= L) continue;
                    if (lv[u] < 0) { lv[u] = L + 1; nxt.push_back(u); pe[u] = e; continue; }
                    // already on level L+1: keep the smallest parent ID, then edge ID
                    const int pu = (g.eu[pe[u]] == u) ? g.ev[pe[u]] : g.eu[pe[u]];
                    if (v < pu || (v == pu && e < pe[u])) pe[u] = e;
                }
            cur.swap(nxt);
        }
        std::vector<uint8_t> res(n, 0);
        for (int i = 0; i < B; ++i) res[i] = defect[i];
        uint32_t obs = 0;
        for (int k = (int)levels_order.size() - 1; k >= 0; --k) {
            const int v = levels_order[k];
            const int e = pe[v];
            if (e < 0 || !res[v]) continue;
            const int p = (g.eu[e] == v) ? g.ev[e] : g.eu[e];
            res[v] = 0;
            if (p != B) res[p] ^= 1;
            obs ^= g.obs[e];
        }
        c.alt_obs = obs;
    } else {
        // Split boundary (as src/par with gen_par_graph.py --split-boundary K): the
        // boundary becomes G virtual nodes with IDs 0..G-1, K boundary edges each in edge
        // order; detector i becomes node G + i. Growth is unchanged (a cluster touching
        // the boundary is inactive however it touches it). Every component of grown edges
        // is rooted at its smallest ID, so a boundary cluster is rooted at a virtual
        // node; a virtual node absorbs any residual that reaches it.
        int nb = 0;
        std::vector<int> vu(E), vv(E);
        std::vector<int> bgrp(E, -1);
        for (int e = 0; e < E; ++e)
            if ((int)g.eu[e] == B || (int)g.ev[e] == B) bgrp[e] = nb++ / g_split;
        const int G = (nb + g_split - 1) / g_split;
        const int NV = G + B;
        auto vid = [&](int x, int e) { return x == B ? bgrp[e] : G + x; };
        for (int e = 0; e < E; ++e) { vu[e] = vid(g.eu[e], e); vv[e] = vid(g.ev[e], e); }
        std::vector<std::vector<std::pair<int, int>>> adj(NV);
        for (int e = 0; e < E; ++e) {
            if (!grown[e]) continue;
            adj[vu[e]].push_back({vv[e], e});
            adj[vv[e]].push_back({vu[e], e});
        }
        // roots: smallest ID of every component of grown edges
        std::vector<int> comp(NV, -1), lv(NV, -1), pe(NV, -1), cur, nxt, stack;
        for (int s0 = 0; s0 < NV; ++s0) {
            if (comp[s0] >= 0) continue;
            comp[s0] = s0; lv[s0] = 0; cur.push_back(s0);
            stack.assign(1, s0);
            while (!stack.empty()) {
                const int v = stack.back(); stack.pop_back();
                for (auto [u, e] : adj[v]) if (comp[u] < 0) { comp[u] = s0; stack.push_back(u); }
            }
        }
        std::vector<int> levels_order;
        for (int L = 0; !cur.empty(); ++L) {
            for (int v : cur) levels_order.push_back(v);
            nxt.clear();
            for (int v : cur)
                for (auto [u, e] : adj[v]) {
                    if (lv[u] >= 0 && lv[u] <= L) continue;
                    if (lv[u] < 0) { lv[u] = L + 1; nxt.push_back(u); pe[u] = e; continue; }
                    const int pu = (vu[pe[u]] == u) ? vv[pe[u]] : vu[pe[u]];
                    if (v < pu || (v == pu && e < pe[u])) pe[u] = e;
                }
            cur.swap(nxt);
        }
        std::vector<uint8_t> res(NV, 0);
        for (int i = 0; i < B; ++i) res[G + i] = defect[i];
        uint32_t obs = 0;
        for (int k = (int)levels_order.size() - 1; k >= 0; --k) {
            const int v = levels_order[k];
            const int e = pe[v];
            if (e < 0 || !res[v]) continue;
            const int p = (vu[e] == v) ? vv[e] : vu[e];
            res[v] = 0;
            if (p >= G) res[p] ^= 1;
            obs ^= g.obs[e];
        }
        c.alt_obs = obs;
    }
    c.cycles = cyc;
    return c;
}

double pct(std::vector<uint64_t> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t i = (size_t)std::ceil(p * v.size()) - 1;
    return (double)v[std::min(i, v.size() - 1)];
}

}  // namespace

int main(int argc, char** argv) {
    double mhz = 250;
    std::string csv;
    bool write_alt = false;
    std::vector<std::string> dirs;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--mhz" && i + 1 < argc) mhz = atof(argv[++i]);
        else if (a == "--csv" && i + 1 < argc) csv = argv[++i];
        else if (a == "--write-par-expected") write_alt = true;
        else if (a == "--local-parity") g_ecc_passes = 3;
        else if (a == "--split-boundary" && i + 1 < argc) g_split = atoi(argv[++i]);
        else if (a == "--quantize" && i + 1 < argc) g_quant = atoi(argv[++i]);
        else dirs.push_back(a);
    }
    if (dirs.empty()) { fprintf(stderr, "usage: par_model [--mhz M] [--csv f] <case_dir>...\n"); return 1; }
    FILE* out = csv.empty() ? nullptr : fopen(csv.c_str(), "w");
    if (out) fprintf(out, "case,d,num_det,num_edges,shots,mean_cycles,p99_cycles,p999_cycles,max_cycles,mean_iters,mean_ecc_sum,mean_depth,mean_us,us_per_round_mean,us_per_round_p999,golden_ok\n");
    printf("%-36s %6s %8s %8s %8s %8s %6s %6s %8s %8s\n", "case", "N", "mean_cyc", "p99", "p99.9", "max",
           "iters", "depth", "mean_us", "us/round");
    auto g = std::make_unique<uf::Graph>();
    auto ws = std::make_unique<uf::Workspace>();
    for (const auto& dir : dirs) {
        Case c;
        std::string err = load_case(dir, c);
        if (!err.empty()) { fprintf(stderr, "%s\n", err.c_str()); continue; }
        if ((int)c.num_det > uf::MAX_DET || (int)c.num_edges > uf::MAX_EDGES) {
            fprintf(stderr, "%s: graph too large for this build\n", dir.c_str()); continue;
        }
        g->num_det = c.num_det; g->num_obs = c.num_obs; g->num_edges = c.num_edges;
        for (uint32_t e = 0; e < c.num_edges; ++e) {
            g->eu[e] = c.graph[3 + 4 * e]; g->ev[e] = c.graph[4 + 4 * e];
            g->w[e] = c.graph[5 + 4 * e]; g->obs[e] = c.graph[6 + 4 * e];
            if (g_quant > 1) g->w[e] = std::max<int>(1, ((int)g->w[e] + g_quant / 2) / g_quant);
        }
        uf::build_adjacency(*g);
        const int n = c.num_det + 1;
        const int L_red = (int)std::ceil(std::log2((double)n) / 2.0);
        std::vector<uint64_t> cyc;
        double iters = 0, ecc = 0, depth = 0, units = 0;
        uint32_t wmax = 0;
        for (uint32_t e = 0; e < c.num_edges; ++e) wmax = std::max<uint32_t>(wmax, g->w[e]);
        size_t bad = 0, alt_diff = 0, alt_err = 0, ref_err = 0;
        std::vector<uint8_t> obs_true;
        read_bytes(dir + "/obs.bin", obs_true);
        std::vector<uint8_t> defect(c.num_det);
        std::vector<uint8_t> alt_bytes;
        for (size_t s = 0; s < c.shots; ++s) {
            for (uint32_t i = 0; i < c.num_det; ++i)
                defect[i] = (c.dets[s * c.det_words + i / 32] >> (i % 32)) & 1;
            const uf::Result r = uf::decode(*g, defect.data(), *ws);
            if (r.obs_flip != c.expected[s]) ++bad;
            const Cost k = model_shot(*g, defect.data(), L_red);
            cyc.push_back(k.cycles);
            if (k.alt_obs != r.obs_flip) ++alt_diff;
            alt_bytes.push_back((uint8_t)k.alt_obs);
            if (obs_true.size() == c.shots) {
                ref_err += (r.obs_flip & 1) != (obs_true[s] & 1);
                alt_err += (k.alt_obs & 1) != (obs_true[s] & 1);
            }
            iters += k.iters; ecc += k.ecc_sum; depth += k.depth; units += k.units;
        }
        if (write_alt && c.num_obs <= 8) {
            FILE* f = fopen((dir + "/par_expected.bin").c_str(), "wb");
            if (f) { fwrite(alt_bytes.data(), 1, alt_bytes.size(), f); fclose(f); }
        }
        double mean = 0;
        for (auto x : cyc) mean += x;
        mean /= cyc.size();
        // d from the case name (..._d<d>_...), for per-round numbers.
        int d = 0;
        size_t p = dir.find("_d");
        if (p != std::string::npos) d = atoi(dir.c_str() + p + 2);
        const double us = mean / mhz;
        std::string name = dir.substr(dir.find_last_of('/') + 1);
        printf("%-36s %6d %8.1f %8.0f %8.0f %8.0f %6.2f %6.2f %8.3f %8.3f%s\n", name.c_str(), n, mean,
               pct(cyc, 0.99), pct(cyc, 0.999), pct(cyc, 1.0), iters / c.shots, depth / c.shots, us,
               d ? us / d : 0.0, bad ? "  GOLDEN MISMATCH" : "");
        printf("    parallel-forest peeling: %zu / %zu predictions differ; logical errors %zu (reference %zu)\n",
               alt_diff, c.shots, alt_err, ref_err);
        printf("    weights /%d (max %u): %.2f growth iterations, %.2f growth units per shot\n", g_quant, wmax,
               iters / c.shots, units / c.shots);
        if (out)
            fprintf(out, "%s,%d,%u,%u,%zu,%.1f,%.0f,%.0f,%.0f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%d\n", name.c_str(), d,
                    c.num_det, c.num_edges, c.shots, mean, pct(cyc, 0.99), pct(cyc, 0.999), pct(cyc, 1.0),
                    iters / c.shots, ecc / c.shots, depth / c.shots, us, d ? us / d : 0.0,
                    d ? pct(cyc, 0.999) / mhz / d : 0.0, bad == 0);
    }
    if (out) fclose(out);
    return 0;
}
