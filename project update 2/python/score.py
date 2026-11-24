"""Score the software decoder's predictions for one case directory.

Usage: python python/score.py <case_dir>

Compares sw_pred.bin (written by `make sw`) against the actual observable flips in
obs.bin, and against PyMatching's predictions if pymatching_pred.bin exists.
"""

import json
import math
import os
import sys

import numpy as np


def load_bits(path, num_bits, shots=None):
    nbytes = (num_bits + 7) // 8
    raw = np.fromfile(path, dtype=np.uint8).reshape(-1, nbytes)
    return np.unpackbits(raw, axis=1, bitorder="little", count=num_bits)


def rate_line(name, errors, shots):
    rate = errors / shots
    stderr = math.sqrt(rate * (1 - rate) / shots)
    return f"score: {name:<28} logical errors = {errors} / {shots} = {rate:.6f} (+/- {stderr:.6f})"


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: python python/score.py <case_dir>")
    case = sys.argv[1]
    with open(os.path.join(case, "meta.json")) as f:
        meta = json.load(f)
    num_obs = meta["num_observables"]

    obs = load_bits(os.path.join(case, "obs.bin"), num_obs)
    sw = load_bits(os.path.join(case, "sw_pred.bin"), num_obs)
    if sw.shape != obs.shape:
        sys.exit(f"score: sw_pred.bin has {sw.shape[0]} shots but obs.bin has {obs.shape[0]}")
    shots = obs.shape[0]

    stats = {}
    with open(os.path.join(case, "sw_stats.txt")) as f:
        for line in f:
            key, _, value = line.strip().partition("=")
            stats[key] = value

    print(f"score: case = {case}")
    print(f"score: {meta.get('stim_task', meta.get('code'))} d={meta.get('distance')} "
          f"rounds={meta.get('rounds')} p={meta.get('p')}")
    print(rate_line("union-find (sw)", int(np.any(sw != obs, axis=1).sum()), shots))
    pm_path = os.path.join(case, "pymatching_pred.bin")
    if os.path.exists(pm_path):
        pm = load_bits(pm_path, num_obs)
        print(rate_line("PyMatching (MWPM baseline)", int(np.any(pm != obs, axis=1).sum()), shots))
        print(f"score: shots where union-find and PyMatching disagree = {int(np.any(sw != pm, axis=1).sum())}")
    print(f"score: decode time = {float(stats['us_per_shot']):.3f} us/shot")
    print(f"score: invalid corrections = {stats['invalid']}")
    print(f"score: cap hits = {stats['cap_hits']}")


if __name__ == "__main__":
    main()
