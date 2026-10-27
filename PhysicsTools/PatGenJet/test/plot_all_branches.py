#!/usr/bin/env python3
"""Plot every branch of the three pat::GenJet NanoAOD tables, one histogram per branch, CMS style.

    python3 plot_all_branches.py NANO.root OUT.pdf [--sample TEXT] [--per-page 12]

Writes a multi-page PDF (a grid of histograms per page, one section per table) and, next to it, a text summary
OUT_summary.txt listing for each branch: entries, min, max, mean, number of distinct values, and whether the branch is
constant on this sample (one value only) or holds NaN. Constant branches are drawn too, so nothing is hidden.
Branches that also exist in today's gen tables (GenJet, GenJetAK8, SubGenJetAK8) are marked "(also in today's table)".
"""
import argparse, os, sys
import numpy as np
import uproot, awkward as ak
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import mplhep as hep

hep.style.use("CMS")
C_MAIN, C_CONST = "#2a78d6", "#898781"
TABLES = [("PatGenJet", "GenJet", "AK4 generator jets"), ("PatGenJetAK8", "GenJetAK8", "AK8 generator jets"),
          ("PatSubGenJetAK8", "SubGenJetAK8", "AK8 soft-drop generator subjets")]


def values(tree, name):
    v = tree[name].array(library="ak")
    return ak.to_numpy(ak.flatten(v, axis=None)).astype(float)


def binning(v):
    f = v[np.isfinite(v)]
    if f.size == 0:
        return None, "no finite values"
    u = np.unique(f)
    if u.size == 1:
        return np.array([u[0] - 0.5, u[0] + 0.5]), "constant"
    if u.size <= 25 and np.allclose(u, np.round(u)):  # integer-like: one bin per value
        return np.arange(u.min() - 0.5, u.max() + 1.5, 1.0), "integer"
    lo, hi = np.percentile(f, [0.5, 99.5])
    if lo == hi:
        lo, hi = f.min(), f.max()
    return np.linspace(lo, hi, 41), "continuous"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("nano")
    ap.add_argument("out")
    ap.add_argument("--sample", default=r"$t\bar{t}$, generator level")
    ap.add_argument("--per-page", type=int, default=12)
    a = ap.parse_args()
    if not os.path.isfile(a.nano):
        sys.exit("error: %s not found" % a.nano)
    t = uproot.open(a.nano)["Events"]
    keys = set(t.keys())
    summary = ["%-55s %8s %12s %12s %12s %8s  %s" % ("branch", "entries", "min", "max", "mean", "distinct", "note")]
    ncols, nrows = 3, (a.per_page + 2) // 3
    totals = {"branches": 0, "constant": 0, "nan": 0, "in_today": 0}
    with PdfPages(a.out) as pdf:
        for table, old, title in TABLES:
            names = sorted(k for k in keys if k.startswith(table + "_"))
            if "n" + table in keys:
                names = ["n" + table] + names
            for start in range(0, len(names), a.per_page):
                chunk = names[start:start + a.per_page]
                fig, axes = plt.subplots(nrows, ncols, figsize=(18, 4.6 * nrows))
                for ax in axes.ravel()[len(chunk):]:
                    ax.axis("off")
                for ax, name in zip(axes.ravel(), chunk):
                    v = values(t, name)
                    totals["branches"] += 1
                    bins, kind = binning(v)
                    col = name.split("_", 1)[1] if "_" in name else name
                    today = (old + "_" + col) in keys or (name == "n" + table and ("n" + old) in keys)
                    totals["in_today"] += today
                    nan = int(np.isnan(v).sum())
                    totals["nan"] += nan > 0
                    totals["constant"] += kind == "constant"
                    f = v[np.isfinite(v)]
                    if kind == "constant":
                        ax.set_xticks([]); ax.set_yticks([])
                        ax.text(0.5, 0.5, "same value for every jet:\n%g" % f[0], transform=ax.transAxes, ha="center", va="center", fontsize=18, color=C_CONST)
                    elif bins is not None:
                        h, _ = np.histogram(np.clip(f, bins[0], bins[-1]), bins)
                        hep.histplot(h, bins, ax=ax, color=C_MAIN, lw=2)
                    ax.set_title(name + ("  (also in today's table)" if today else ""), fontsize=13)
                    note = []
                    if kind == "constant":
                        note.append("constant = %g" % f[0])
                    if nan:
                        note.append("NaN: %d" % nan)
                    ax.text(0.97, 0.93, "\n".join(["%d entries" % v.size] + note), transform=ax.transAxes, ha="right", va="top", fontsize=11)
                    ax.tick_params(labelsize=11)
                    if kind != "constant":
                        ax.set_ylabel("Entries", fontsize=12)
                    summary.append("%-55s %8d %12.4g %12.4g %12.4g %8d  %s" % (name, v.size, f.min() if f.size else np.nan, f.max() if f.size else np.nan,
                                   f.mean() if f.size else np.nan, np.unique(f).size, ", ".join(note + (["also in %s" % old] if today else []))))
                fig.suptitle("%s (%s), branches %d-%d of %d.  %s.  Private work" % (table, title, start + 1, start + len(chunk), len(names), a.sample.replace("$t\\bar{t}$", "ttbar")), fontsize=16)
                fig.tight_layout(rect=(0, 0, 1, 0.97))
                pdf.savefig(fig)
                plt.close(fig)
    head = ("%d branches; %d constant on this sample; %d contain NaN; %d also exist in today's gen tables\n"
            % (totals["branches"], totals["constant"], totals["nan"], totals["in_today"]))
    out = os.path.splitext(a.out)[0] + "_summary.txt"
    open(out, "w").write(head + "\n".join(summary) + "\n")
    print(head.strip())
    print("wrote", a.out, "and", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
