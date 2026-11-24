"""Summarize a hardware sweep run on the U280 (hls/testbed_run.sh-style host output).

Example:
    python python/hw_summary.py results/hw/hw_sweep.log results/hw/sweep_work

Inputs:
    log       output of build/host for every case ("### <case>" before each run)
    work_dir  one <case>.csv per case from build/host's work_csv argument
              (shot,grow_iters,work)

For each case it prints the measured time per shot and the cycles per unit of work
(measured time x clock / mean work), then turns the per-shot work into a latency
distribution using that case's own cycles per unit of work.
Writes results/hw_sweep.csv.
"""

import csv
import os
import re
import sys

import numpy as np

CLOCK_MHZ = 300.0


def parse_log(path):
    runs, case = {}, None
    for line in open(path):
        if line.startswith("### "):
            case = line[4:].strip()
            runs[case] = {}
        elif case and line.startswith("Kernel time:"):
            m = re.match(r"Kernel time: ([\d.e+]+) us for (\d+) shots", line)
            runs[case]["us"], runs[case]["shots"] = float(m.group(1)), int(m.group(2))
        elif case and line.startswith(("PASS", "FAIL")):
            runs[case]["status"] = line.split(":")[0]
    return runs


def main():
    log, work_dir = sys.argv[1], sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.join("results", "hw_sweep.csv")
    rows = []
    for case, r in parse_log(log).items():
        m = re.match(r"(\w+?)_d(\d+)_p([\d.]+)_n\d+", case)
        w = np.loadtxt(os.path.join(work_dir, case + ".csv"), delimiter=",", skiprows=1, usecols=2)
        us_shot = r["us"] / r["shots"]
        cyc_per_work = us_shot * CLOCK_MHZ / w.mean()
        to_us = cyc_per_work / CLOCK_MHZ
        rows.append(dict(code=m.group(1), d=int(m.group(2)), p=float(m.group(3)),
                         shots=r["shots"], status=r.get("status", "?"),
                         us_per_shot=us_shot, cycles_per_work=cyc_per_work,
                         p50_us=np.percentile(w, 50) * to_us, p99_us=np.percentile(w, 99) * to_us,
                         p999_us=np.percentile(w, 99.9) * to_us, max_us=w.max() * to_us))
    rows.sort(key=lambda x: (x["code"], x["p"], x["d"]))
    with open(out, "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0]))
        wr.writeheader()
        wr.writerows(rows)
    print(f"{'code':7}{'d':>3}{'p':>7}  {'status':6}{'us/shot':>10}{'cyc/work':>9}"
          f"{'p50 us':>10}{'p99 us':>10}{'p99.9 us':>10}{'max us':>10}")
    for x in rows:
        print(f"{x['code']:7}{x['d']:>3}{x['p']:>7}  {x['status']:6}{x['us_per_shot']:>10.2f}"
              f"{x['cycles_per_work']:>9.2f}{x['p50_us']:>10.2f}{x['p99_us']:>10.2f}"
              f"{x['p999_us']:>10.2f}{x['max_us']:>10.2f}")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
