"""Per-shot growth iterations (mean and tail) and CPU speed of union-find vs PyMatching.

Usage: python python/cpu_latency.py [case_dir ...] [--out results/cpu_latency.csv]
       (default: every case under data/sweep/, made by python/sweep.py)

For each case it decodes all shots twice on one CPU core, back to back:
  - union-find: build/sw/golden (time measured inside the C++ program)
  - PyMatching: Matching.decode_batch on the same shots (graph construction not timed)
Both decoders' predictions are checked against the files the sweep already wrote, so
the timings are of exactly the runs reported in the sweep.
"""

import argparse
import csv
import glob
import json
import os
import subprocess
import sys
import tempfile
import time

import numpy as np
import pymatching

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_data import build_circuit  # noqa: E402
from score import load_bits  # noqa: E402

FIELDS = ["code", "d", "rounds", "p", "shots", "uf_us_per_shot", "pm_us_per_shot",
          "iters_mean", "iters_p50", "iters_p99", "iters_p999", "iters_max"]


def read_stats(path):
    stats = {}
    with open(path) as f:
        for line in f:
            key, _, value = line.strip().partition("=")
            stats[key] = value
    return stats


def measure(case, golden):
    with open(os.path.join(case, "meta.json")) as f:
        meta = json.load(f)
    n_det, n_obs = meta["num_detectors"], meta["num_observables"]

    with tempfile.TemporaryDirectory() as tmp:
        pred, stats_path = os.path.join(tmp, "pred.bin"), os.path.join(tmp, "stats.txt")
        rc = subprocess.run([golden, case, "--out", pred, "--stats", stats_path],
                            stdout=subprocess.DEVNULL).returncode
        if rc not in (0, 3):
            raise RuntimeError(f"{golden} {case} failed with exit code {rc}")
        stats = read_stats(stats_path)
        with open(pred, "rb") as a, open(os.path.join(case, "sw_pred.bin"), "rb") as b:
            if a.read() != b.read():
                raise RuntimeError(f"{case}: union-find predictions differ from the sweep run")

    circuit = build_circuit(meta["code"], meta["distance"], meta["rounds"], meta["p"])
    matching = pymatching.Matching.from_detector_error_model(
        circuit.detector_error_model(decompose_errors=True))
    dets = load_bits(os.path.join(case, "dets.bin"), n_det).astype(np.uint8)
    t0 = time.perf_counter()
    pm_pred = matching.decode_batch(dets)
    pm_s = time.perf_counter() - t0
    if not np.array_equal(pm_pred.astype(np.uint8), load_bits(os.path.join(case, "pymatching_pred.bin"), n_obs)):
        raise RuntimeError(f"{case}: PyMatching predictions differ from the sweep run")

    shots = dets.shape[0]
    return {
        "code": meta["code"], "d": meta["distance"], "rounds": meta["rounds"], "p": meta["p"],
        "shots": shots,
        "uf_us_per_shot": f"{float(stats['us_per_shot']):.3f}",
        "pm_us_per_shot": f"{pm_s * 1e6 / shots:.3f}",
        "iters_mean": f"{float(stats['mean_grow_iters']):.3f}",
        "iters_p50": stats["p50_grow_iters"], "iters_p99": stats["p99_grow_iters"],
        "iters_p999": stats["p999_grow_iters"], "iters_max": stats["max_grow_iters"],
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cases", nargs="*")
    ap.add_argument("--golden", default="build/sw/golden")
    ap.add_argument("--out", default=os.path.join("results", "cpu_latency.csv"))
    args = ap.parse_args()
    cases = args.cases or sorted(glob.glob(os.path.join("data", "sweep", "*")))

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    rows = [measure(c, args.golden) for c in cases]
    rows.sort(key=lambda r: (r["code"], r["d"], r["p"]))
    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    for r in rows:
        print(f"cpu_latency: {r['code']:<6} d={r['d']:<2} p={r['p']:<6g} UF {r['uf_us_per_shot']:>9} us  "
              f"PM {r['pm_us_per_shot']:>8} us  iters mean {r['iters_mean']} p50 {r['iters_p50']} "
              f"p99 {r['iters_p99']} p99.9 {r['iters_p999']} max {r['iters_max']}")
    print(f"cpu_latency: wrote {args.out}")


if __name__ == "__main__":
    main()
