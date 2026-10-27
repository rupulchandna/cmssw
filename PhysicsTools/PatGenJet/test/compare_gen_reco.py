#!/usr/bin/env python3
"""Gen against reco: overlay every column that the reco jet tables and the pat::GenJet tables share, CMS style.

    python3 compare_gen_reco.py NANO.root OUTDIR [--sample TEXT]

NANO.root must hold both the reco tables (Jet, FatJet, SubJet) and the pat::GenJet tables (PatGenJet, PatGenJetAK8,
PatSubGenJetAK8), i.e. a NanoAOD made from a full simulation + reconstruction job with customisePatGenJetsNano.
Writes to OUTDIR:
  - gen_vs_reco_all.pdf: one overlay per shared column (reco and gen, each normalised to unit area), grouped by table;
  - a few single CMS-style figures (PNG and PDF): pt, eta, mass, soft-drop mass, tau21, N2, energy fractions,
    constituent counts, and the jet response pt(reco)/pt(gen) of reco jets matched through Jet_genJetIdx;
  - tagger plots: the reco b-tag score of reco jets split by the truth flavour of their matched gen jet
    (PatGenJet_btagDeepFlavB, truth-based), showing what the truth tagger stands for.
Gen tagger columns hold truth values (one-hot): they are drawn, but the comparison is about what the reco tagger does.
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
C_RECO, C_GEN, C_3, C_4 = "#eb6834", "#2a78d6", "#1baf7a", "#eda100"
PAIRS = [("Jet", "PatGenJet", "AK4"), ("FatJet", "PatGenJetAK8", "AK8"), ("SubJet", "PatSubGenJetAK8", "AK8 soft-drop subjets")]


def flat(t, name):
    return ak.to_numpy(ak.flatten(t[name].array(library="ak"), axis=None)).astype(float)


def bins_for(a, b, n=40):
    f = np.concatenate([a[np.isfinite(a)], b[np.isfinite(b)]])
    if f.size == 0:
        return None
    u = np.unique(f)
    if u.size == 1:
        return np.array([u[0] - 0.5, u[0] + 0.5])
    if u.size <= 30 and np.allclose(u, np.round(u)):
        return np.arange(u.min() - 0.5, u.max() + 1.5, 1.0)
    lo, hi = np.percentile(f, [0.5, 99.5])
    if lo == hi:
        lo, hi = f.min(), f.max() + 1e-9
    return np.linspace(lo, hi, n + 1)


def overlay(ax, a, b, bins, la, lb, density=True):
    for v, lab, col, ls in ((a, la, C_RECO, "-"), (b, lb, C_GEN, "--")):
        f = v[np.isfinite(v)]
        h, _ = np.histogram(np.clip(f, bins[0], bins[-1]), bins)
        if density and h.sum() > 0:
            h = h / h.sum()
        hep.histplot(h, bins, ax=ax, color=col, lw=2, ls=ls, label="%s (%d)" % (lab, f.size))


def single(opt, t, reco, gen, col, xlabel, name, bins, logy=False, sel_reco=None, sel_gen=None):
    if reco + "_" + col not in opt.keys or gen + "_" + col not in opt.keys:
        print("skip", name)
        return
    a, b = flat(t, reco + "_" + col), flat(t, gen + "_" + col)
    if sel_reco is not None:
        a = a[sel_reco(t, a)]
    if sel_gen is not None:
        b = b[sel_gen(t, b)]
    fig, ax = plt.subplots(figsize=(10, 8))
    overlay(ax, a, b, bins, "Reco (%s)" % reco, "Gen (%s)" % gen)
    ax.set_xlabel(xlabel)
    ax.set_ylabel("Fraction of jets / bin")
    if logy:
        ax.set_yscale("log")
    ax.set_ylim(top=ax.get_ylim()[1] * (20 if logy else 1.45))
    ax.legend(loc="upper right", fontsize=16, title=opt.sample, title_fontsize=16)
    hep.cms.label("Private work", data=False, rlabel="13.6 TeV", ax=ax, fontsize=20)
    for ext in ("png", "pdf"):
        fig.savefig(os.path.join(opt.outdir, "%s.%s" % (name, ext)), dpi=150 if ext == "png" else None)
    plt.close(fig)
    print("wrote", name)


def response(opt, t):
    if "Jet_genJetIdx" not in opt.keys:
        print("skip response: no Jet_genJetIdx")
        return
    idx = t["Jet_genJetIdx"].array()
    rpt = t["Jet_pt"].array()
    gpt = t["PatGenJet_pt"].array()
    ok = (idx >= 0) & (idx < ak.num(gpt))
    safe = ak.where(ok, idx, 0)
    matched_g = gpt[safe]
    resp = ak.to_numpy(ak.flatten((rpt / matched_g)[ok]))
    gm = ak.to_numpy(ak.flatten(matched_g[ok]))
    fig, ax = plt.subplots(figsize=(10, 8))
    bins = np.linspace(0, 2, 51)
    for lo, hi, col in ((20, 50, C_GEN), (50, 100, C_RECO), (100, 1e9, C_3)):
        s = (gm >= lo) & (gm < hi)
        h, _ = np.histogram(np.clip(resp[s], 0, 2), bins)
        if h.sum():
            h = h / h.sum()
        lab = "%d < gen $p_T$ < %d GeV" % (lo, hi) if hi < 1e8 else "gen $p_T$ > %d GeV" % lo
        hep.histplot(h, bins, ax=ax, color=col, lw=2, label="%s (%d jets)" % (lab, s.sum()))
    ax.set_xlabel(r"$p_T$(reco jet) / $p_T$(matched PatGenJet)")
    ax.set_ylabel("Fraction of jets / bin")
    ax.set_ylim(top=ax.get_ylim()[1] * 1.45)
    ax.legend(loc="upper right", fontsize=16, title=opt.sample + "; AK4, matched by Jet_genJetIdx", title_fontsize=15)
    hep.cms.label("Private work", data=False, rlabel="13.6 TeV", ax=ax, fontsize=20)
    for ext in ("png", "pdf"):
        fig.savefig(os.path.join(opt.outdir, "ak4_response.%s" % ext), dpi=150 if ext == "png" else None)
    plt.close(fig)
    print("wrote ak4_response")


def btag_by_truth(opt, t):
    need = ("Jet_genJetIdx", "Jet_btagDeepFlavB", "PatGenJet_hadronFlavour")
    if any(k not in opt.keys for k in need):
        print("skip btag plot")
        return
    idx = t["Jet_genJetIdx"].array()
    gfl = t["PatGenJet_hadronFlavour"].array()
    ok = (idx >= 0) & (idx < ak.num(gfl))
    fl = ak.to_numpy(ak.flatten(gfl[ak.where(ok, idx, 0)][ok]))
    tags = [k for k in ("Jet_btagDeepFlavB", "Jet_btagPNetB", "Jet_btagUParTAK4B") if k in opt.keys]
    for tag in tags:
        score = ak.to_numpy(ak.flatten(t[tag].array()[ok]))
        fig, ax = plt.subplots(figsize=(10, 8))
        bins = np.linspace(0, 1, 41)
        for code, lab, col in ((5, "truth b (PatGenJet hadronFlavour 5)", C_GEN), (4, "truth c (4)", C_RECO), (0, "truth light (0)", C_3)):
            s = fl == code
            h, _ = np.histogram(np.clip(score[s], 0, 1), bins)
            if h.sum():
                h = h / h.sum()
            hep.histplot(h, bins, ax=ax, color=col, lw=2, label="%s (%d jets)" % (lab, s.sum()))
        ax.set_yscale("log")
        ax.set_xlabel("Reco %s" % tag.replace("Jet_", ""))
        ax.set_ylabel("Fraction of jets / bin")
        ax.set_ylim(top=ax.get_ylim()[1] * 30)
        ax.legend(loc="upper center", fontsize=15, title=opt.sample + "; reco AK4 jets matched to PatGenJet", title_fontsize=14)
        hep.cms.label("Private work", data=False, rlabel="13.6 TeV", ax=ax, fontsize=20)
        name = "reco_%s_by_gen_truth" % tag.replace("Jet_", "")
        for ext in ("png", "pdf"):
            fig.savefig(os.path.join(opt.outdir, "%s.%s" % (name, ext)), dpi=150 if ext == "png" else None)
        plt.close(fig)
        print("wrote", name)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("nano")
    ap.add_argument("outdir")
    ap.add_argument("--sample", default=r"$t\bar{t}$, 13.6 TeV")
    opt = ap.parse_args()
    if not os.path.isfile(opt.nano):
        sys.exit("error: %s not found" % opt.nano)
    os.makedirs(opt.outdir, exist_ok=True)
    t = uproot.open(opt.nano)["Events"]
    opt.keys = set(t.keys())
    missing = [p for r, g, _ in PAIRS for p in (r, g) if not any(k.startswith(p + "_") for k in opt.keys)]
    if missing:
        sys.exit("error: tables missing in the file: %s (need a full reco NANO made with customisePatGenJetsNano)" % ", ".join(missing))

    # all shared columns, multi-page
    with PdfPages(os.path.join(opt.outdir, "gen_vs_reco_all.pdf")) as pdf:
        for reco, gen, title in PAIRS:
            cols = sorted(k.split("_", 1)[1] for k in opt.keys if k.startswith(reco + "_") and (gen + "_" + k.split("_", 1)[1]) in opt.keys)
            for start in range(0, len(cols), 12):
                chunk = cols[start:start + 12]
                fig, axes = plt.subplots(4, 3, figsize=(18, 18.4))
                for ax in axes.ravel()[len(chunk):]:
                    ax.axis("off")
                for ax, c in zip(axes.ravel(), chunk):
                    a, b = flat(t, reco + "_" + c), flat(t, gen + "_" + c)
                    bins = bins_for(a, b)
                    if bins is not None:
                        overlay(ax, a, b, bins, "Reco", "Gen")
                    ax.set_title(c, fontsize=14)
                    ax.tick_params(labelsize=11)
                    ax.legend(fontsize=11, loc="upper right")
                fig.suptitle("%s (reco) vs %s (gen), %s: columns %d-%d of %d. Normalised to unit area. Private work"
                             % (reco, gen, title, start + 1, start + len(chunk), len(cols)), fontsize=16)
                fig.tight_layout(rect=(0, 0, 1, 0.97))
                pdf.savefig(fig)
                plt.close(fig)
            print("%s vs %s: %d shared columns" % (reco, gen, len(cols)))

    # key single plots
    lin = lambda lo, hi, n=40: np.linspace(lo, hi, n + 1)
    single(opt, t, "Jet", "PatGenJet", "pt", "AK4 jet $p_T$ [GeV]", "gen_reco_ak4_pt", lin(15, 415), logy=True)
    single(opt, t, "Jet", "PatGenJet", "eta", r"AK4 jet $\eta$", "gen_reco_ak4_eta", lin(-5, 5))
    single(opt, t, "Jet", "PatGenJet", "mass", "AK4 jet mass [GeV]", "gen_reco_ak4_mass", lin(0, 60))
    single(opt, t, "Jet", "PatGenJet", "nConstituents", "AK4 jet constituents", "gen_reco_ak4_nconstituents", np.arange(-0.5, 80.5, 2))
    single(opt, t, "Jet", "PatGenJet", "chHEF", "AK4 charged-hadron energy fraction", "gen_reco_ak4_chHEF", lin(0, 1, 25))
    single(opt, t, "Jet", "PatGenJet", "neEmEF", "AK4 photon (neutral EM) energy fraction", "gen_reco_ak4_neEmEF", lin(0, 1, 25))
    single(opt, t, "FatJet", "PatGenJetAK8", "pt", "AK8 jet $p_T$ [GeV]", "gen_reco_ak8_pt", lin(150, 800, 26), logy=True)
    single(opt, t, "FatJet", "PatGenJetAK8", "msoftdrop", "AK8 soft-drop mass [GeV]", "gen_reco_ak8_msoftdrop", lin(0, 300, 30),
           sel_reco=lambda t, v: v > 0, sel_gen=lambda t, v: v > 0)
    for c, lab in (("tau1", r"AK8 $\tau_1$"), ("tau2", r"AK8 $\tau_2$"), ("tau3", r"AK8 $\tau_3$")):
        single(opt, t, "FatJet", "PatGenJetAK8", c, lab, "gen_reco_ak8_" + c, lin(0, 0.6, 30))
    single(opt, t, "FatJet", "PatGenJetAK8", "n2b1", r"AK8 $N_2^{(\beta=1)}$", "gen_reco_ak8_n2b1", lin(0, 0.6, 30),
           sel_reco=lambda t, v: v > 0, sel_gen=lambda t, v: v > 0)
    single(opt, t, "SubJet", "PatSubGenJetAK8", "pt", "AK8 subjet $p_T$ [GeV]", "gen_reco_subjet_pt", lin(0, 500, 25), logy=True)
    response(opt, t)
    btag_by_truth(opt, t)
    return 0


if __name__ == "__main__":
    sys.exit(main())
