"""Report figures from the measured and projected results.

Usage: python python/plot_results.py [out_dir]     (default results/figures)

  fig_versions.png      time per round at d = 11, p = 0.001, every kernel version:
                        v1..v5 measured on the U280, v7 / v8 projected
  fig_stages.png        where the cycles of one d = 11 shot go: v5 measured (stage
                        cycles from the decoder's own loop counts, the rest is control
                        overhead between loops), v7 and v8 projected
  fig_cpu_fpga.png      same decoder on an Apple M3 core vs the U280 (v5)
  fig_accuracy.png      logical error rate, this decoder vs PyMatching, 1M shots

Inputs: results/hw/hw_sweep*.csv and hw_long_v5.csv (hw_summary.py),
results/stage_counts.csv (mean loop counts per shot), results/cpu_baseline_m3.txt
(kernel_tb --time), results/sweep_1m.csv (sweep.py). v7 / v8 projections: their stage
cycles (stage_breakdown.py, assumed IIs for the new loops) plus the control overhead
measured for v5, unchanged.
"""

import csv
import os
import re
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stage_breakdown import VERSIONS, CLOCK_MHZ  # noqa: E402

COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300"]
GRAY = "#a3a3a3"
INK, MUTED, GRID = "#1f1f1f", "#6b6b6b", "#e6e6e6"
LONG = {"surf_z_d7_p0.001_n200000": 7, "surf_z_d11_p0.001_n200000": 11,
        "surf_z_d11_p0.003_n200000": 11}


def style(ax, ygrid=True):
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    if ygrid:
        ax.grid(axis="y", color=GRID, lw=0.8)
    ax.set_axisbelow(True)


def note(fig, text):
    fig.text(0.01, 0.01, text, fontsize=8, color=MUTED)


def load_counts():
    counts = {}
    with open("results/stage_counts.csv") as f:
        for r in csv.DictReader(f):
            counts[(r["version"], r["case"])] = {k: float(v) for k, v in r.items()
                                                 if k not in ("version", "case", "shots")}
    return counts


def v5_measured_us():
    with open("results/hw/hw_long_v5.csv") as f:
        return {f"surf_z_d{r['d']}_p{r['p']}_n200000": float(r["us_per_shot"])
                for r in csv.DictReader(f)}


def stage_cycles(version, mean):
    return {name: fn(mean) for name, (fn, _) in VERSIONS[version].items()}


def projected_us(counts, meas, case):
    """v5 measured, and v7 / v8 projected (stage cycles + v5's measured overhead)."""
    overhead = meas[case] * CLOCK_MHZ - sum(stage_cycles("v6", counts[("v6", case)]).values())
    out = {"v5": meas[case]}
    for v in ("v7", "v8"):
        out[v] = (sum(stage_cycles(v, counts[(v, case)]).values()) + overhead) / CLOCK_MHZ
    return out, overhead


def fig_versions(out, counts, meas):
    case, d = "surf_z_d11_p0.001_n200000", 11
    names, vals, proj = [], [], []
    for name, suffix in [("v1", ""), ("v2", "_v2"), ("v3", "_v3"), ("v4", "_v4"), ("v5", "_v5")]:
        with open(f"results/hw/hw_sweep{suffix}.csv") as f:
            row = [r for r in csv.DictReader(f) if r["code"] == "surf_z" and r["d"] == "11"
                   and r["p"] == "0.001"][0]
        names.append(name)
        vals.append(float(row["us_per_shot"]) / d)
        proj.append(False)
    p, _ = projected_us(counts, meas, case)
    for v in ("v7", "v8"):
        names.append(v + "\n(projected)")
        vals.append(p[v] / d)
        proj.append(True)

    fig, ax = plt.subplots(figsize=(8, 4.2))
    style(ax)
    for i, (v, pr) in enumerate(zip(vals, proj)):
        ax.bar(i, v, width=0.6, color="white" if pr else COLORS[0], edgecolor=COLORS[0],
               lw=1.5, hatch="///" if pr else None)
        ax.text(i, v * 1.12, f"{v:.2f}" if v < 10 else f"{v:.0f}", ha="center", fontsize=9,
                color=INK)
    ax.set_yscale("log")
    ax.set_xticks(range(len(names)), names, fontsize=9)
    ax.axhline(1.0, color=MUTED, lw=1.2, ls=(0, (4, 3)), label="1 µs syndrome round")
    ax.legend(frameon=False, fontsize=9, loc="upper right")
    ax.set_ylabel("µs per round (mean)")
    ax.set_title("Decode time per round, rotated surface code d = 11, p = 0.001",
                 fontsize=10.5, color=INK, loc="left")
    note(fig, "v1–v5 measured on the U280 at 300 MHz (10k shots). v7/v8: decoder loop counts × "
              "assumed IIs for the new loops + v5's measured overhead; not yet synthesized.")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(os.path.join(out, "fig_versions.png"), dpi=160)


def fig_stages(out, counts, meas):
    case = "surf_z_d11_p0.001_n200000"
    groups = [("frontier scan", ["frontier"]), ("growth step", ["apply"]),
              ("cluster bookkeeping", ["members", "merge", "compact"]),
              ("peeling", ["peel_scan", "peel_walk"]), ("cleanup", ["cleanup"]),
              ("unpack", ["unpack"])]
    _, overhead = projected_us(counts, meas, case)
    rows = [("v5 (measured)", "v6"), ("v7 (projected)", "v7"), ("v8 (projected)", "v8")]
    fig, ax = plt.subplots(figsize=(9, 3.6))
    style(ax, ygrid=False)
    ax.grid(axis="x", color=GRID, lw=0.8)
    for yi, (label, ver) in enumerate(rows):
        cyc = stage_cycles(ver, counts[(ver, case)])
        left = 0
        for gi, (gname, parts) in enumerate(groups):
            w = sum(cyc[p] for p in parts)
            ax.barh(yi, w, left=left, height=0.6, color=COLORS[gi], edgecolor="white", lw=2,
                    label=gname if yi == 0 else None)
            left += w
        ax.barh(yi, overhead, left=left, height=0.6, color=GRAY, edgecolor="white", lw=2,
                label="between-loop overhead (v5, measured)" if yi == 0 else None)
        ax.text(left + overhead + 40, yi, f"{(left + overhead):.0f} cycles = "
                f"{(left + overhead) / CLOCK_MHZ:.1f} µs", va="center", fontsize=9, color=INK)
    ax.set_yticks(range(len(rows)), [r[0] for r in rows])
    ax.invert_yaxis()
    ax.set_xlim(0, 5200)
    ax.set_xlabel("clock cycles per shot (300 MHz)")
    ax.set_title("Where one d = 11, p = 0.001 shot spends its cycles", fontsize=10.5,
                 color=INK, loc="left")
    ax.legend(frameon=False, fontsize=8, ncol=4, loc="upper center",
              bbox_to_anchor=(0.5, -0.28))
    note(fig, "Stage cycles = loop counts reported by the decoder × each loop's II (+ fill). "
              "v5 total measured on the U280; v7/v8 IIs for new loops assumed.")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(os.path.join(out, "fig_stages.png"), dpi=160)


def fig_cpu_fpga(out, meas):
    cpu = []
    with open("results/cpu_baseline_m3.txt") as f:
        for line in f:
            m = re.search(r"mean ([\d.]+) us, p50 [\d.]+, p99 [\d.]+, p99.9 ([\d.]+)", line)
            if m:
                cpu.append((float(m.group(1)), float(m.group(2))))
    # cpu_baseline_m3.txt lists the cases in data/long order (d11 p.001, d11 p.003, d7)
    order = ["surf_z_d11_p0.001_n200000", "surf_z_d11_p0.003_n200000", "surf_z_d7_p0.001_n200000"]
    cpu = dict(zip(order, cpu))
    with open("results/hw/hw_long_v5.csv") as f:
        fpga = {f"surf_z_d{r['d']}_p{r['p']}_n200000": (float(r["us_per_shot"]), float(r["p999_us"]))
                for r in csv.DictReader(f)}
    cases = ["surf_z_d7_p0.001_n200000", "surf_z_d11_p0.001_n200000", "surf_z_d11_p0.003_n200000"]
    labels = ["d=7, p=0.001", "d=11, p=0.001", "d=11, p=0.003"]
    fig, ax = plt.subplots(figsize=(8, 4))
    style(ax)
    xs = range(len(cases))
    for off, src, color, name in [(-0.2, cpu, COLORS[0], "CPU (M3, 1 core)"),
                                  (0.2, fpga, COLORS[1], "FPGA (U280, v5)")]:
        ax.bar([x + off for x in xs], [src[c][0] for c in cases], width=0.36, color=color,
               label=name + ": mean")
        ax.scatter([x + off for x in xs], [src[c][1] for c in cases], marker="_", s=300,
                   color=INK, zorder=3)
    ax.scatter([], [], marker="_", s=200, color=INK, label="p99.9 (FPGA: estimated from work)")
    ax.set_yscale("log")
    ax.set_xticks(list(xs), labels)
    ax.set_ylabel("µs per shot")
    ax.legend(frameon=False, fontsize=8.5, loc="upper left")
    ax.set_title("Same decoder: laptop CPU vs FPGA", fontsize=10.5, color=INK, loc="left")
    note(fig, "200k shots per case. CPU: g++ -O2, decode only (kernel_tb --time). FPGA: mean "
              "measured on the card; p99.9 = work × measured cycles/work.")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(os.path.join(out, "fig_cpu_fpga.png"), dpi=160)


def fig_accuracy(out):
    with open("results/sweep_1m.csv") as f:
        rows = list(csv.DictReader(f))
    fig, ax = plt.subplots(figsize=(7, 4))
    style(ax)
    for p, color in [("0.003", COLORS[0]), ("0.005", COLORS[1])]:
        rs = sorted([r for r in rows if r["p"] == p], key=lambda r: int(r["d"]))
        ds = [int(r["d"]) for r in rs]
        ax.plot(ds, [float(r["uf_ler"]) for r in rs], color=color, lw=2, marker="o", ms=6,
                label=f"union-find (this decoder), p = {p}")
        ax.plot(ds, [float(r["pm_ler"]) for r in rs], color=color, lw=2, ls="--", marker="s",
                ms=5, label=f"PyMatching (MWPM), p = {p}")
        ax.text(ds[-1] + 0.15, float(rs[-1]["uf_ler"]),
                f"×{float(rs[-1]['uf_ler']) / float(rs[-1]['pm_ler']):.2f}", va="center",
                fontsize=9, color=INK)
    ax.set_yscale("log")
    ax.set_xticks([5, 7, 9, 11])
    ax.set_xlim(4.5, 11.8)
    ax.set_xlabel("code distance d")
    ax.set_ylabel("logical error rate per shot")
    ax.legend(frameon=False, fontsize=8.5, loc="lower left")
    ax.set_title("Accuracy: union-find vs minimum-weight matching", fontsize=10.5, color=INK,
                 loc="left")
    note(fig, "1M shots per point, rotated surface code memory-Z, d rounds, circuit-level noise. "
              "FPGA output is bit-exact to this decoder.")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(os.path.join(out, "fig_accuracy.png"), dpi=160)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join("results", "figures")
    os.makedirs(out, exist_ok=True)
    plt.rcParams.update({"font.size": 10, "axes.edgecolor": MUTED, "axes.labelcolor": INK,
                         "xtick.color": MUTED, "ytick.color": MUTED})
    counts, meas = load_counts(), v5_measured_us()
    fig_versions(out, counts, meas)
    fig_stages(out, counts, meas)
    fig_cpu_fpga(out, meas)
    fig_accuracy(out)
    print(f"wrote 4 figures to {out}")


if __name__ == "__main__":
    main()
