"""Plot decode time per round against code distance, from the U280 hardware sweeps.

Usage: python python/plot_round_budget.py [out_png]

Left: mean time per round at p = 0.001 for every kernel version (v1..v5).
Right: v5 mean and tail (p99, p99.9) per round at p = 0.001 and 0.003.
Reads results/hw/hw_sweep*.csv (written by hw_summary.py). Time per round is the
time per shot divided by d, since every shot has d rounds.
"""

import csv
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402

VERSIONS = [("v1", ""), ("v2", "_v2"), ("v3", "_v3"), ("v4", "_v4"), ("v5", "_v5")]
DISTANCES = [3, 5, 7, 9, 11]
COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
INK, MUTED, GRID = "#1f1f1f", "#6b6b6b", "#e6e6e6"


def load(path):
    with open(path) as f:
        return {(r["code"], int(r["d"]), r["p"]): r for r in csv.DictReader(f)}


def per_round(table, p, key):
    return [float(table[("surf_z", d, p)][key]) / d for d in DISTANCES]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "results/hw/fig_round_budget.png"
    plt.rcParams.update({"font.size": 10, "axes.edgecolor": MUTED, "axes.labelcolor": INK,
                         "xtick.color": MUTED, "ytick.color": MUTED})
    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4.3))
    for ax in (a, b):
        ax.set_yscale("log")
        ax.set_xticks(DISTANCES)
        ax.grid(axis="y", color=GRID, lw=0.8)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        ax.axhline(1.0, color=MUTED, lw=1.2, ls=(0, (4, 3)))
        ax.set_xlabel("code distance d")
        ax.set_ylabel("µs per round")
    a.set_xlim(2.5, 12.6)
    b.set_xlim(2.5, 11.5)
    a.text(3.6, 1.12, "1 µs syndrome round", color=MUTED, fontsize=9)
    b.text(5.4, 0.86, "1 µs syndrome round", color=MUTED, fontsize=9, va="top")

    # Left: every version, mean only. Only the well-separated line ends are labelled.
    for (name, suffix), color in zip(VERSIONS, COLORS):
        y = per_round(load(f"results/hw/hw_sweep{suffix}.csv"), "0.001", "us_per_shot")
        a.plot(DISTANCES, y, color=color, lw=2, marker="o", ms=6, label=name)
        if name in ("v1", "v2", "v5"):
            a.text(11.3, y[-1], f"{name}  {y[-1]:.2f}", color=INK, va="center", fontsize=9)
    a.set_title("Mean decode time per round, p = 0.001 (measured on U280)",
                fontsize=10.5, color=INK, loc="left")
    a.legend(frameon=False, fontsize=9, loc="lower right", bbox_to_anchor=(1.0, 0.0))

    # Right: v5, mean and tail, two error rates.
    v5 = load("results/hw/hw_sweep_v5.csv")
    for key, ls in [("us_per_shot", "-"), ("p99_us", "--"), ("p999_us", ":")]:
        for p, color in [("0.001", COLORS[0]), ("0.003", COLORS[1])]:
            b.plot(DISTANCES, per_round(v5, p, key), color=color, lw=2, ls=ls,
                   marker="o" if ls == "-" else None, ms=5)
    b.set_title("v5 latency per round: mean and tail", fontsize=10.5, color=INK, loc="left")
    handles = [Line2D([], [], color=COLORS[0], lw=2), Line2D([], [], color=COLORS[1], lw=2),
               Line2D([], [], color=MUTED, lw=2), Line2D([], [], color=MUTED, lw=2, ls="--"),
               Line2D([], [], color=MUTED, lw=2, ls=":")]
    b.legend(handles, ["p = 0.001", "p = 0.003", "mean (measured)", "p99 (est.)", "p99.9 (est.)"],
             frameon=False, fontsize=8.5, loc="lower right")

    fig.text(0.01, 0.005, "Rotated surface code, d rounds, circuit-level noise, 10k shots per "
             "point. Tails estimated from per-shot work × measured cycles/work.",
             fontsize=8, color=MUTED)
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(out, dpi=160)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
