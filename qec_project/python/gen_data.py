"""Generate QEC decoding data with Stim for the union-find decoder.

Example:
    python python/gen_data.py --code surf_z --d 3 --p 0.01 --shots 100000

Writes a case directory (default data/<code>_d<d>_p<p>_n<shots>) containing:
    graph.txt            decoding graph: "u v weight obs_mask" per edge, v = -1 is the boundary
    dets.bin             detection events, ceil(num_det/8) bytes per shot (little-endian bits)
    obs.bin              actual observable flips, ceil(num_obs/8) bytes per shot
    pymatching_pred.bin  PyMatching (MWPM) predictions, same format as obs.bin, as a baseline
    meta.json            parameters and versions used
"""

import argparse
import json
import math
import os

import numpy as np
import pymatching
import stim

CODES = {
    "rep": "repetition_code:memory",
    "surf_z": "surface_code:rotated_memory_z",
    "surf_x": "surface_code:rotated_memory_x",
    "unsurf_z": "surface_code:unrotated_memory_z",
}


def build_circuit(code, d, rounds, p):
    # Same circuit-level noise model as simulation/generate_test_syndromes.py.
    return stim.Circuit.generated(
        CODES[code],
        distance=d,
        rounds=rounds,
        after_clifford_depolarization=p,
        before_round_data_depolarization=p,
        after_reset_flip_probability=p,
        before_measure_flip_probability=p,
    )


def dem_to_edges(dem, num_det):
    """Turn a decomposed detector error model into a list of graph edges.

    Each error mechanism is split at its separators into graph-like pieces with one
    or two detectors. Parallel edges are merged by XOR-combining their probabilities.
    Returns {(u, v): [prob, obs_mask]} with v = num_det for the boundary, plus counts
    of skipped and conflicting pieces.
    """
    edges = {}
    obs_only = 0
    conflicts = 0
    for inst in dem.flattened():
        if inst.type != "error":
            continue
        prob = inst.args_copy()[0]
        pieces = [[]]
        for t in inst.targets_copy():
            if t.is_separator():
                pieces.append([])
            else:
                pieces[-1].append(t)
        for piece in pieces:
            dets = [t.val for t in piece if t.is_relative_detector_id()]
            mask = 0
            for t in piece:
                if t.is_logical_observable_id():
                    mask ^= 1 << t.val
            if len(dets) == 0:
                obs_only += 1   # undetectable piece, no decoder can see it
                continue
            if len(dets) > 2:
                raise ValueError(f"error is not graph-like after decomposition: {inst}")
            u, v = (dets[0], num_det) if len(dets) == 1 else sorted(dets)
            if (u, v) not in edges:
                edges[(u, v)] = [prob, mask]
                continue
            old_p, old_mask = edges[(u, v)]
            if old_mask != mask:
                conflicts += 1
                if prob > old_p:
                    edges[(u, v)][1] = mask
            edges[(u, v)][0] = old_p * (1 - prob) + prob * (1 - old_p)
    return edges, obs_only, conflicts


def pack(bits):
    return np.packbits(bits.astype(np.uint8), axis=1, bitorder="little")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--code", choices=sorted(CODES), required=True)
    ap.add_argument("--d", type=int, required=True, help="code distance")
    ap.add_argument("--p", type=float, required=True, help="physical error rate")
    ap.add_argument("--shots", type=int, required=True)
    ap.add_argument("--rounds", type=int, default=None, help="syndrome rounds (default: d)")
    ap.add_argument("--seed", type=int, default=5698)
    ap.add_argument("--wscale", type=float, default=2.0,
                    help="integer weight = max(1, round(wscale * ln((1-p)/p)))")
    ap.add_argument("--out", default=None, help="output directory")
    args = ap.parse_args()

    rounds = args.rounds if args.rounds is not None else args.d
    out = args.out or os.path.join("data", f"{args.code}_d{args.d}_p{args.p:g}_n{args.shots}")
    os.makedirs(out, exist_ok=True)

    circuit = build_circuit(args.code, args.d, rounds, args.p)
    dem = circuit.detector_error_model(decompose_errors=True)
    num_det = circuit.num_detectors
    num_obs = circuit.num_observables

    edges, obs_only, conflicts = dem_to_edges(dem, num_det)
    with open(os.path.join(out, "graph.txt"), "w") as f:
        f.write(f"# {CODES[args.code]} d={args.d} rounds={rounds} p={args.p:g}\n")
        f.write("# header: num_detectors num_observables num_edges\n")
        f.write("# edges:  u v weight obs_mask   (v = -1 is the boundary)\n")
        f.write(f"{num_det} {num_obs} {len(edges)}\n")
        for (u, v), (prob, mask) in sorted(edges.items()):
            llr = math.log((1 - prob) / prob) if 0 < prob < 0.5 else 0.0
            w = max(1, round(args.wscale * llr))
            f.write(f"{u} {-1 if v == num_det else v} {w} {mask}\n")

    sampler = circuit.compile_detector_sampler(seed=args.seed)
    dets, obs = sampler.sample(args.shots, separate_observables=True)
    pack(dets).tofile(os.path.join(out, "dets.bin"))
    pack(obs).tofile(os.path.join(out, "obs.bin"))

    matching = pymatching.Matching.from_detector_error_model(dem)
    pm_pred = matching.decode_batch(dets)
    pack(pm_pred).tofile(os.path.join(out, "pymatching_pred.bin"))
    pm_errors = int(np.any(pm_pred != obs, axis=1).sum())

    meta = {
        "code": args.code,
        "stim_task": CODES[args.code],
        "distance": args.d,
        "rounds": rounds,
        "p": args.p,
        "shots": args.shots,
        "seed": args.seed,
        "wscale": args.wscale,
        "num_detectors": num_det,
        "num_observables": num_obs,
        "num_edges": len(edges),
        "obs_only_pieces_skipped": obs_only,
        "parallel_edge_obs_conflicts": conflicts,
        "pymatching_logical_errors": pm_errors,
        "stim_version": stim.__version__,
        "pymatching_version": pymatching.__version__,
    }
    with open(os.path.join(out, "meta.json"), "w") as f:
        json.dump(meta, f, indent=2)

    print(f"gen_data: wrote {out}")
    print(f"gen_data: {CODES[args.code]} d={args.d} rounds={rounds} p={args.p:g} shots={args.shots} seed={args.seed}")
    print(f"gen_data: detectors={num_det} observables={num_obs} edges={len(edges)}")
    if obs_only or conflicts:
        print(f"gen_data: note: {obs_only} undetectable pieces skipped, {conflicts} parallel-edge observable conflicts")
    print(f"gen_data: PyMatching logical errors = {pm_errors} / {args.shots}")


if __name__ == "__main__":
    main()
