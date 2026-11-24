"""Per-stage cycle breakdown of the decoder, from the loop counts the kernel reports.

Usage: python python/stage_breakdown.py <work_csv> [<case_filter>] [--us-per-shot X] [--version v6|v7|v8|v9c]

work_csv is a per-shot CSV from build/host (its work_csv argument) or from
build/sw/kernel_tb --work-csv: grow_iters, work, defects and the per-stage loop counts
(src/info_layout.h). With kernel_tb CSVs, <case_filter> keeps only the rows whose case
contains it.

Every stage's cycles = its loop iterations x the loop's initiation interval, plus one
pipeline fill (the loop's depth) per loop start. The iteration counts are counted by
the decoder itself; the IIs and depths come from the Vitis HLS synthesis log
(Pipelining result lines), so update the tables when the kernel changes. --version
picks the loop structure (default v6): v7 has the 12-lane frontier and the one-loop
peeling search, v8 adds the 2-wide growth step, the one-loop member lists and the
smaller cleanup. The v7 / v8 IIs and depths marked "assumed" are guesses until those
versions are synthesized. With
--us-per-shot (the measured kernel time on the card) it also prints how much of the
measured time the stages account for; the rest is control overhead between loops
(state machine transitions, loop exits) that HLS does not report per loop.
"""

import csv
import sys

CLOCK_MHZ = 300.0

# stage: (cycles per shot as a function of the per-shot means, description).
# II / depth from the v6 synthesis (uf_fast.h loops), see the module docstring.
STAGES = {
    "members":   (lambda m: 1 * m["members"] + 2 * m["grow_iters"], "member lists (II 1)"),
    "frontier":  (lambda m: 1 * m["frontier"] + 17 * m["grow_iters"], "frontier scan (II 1, depth 17)"),
    "apply":     (lambda m: 1 * m["apply"] + 8 * m["grow_iters"], "growth step (II 1, depth 8)"),
    "merge":     (lambda m: 1 * m["relabel"] + 8 * m["merges"], "merges + relabel (II 1)"),
    "compact":   (lambda m: 2 * m["compact"] + 12 * m["grow_iters"], "active-list compaction (2 passes, II 1)"),
    "peel_scan": (lambda m: 2 * m["peel_scan"] + 9 * m["peel_walk"], "peeling search (II 2)"),
    "peel_walk": (lambda m: 6 * m["peel_walk"] + 10 + 1 * m["peel_walk"] + 5, "peel walk (II 6) + check (II 1)"),
    "cleanup":   (lambda m: 1 * m["cleanup"] + 9, "reset after the shot (II 1)"),
    "unpack":    (lambda m: 1 * m["defects"] + 2, "syndrome unpack (II 1)"),
}

# v7: the frontier takes one member per step (frontier = steps), the peeling search is
# one loop over nodes, grown edges and starts (peel_scan = steps). Assumed, not yet
# synthesized: frontier II 1 / depth 20, peeling search II 2 / depth 10.
STAGES_V7 = dict(STAGES)
STAGES_V7["frontier"] = (lambda m: 1 * m["frontier"] + 20 * m["grow_iters"],
                         "frontier, 12 lanes (II 1, depth 20 assumed)")
STAGES_V7["peel_scan"] = (lambda m: 2 * m["peel_scan"] + 10,
                          "peeling search, one loop (II 2, depth 10 assumed)")

# v8: growth step takes two edges per step; member lists are one loop per iteration
# (a step per member and per cluster); cleanup = its steps (counted as such).
STAGES_V8 = dict(STAGES_V7)
STAGES_V8["members"] = (lambda m: 1 * m["members"] + 1 * m["compact"] + 3 * m["grow_iters"],
                        "member lists, one loop (II 1, depth 3 assumed)")
STAGES_V8["apply"] = (lambda m: 0.5 * m["apply"] + 0.5 * m["grow_iters"] + 9 * m["grow_iters"],
                      "growth step, 2 edges/step (II 1, depth 9 assumed)")
STAGES_V8["cleanup"] = (lambda m: 1 * m["cleanup"] + 15, "reset after the shot (3 loops, II 1)")

# v9c (synthesized, results/hw/synth_v7_v9/v9c): same counters as v8. Member lists
# II 2 / depth 4 over one step per member and per cluster; frontier II 1 / depth 13;
# growth step II 1 / depth 10 (two edges per step); boundary-edge loop II 2 / depth 11
# and peeling search II 6 / depth 10 (peel_scan counts both).
STAGES_V9 = dict(STAGES_V8)
STAGES_V9["members"] = (lambda m: 2 * (m["members"] + m["compact"]) + 4 * m["grow_iters"],
                        "member lists, one loop (II 2, depth 4)")
STAGES_V9["frontier"] = (lambda m: 1 * m["frontier"] + 13 * m["grow_iters"],
                         "frontier, 12 lanes (II 1, depth 13)")
STAGES_V9["apply"] = (lambda m: 0.5 * m["apply"] + 0.5 * m["grow_iters"] + 10 * m["grow_iters"],
                      "growth step, 2 edges/step (II 1, depth 10)")
STAGES_V9["peel_scan"] = (lambda m: 6 * m["peel_scan"] + 21,
                          "peeling search, one loop (II 6, depth 10) + boundary edges")

VERSIONS = {"v6": STAGES, "v7": STAGES_V7, "v8": STAGES_V8, "v9c": STAGES_V9}


def main():
    args = [a for a in sys.argv[1:]]
    us_meas = None
    stages = STAGES
    if "--version" in args:
        i = args.index("--version")
        stages = VERSIONS[args[i + 1]]
        del args[i:i + 2]
    if "--us-per-shot" in args:
        i = args.index("--us-per-shot")
        us_meas = float(args[i + 1])
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__)
    path = args[0]
    filt = args[1] if len(args) > 1 else None

    keys = ["grow_iters", "defects", "members", "frontier", "apply", "merges", "relabel",
            "compact", "peel_scan", "peel_walk", "cleanup"]
    sums = dict.fromkeys(keys, 0.0)
    n = 0
    with open(path) as f:
        for row in csv.DictReader(f):
            if filt and filt not in row.get("case", ""):
                continue
            for k in keys:
                sums[k] += float(row[k])
            n += 1
    if n == 0:
        sys.exit("stage_breakdown: no rows matched")
    mean = {k: v / n for k, v in sums.items()}

    cycles = {name: fn(mean) for name, (fn, _) in stages.items()}
    total = sum(cycles.values())
    print(f"{n} shots, {mean['defects']:.1f} defects and {mean['grow_iters']:.2f} growth "
          f"iterations per shot")
    print(f"{'stage':<10} {'cycles/shot':>12} {'share':>7}   loop")
    for name, (_, desc) in stages.items():
        print(f"{name:<10} {cycles[name]:12.0f} {100 * cycles[name] / total:6.1f}%   {desc}")
    print(f"{'total':<10} {total:12.0f}          = {total / CLOCK_MHZ:.2f} us at {CLOCK_MHZ:.0f} MHz")
    if us_meas is not None:
        meas = us_meas * CLOCK_MHZ
        print(f"measured   {meas:12.0f}          ({us_meas:.2f} us); the stages account for "
              f"{100 * total / meas:.0f}%, the rest is control overhead between loops")


if __name__ == "__main__":
    main()
