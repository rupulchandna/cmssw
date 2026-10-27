#!/usr/bin/env python3
"""CMS-style plots of the pat::GenJet NanoAOD tables.

    python3 make_plots.py NANO.root OUTDIR [--label "Private work (CMS simulation)"] [--lumi-text "ttbar, 13.6 TeV"]

Reads the new tables (PatGenJet, PatGenJetAK8, PatSubGenJetAK8) and, where present, today's gen tables
(GenJet, GenJetAK8, SubGenJetAK8) from a NanoAOD file. Writes PNG and PDF files to OUTDIR:
  - closure plots: the same quantity from the new and the existing table, with a ratio panel (should be 1);
  - new information on generator jets: soft-drop mass, N-subjettiness ratios, energy correlators,
    generator energy fractions, flavour composition, constituent counts;
  - truth taggers: fraction of AK4 jets with btagDeepFlavB == 1 per hadron flavour (1 for b, 0 otherwise)
    and the AK8 truth-class composition (top, W, other resonance, QCD) versus pt.
The label is "Private work", not "CMS Preliminary": these are not approved CMS results.
"""
import argparse, os, sys
import numpy as np
import uproot
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import mplhep as hep

hep.style.use("CMS")
# Fixed series colours (validated categorical palette, light background): new table, existing table, third series.
C_NEW, C_OLD, C_3, C_4 = "#2a78d6", "#eb6834", "#1baf7a", "#eda100"


def flat(arr):
    return np.asarray(arr).ravel() if not hasattr(arr, "layout") else np.asarray(arr.layout.content if hasattr(arr.layout, "content") else arr).ravel()


def get(tree, name):
    if name not in tree.keys():
        return None
    a = tree[name].array(library="ak")
    import awkward as ak
    return ak.to_numpy(ak.flatten(a, axis=None))


def label(ax, opt, loc=0):
    hep.cms.label(opt.label, data=False, rlabel=opt.lumi_text, loc=loc, ax=ax, fontsize=20)


def save(fig, opt, name):
    for ext in ("png", "pdf"):
        fig.savefig(os.path.join(opt.outdir, "%s.%s" % (name, ext)), dpi=150 if ext == "png" else None)
    plt.close(fig)
    print("wrote", name)


def closure(tree, opt, new, old, bins, xlabel, name, logy=False):
    a, b = get(tree, new), get(tree, old)
    if a is None or b is None:
        print("skip %s: %s or %s missing" % (name, new, old))
        return
    fig, (ax, rx) = plt.subplots(2, 1, figsize=(10, 10), gridspec_kw={"height_ratios": [3, 1]}, sharex=True)
    ha, _ = np.histogram(a, bins)
    hb, _ = np.histogram(b, bins)
    hep.histplot(hb, bins, ax=ax, label="Existing table (%s)" % old.split("_")[0], color=C_OLD, lw=2)
    hep.histplot(ha, bins, ax=ax, label="pat::GenJet table (%s)" % new.split("_")[0], color=C_NEW, lw=2, ls="--")
    ax.set_ylabel("Jets / bin")
    if logy:
        ax.set_yscale("log")
    ax.legend(loc="upper right", fontsize=16, title=opt.sample, title_fontsize=16)
    ax.set_ylim(top=ax.get_ylim()[1] * (8 if logy else 1.4))
    label(ax, opt)
    with np.errstate(divide="ignore", invalid="ignore"):
        r = np.where(hb > 0, ha / hb, np.nan)
    centres = 0.5 * (bins[1:] + bins[:-1])
    rx.plot(centres, r, "o", color=C_NEW, ms=6)
    rx.axhline(1, color="#898781", lw=1)
    rx.set_ylim(0.9, 1.1)
    rx.set_ylabel("New / existing")
    rx.set_xlabel(xlabel)
    same = len(a) == len(b) and np.array_equal(a, b)
    rx.text(0.02, 0.85, "identical, jet by jet" if same else "max |diff| %.3g" % (np.max(np.abs(a - b)) if len(a) == len(b) else np.nan),
            transform=rx.transAxes, fontsize=16)
    save(fig, opt, name)


def single(tree, opt, series, bins, xlabel, name, density=False, logy=False):
    fig, ax = plt.subplots(figsize=(10, 8))
    drawn = 0
    for col, lab, color, sel in series:
        v = get(tree, col)
        if v is None:
            print("skip %s in %s" % (col, name))
            continue
        if sel is not None:
            v = sel(tree, v)
        v = v[np.isfinite(v)]
        h, _ = np.histogram(v, bins)
        if density and h.sum() > 0:
            h = h / h.sum()
        hep.histplot(h, bins, ax=ax, label="%s (%d jets)" % (lab, len(v)), color=color, lw=2)
        drawn += 1
    if not drawn:
        plt.close(fig)
        return
    ax.set_xlabel(xlabel)
    ax.set_ylabel("Fraction of jets / bin" if density else "Jets / bin")
    if logy:
        ax.set_yscale("log")
    ax.legend(loc="upper right", fontsize=16, title=opt.sample, title_fontsize=16)
    ax.set_ylim(top=ax.get_ylim()[1] * (30 if logy else 1.45))
    label(ax, opt)
    save(fig, opt, name)


def ratio_col(num, den):
    def f(tree, v):
        n, d = get(tree, num), get(tree, den)
        with np.errstate(divide="ignore", invalid="ignore"):
            return np.where(d > 0, n / d, np.nan)
    return f


def with_softdrop(tree, v):
    m = get(tree, "PatGenJetAK8_msoftdrop")
    return v[m > 0] if m is not None and len(m) == len(v) else v


def flavour_bars(tree, opt):
    hf = get(tree, "PatGenJet_hadronFlavour")
    pt = get(tree, "PatGenJet_pt")
    if hf is None:
        return
    bins = np.array([20, 30, 50, 80, 120, 200, 400])
    fig, ax = plt.subplots(figsize=(10, 8))
    cats = [(5, "b (hadronFlavour 5)", C_NEW), (4, "c (hadronFlavour 4)", C_OLD), (0, "light (hadronFlavour 0)", C_3)]
    tot, _ = np.histogram(pt, bins)
    bottom = np.zeros(len(bins) - 1)
    for code, lab, color in cats:
        h, _ = np.histogram(pt[hf == code], bins)
        frac = np.where(tot > 0, h / np.maximum(tot, 1), 0)
        ax.bar(bins[:-1], frac, width=np.diff(bins), bottom=bottom, align="edge", color=color, label=lab, edgecolor="#fcfcfb", linewidth=2)
        bottom += frac
    ax.set_xscale("log")
    ax.set_xlim(bins[0], bins[-1])
    ax.set_xticks(bins)
    ax.set_xticklabels([str(b) for b in bins])
    ax.minorticks_off()
    ax.set_ylim(0, 1.45)
    ax.set_xlabel("AK4 generator jet $p_T$ [GeV]")
    ax.set_ylabel("Fraction of jets")
    ax.legend(loc="upper right", fontsize=16, ncol=1, title=opt.sample, title_fontsize=16)
    label(ax, opt)
    save(fig, opt, "ak4_flavour_composition")


def truth_btag(tree, opt):
    """btagDeepFlavB is the truth b label, so the fraction with value 1 is 1 for b jets and 0 otherwise."""
    hf, b = get(tree, "PatGenJet_hadronFlavour"), get(tree, "PatGenJet_btagDeepFlavB")
    if hf is None or b is None:
        print("skip truth_ak4_btag: columns missing")
        return
    cats = [(0, "light (0)"), (4, "c (4)"), (5, "b (5)")]
    frac = [np.mean(b[hf == c] == 1) if np.any(hf == c) else 0 for c, _ in cats]
    n = [int(np.sum(hf == c)) for c, _ in cats]
    fig, ax = plt.subplots(figsize=(10, 8))
    ax.bar(range(3), frac, color=C_NEW, width=0.6, edgecolor="#fcfcfb", linewidth=2)
    for i, (f, k) in enumerate(zip(frac, n)):
        ax.text(i, f + 0.03, "%.3f (%d jets)" % (f, k), ha="center", fontsize=16)
    ax.set_xticks(range(3))
    ax.set_xticklabels([l for _, l in cats])
    ax.set_ylim(0, 1.45)
    ax.set_xlabel("AK4 generator jet hadronFlavour")
    ax.set_ylabel("Fraction with btagDeepFlavB = 1")
    ax.text(0.03, 0.9, "Truth-based tagger values on gen jets", transform=ax.transAxes, fontsize=16)
    label(ax, opt)
    save(fig, opt, "truth_ak4_btag")


def truth_ak8_classes(tree, opt):
    """AK8 truth classes (GlobalParT labels) versus pt, as stacked fractions."""
    pt = get(tree, "PatGenJetAK8_pt")
    top, w, qcd = (get(tree, "PatGenJetAK8_globalParT3_" + c) for c in ("withMassTopvsQCD", "withMassWvsQCD", "QCD"))
    if pt is None or top is None or w is None or qcd is None:
        print("skip truth_ak8_classes: columns missing")
        return
    other = (top != 1) & (w != 1) & (qcd != 1)
    bins = np.array([100, 150, 200, 300, 400, 600, 1000])
    tot, _ = np.histogram(pt, bins)
    fig, ax = plt.subplots(figsize=(10, 8))
    bottom = np.zeros(len(bins) - 1)
    for mask, lab, color in [(top == 1, "Top (any decay class)", C_NEW), (w == 1, r"W $\to$ qq", C_OLD),
                             (other, "Other resonance class", C_4), (qcd == 1, "QCD", C_3)]:
        h, _ = np.histogram(pt[mask], bins)
        frac = np.where(tot > 0, h / np.maximum(tot, 1), 0)
        ax.bar(bins[:-1], frac, width=np.diff(bins), bottom=bottom, align="edge", color=color, label=lab, edgecolor="#fcfcfb", linewidth=2)
        bottom += frac
    ax.set_xscale("log")
    ax.set_xlim(bins[0], bins[-1])
    ax.set_xticks(bins)
    ax.set_xticklabels([str(b) for b in bins])
    ax.minorticks_off()
    ax.set_ylim(0, 1.45)
    ax.set_xlabel("AK8 generator jet $p_T$ [GeV]")
    ax.set_ylabel("Fraction of jets (truth class)")
    ax.legend(loc="upper right", fontsize=16, ncol=2, title=opt.sample, title_fontsize=16)
    label(ax, opt)
    save(fig, opt, "truth_ak8_classes")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("nano")
    ap.add_argument("outdir")
    ap.add_argument("--label", default="Private work")
    ap.add_argument("--lumi-text", default="13.6 TeV")
    ap.add_argument("--sample", default=r"$t\bar{t}$, generator level")
    opt = ap.parse_args()
    if not os.path.isfile(opt.nano):
        sys.exit("error: %s not found. Give the path of a NanoAOD file that holds the PatGenJet tables." % opt.nano)
    os.makedirs(opt.outdir, exist_ok=True)
    t = uproot.open(opt.nano)["Events"]

    # Closure: the new tables reproduce today's generator-jet tables
    closure(t, opt, "PatGenJet_pt", "GenJet_pt", np.linspace(10, 410, 41), "AK4 generator jet $p_T$ [GeV]", "closure_ak4_pt", logy=True)
    closure(t, opt, "PatGenJet_eta", "GenJet_eta", np.linspace(-5, 5, 41), r"AK4 generator jet $\eta$", "closure_ak4_eta")
    closure(t, opt, "PatGenJetAK8_pt", "GenJetAK8_pt", np.linspace(100, 700, 31), "AK8 generator jet $p_T$ [GeV]", "closure_ak8_pt", logy=True)
    closure(t, opt, "PatGenJetAK8_mass", "GenJetAK8_mass", np.linspace(0, 300, 31), "AK8 generator jet mass [GeV]", "closure_ak8_mass")
    closure(t, opt, "PatSubGenJetAK8_pt", "SubGenJetAK8_pt", np.linspace(0, 400, 41), "AK8 soft-drop generator subjet $p_T$ [GeV]", "closure_subjet_pt", logy=True)

    # New on generator jets: substructure stored on the jet
    single(t, opt, [("PatGenJetAK8_mass", "Ungroomed mass", C_OLD, None), ("PatGenJetAK8_msoftdrop", "Soft-drop mass", C_NEW, with_softdrop)],
           np.linspace(0, 300, 31), "AK8 generator jet mass [GeV]", "ak8_softdrop_mass")
    single(t, opt, [("PatGenJetAK8_tau2", r"$\tau_{21}$", C_NEW, ratio_col("PatGenJetAK8_tau2", "PatGenJetAK8_tau1")),
                    ("PatGenJetAK8_tau3", r"$\tau_{32}$", C_OLD, ratio_col("PatGenJetAK8_tau3", "PatGenJetAK8_tau2"))],
           np.linspace(0, 1, 26), "N-subjettiness ratio", "ak8_nsubjettiness_ratios", density=True)
    single(t, opt, [("PatGenJetAK8_n2b1", r"$N_2^{(\beta=1)}$", C_NEW, with_softdrop), ("PatGenJetAK8_n3b1", r"$N_3^{(\beta=1)}$", C_OLD, with_softdrop)],
           np.linspace(0, 3, 31), "Energy-correlator ratio (soft-drop jets)", "ak8_energy_correlators", density=True)
    single(t, opt, [("PatGenJet_chHEF", "Charged hadrons", C_NEW, None), ("PatGenJet_neHEF", "Neutral hadrons", C_OLD, None),
                    ("PatGenJet_neEmEF", "Photons (neutral EM)", C_3, None), ("PatGenJet_chEmEF", "Electrons (charged EM)", C_4, None)],
           np.linspace(0, 1, 26), "Fraction of AK4 generator jet energy", "ak4_energy_fractions", density=True, logy=True)
    single(t, opt, [("PatGenJet_nConstituents", "AK4", C_NEW, None), ("PatGenJetAK8_nConstituents", "AK8", C_OLD, None)],
           np.linspace(0, 150, 31), "Number of constituents", "constituent_counts", density=True)
    flavour_bars(t, opt)
    truth_btag(t, opt)
    truth_ak8_classes(t, opt)
    return 0


if __name__ == "__main__":
    sys.exit(main())
