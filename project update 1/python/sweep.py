"""Sweep the software decoder over code distance and physical error rate.

Example:
    python python/sweep.py --codes rep surf_z --ds 3 5 7 9 11 --ps 0.001 0.003 0.005 0.01 --shots 100000

For every (code, d, p) it runs python/gen_data.py (rounds = d), decodes with the C++
decoder (build/sw/golden, build it with `make golden`), and appends one row to a CSV
file with the union-find and PyMatching logical error rates, decode time, invalid
corrections and cap hits.
"""

import argparse
import csv
import json
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from score import load_bits  # noqa: E402

FIELDS = ["code", "d", "rounds", "p", "shots", "detectors", "edges",
          "uf_errors", "uf_ler", "pm_errors", "pm_ler", "uf_pm_disagree",
          "us_per_shot", "max_grow_iters", "invalid", "cap_hits"]


def run_point(code, d, p, shots, seed, golden, data_root, wscale):
    case = os.path.join(data_root, f"{code}_d{d}_p{p:g}_n{shots}")
    subprocess.run([sys.executable, "python/gen_data.py", "--code", code, "--d", str(d),
                    "--p", str(p), "--shots", str(shots), "--seed", str(seed), "--out", case,
                    "--wscale", str(wscale)],
                   check=True, stdout=subprocess.DEVNULL)
    # golden exits with 3 when there were invalid corrections or cap hits; those are
    # recorded in the CSV, so only other exit codes are errors.
    rc = subprocess.run([golden, case], stdout=subprocess.DEVNULL).returncode
    if rc not in (0, 3):
        raise RuntimeError(f"{golden} {case} failed with exit code {rc}")

    with open(os.path.join(case, "meta.json")) as f:
        meta = json.load(f)
    stats = {}
    with open(os.path.join(case, "sw_stats.txt")) as f:
        for line in f:
            key, _, value = line.strip().partition("=")
            stats[key] = value
    n_obs = meta["num_observables"]
    obs = load_bits(os.path.join(case, "obs.bin"), n_obs)
    sw = load_bits(os.path.join(case, "sw_pred.bin"), n_obs)
    pm = load_bits(os.path.join(case, "pymatching_pred.bin"), n_obs)
    uf_err = int(np.any(sw != obs, axis=1).sum())
    pm_err = int(np.any(pm != obs, axis=1).sum())
    return {
        "code": code, "d": d, "rounds": meta["rounds"], "p": p, "shots": shots,
        "detectors": meta["num_detectors"], "edges": meta["num_edges"],
        "uf_errors": uf_err, "uf_ler": f"{uf_err / shots:.6f}",
        "pm_errors": pm_err, "pm_ler": f"{pm_err / shots:.6f}",
        "uf_pm_disagree": int(np.any(sw != pm, axis=1).sum()),
        "us_per_shot": f"{float(stats['us_per_shot']):.3f}",
        "max_grow_iters": stats["max_grow_iters"],
        "invalid": stats["invalid"], "cap_hits": stats["cap_hits"],
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--codes", nargs="+", default=["rep", "surf_z"])
    ap.add_argument("--ds", nargs="+", type=int, default=[3, 5, 7])
    ap.add_argument("--ps", nargs="+", type=float, default=[0.001, 0.003, 0.005, 0.01])
    ap.add_argument("--shots", type=int, default=100000)
    ap.add_argument("--seed", type=int, default=5698)
    ap.add_argument("--golden", default="build/sw/golden")
    ap.add_argument("--wscale", type=float, default=2.0, help="edge weight scale passed to gen_data.py")
    ap.add_argument("--data-root", default=os.path.join("data", "sweep"))
    ap.add_argument("--out", default=os.path.join("results", "sweep.csv"))
    args = ap.parse_args()

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        for code in args.codes:
            for d in args.ds:
                for p in args.ps:
                    row = run_point(code, d, p, args.shots, args.seed, args.golden, args.data_root, args.wscale)
                    writer.writerow(row)
                    f.flush()
                    print(f"sweep: {code:<6} d={d:<2} p={p:<6g} UF {row['uf_errors']:>6} ({row['uf_ler']})  "
                          f"PM {row['pm_errors']:>6} ({row['pm_ler']})  {row['us_per_shot']:>9} us/shot  "
                          f"invalid={row['invalid']} cap={row['cap_hits']}", flush=True)
    print(f"sweep: wrote {args.out}")


if __name__ == "__main__":
    main()
