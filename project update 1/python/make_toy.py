"""Write the hand-checkable toy test cases into tests/cases/.

Each toy case is a code-capacity repetition code of distance d (bit-flip errors on
d data qubits, perfect measurements, all edge weights 1):

    B --q0-- D0 --q1-- D1 -- ... -- D(d-2) --q(d-1)-- B

Detector Di = q(i) XOR q(i+1). The observable is the flip of q0. The shots are every
error pattern with at most t = (d-1)//2 flipped qubits. A distance-d code can always
correct those, and union-find is proven to correct them, so the expected prediction
is simply the true observable flip. This makes the toy cases a correctness test that
does not depend on the decoder's own output.
"""

import itertools
import json
import os

import numpy as np


def make_case(d, root):
    n_det = d - 1
    out = os.path.join(root, f"toy_rep_d{d}")
    os.makedirs(out, exist_ok=True)

    # Edge for qubit q: q0 -> (D0, B), q(d-1) -> (D(d-2), B), else (D(q-1), D(q)).
    edges = []
    for q in range(d):
        if q == 0:
            edges.append((0, -1, 1, 1))   # q0 is the observable
        elif q == d - 1:
            edges.append((d - 2, -1, 1, 0))
        else:
            edges.append((q - 1, q, 1, 0))
    with open(os.path.join(out, "graph.txt"), "w") as f:
        f.write(f"# toy code-capacity repetition code d={d}, all weights 1\n")
        f.write("# header: num_detectors num_observables num_edges\n")
        f.write("# edges:  u v weight obs_mask   (v = -1 is the boundary)\n")
        f.write(f"{n_det} 1 {len(edges)}\n")
        for u, v, w, m in edges:
            f.write(f"{u} {v} {w} {m}\n")

    t = (d - 1) // 2
    dets, obs = [], []
    for k in range(t + 1):
        for flipped in itertools.combinations(range(d), k):
            err = np.zeros(d, dtype=np.uint8)
            err[list(flipped)] = 1
            dets.append([err[i] ^ err[i + 1] for i in range(n_det)])
            obs.append([err[0]])
    dets = np.packbits(np.array(dets, dtype=np.uint8), axis=1, bitorder="little")
    obs = np.packbits(np.array(obs, dtype=np.uint8), axis=1, bitorder="little")
    dets.tofile(os.path.join(out, "dets.bin"))
    obs.tofile(os.path.join(out, "obs.bin"))
    obs.tofile(os.path.join(out, "expected_pred.bin"))

    meta = {
        "code": "toy_rep",
        "stim_task": "toy code-capacity repetition code",
        "distance": d,
        "rounds": 1,
        "p": None,
        "shots": int(dets.shape[0]),
        "num_detectors": n_det,
        "num_observables": 1,
        "num_edges": len(edges),
        "note": f"all error patterns of weight <= {t}; expected_pred.bin = true observable flips",
    }
    with open(os.path.join(out, "meta.json"), "w") as f:
        json.dump(meta, f, indent=2)
    print(f"make_toy: wrote {out} ({dets.shape[0]} shots)")


def main():
    root = os.path.join("tests", "cases")
    for d in (3, 5):
        make_case(d, root)


if __name__ == "__main__":
    main()
