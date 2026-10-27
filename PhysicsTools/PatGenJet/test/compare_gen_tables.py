#!/usr/bin/env python3
"""C3: the PatGen* tables against today's gen tables, on the NANO file of run_patgenjets_cfg.py (sample of gen_ttbar20_cfg.py).

Usage:  python3 compare_gen_tables.py ttbar20_nano.root

  PatGenJet        vs GenJet         same rows; pt, eta, phi, mass bitwise; partonFlavour, hadronFlavour, nBHadrons, nCHadrons equal
  PatGenJetAK8     vs GenJetAK8      same;  flavour differences are REPORTED, not failed (flavour clustered on AOD versus packed
                                     constituents in the ghost clustering)
  PatSubGenJetAK8  vs SubGenJetAK8   same rows; p4 bitwise
  index columns    genJetIdx, genJetAK8Idx, subGenJetAK8Idx == row; subJetIdx1/2 index PatSubGenJetAK8 rows whose subjets lie
                   inside the fat jet cone (dR < 0.8)
  the added/changed columns have sane values: genEmEF+genHadEF+genInvisibleEF+genAuxiliaryEF = 1, the real PF fractions are not the -1 of the isPFJet() guard, no NaN in a column that must not have one
Exit code 0 if the failing checks all hold.
"""
import sys

import awkward as ak
import numpy as np
import uproot

fail = []
note = []


def check(cond, msg):
    if not cond:
        fail.append(msg)


t = uproot.open(sys.argv[1])["Events"]


def col(name):
    return t[name].array(library="ak")


def same_rows(a, b, cols, label, exact=True):
    check(ak.all(col("n" + a) == col("n" + b)), "%s: row counts differ" % label)
    nrow = int(ak.sum(col("n" + a)))
    for c in cols:
        x, y = col("%s_%s" % (a, c)), col("%s_%s" % (b, c))
        if not ak.all(ak.num(x) == ak.num(y)):
            fail.append("%s.%s: shape" % (label, c))
            continue
        nd = int(ak.sum(ak.flatten(x != y)))
        if nd:
            (note if label.endswith("AK8") and c.endswith(("Flavour", "Hadrons")) else fail).append(
                "%s.%s: %d of %d rows differ" % (label, c, nd, nrow))
    return nrow


p4 = ["pt", "eta", "phi", "mass"]
fl = ["partonFlavour", "hadronFlavour", "nBHadrons", "nCHadrons"]
n4 = same_rows("PatGenJet", "GenJet", p4 + fl, "PatGenJet vs GenJet")
n8 = same_rows("PatGenJetAK8", "GenJetAK8", p4 + fl, "PatGenJetAK8 vs GenJetAK8")
ns = same_rows("PatSubGenJetAK8", "SubGenJetAK8", p4, "PatSubGenJetAK8 vs SubGenJetAK8")
print("rows compared: PatGenJet %d, PatGenJetAK8 %d, PatSubGenJetAK8 %d" % (n4, n8, ns))
check(n4 > 0 and n8 > 0 and ns > 0, "an empty table: nothing was compared")

# index columns equal the row
for tab, c in (("PatGenJet", "genJetIdx"), ("PatGenJetAK8", "genJetAK8Idx"), ("PatSubGenJetAK8", "subGenJetAK8Idx")):
    idx = col("%s_%s" % (tab, c))
    row = ak.local_index(idx)
    check(ak.all(idx == row), "%s.%s != row" % (tab, c))

# subjet links: indices into the PatSubGenJetAK8 rows of the same event, inside the fat jet
sub_eta, sub_phi = col("PatSubGenJetAK8_eta"), col("PatSubGenJetAK8_phi")
fat_eta, fat_phi = col("PatGenJetAK8_eta"), col("PatGenJetAK8_phi")
i1, i2 = col("PatGenJetAK8_subJetIdx1"), col("PatGenJetAK8_subJetIdx2")
nlinked = 0
for ev in range(len(fat_eta)):
    for j in range(len(fat_eta[ev])):
        for i in (i1[ev][j], i2[ev][j]):
            if i < 0:
                continue
            nlinked += 1
            check(i < len(sub_eta[ev]), "event %d fat %d: subjet index %d out of range" % (ev, j, i))
            if i < len(sub_eta[ev]):
                dphi = (sub_phi[ev][i] - fat_phi[ev][j] + np.pi) % (2 * np.pi) - np.pi
                check(np.hypot(sub_eta[ev][i] - fat_eta[ev][j], dphi) < 0.8, "event %d fat %d: subjet %d outside the cone" % (ev, j, i))
print("fat-jet subjet links checked: %d" % nlinked)
check(nlinked > 0, "no fat jet has a subjet link")

# gen energy fractions sum to 1 (zero-energy jets are excluded by the cuts)
for tab in ("PatGenJet", "PatGenJetAK8", "PatSubGenJetAK8"):
    s = sum(col("%s_%s" % (tab, c)) for c in ("genEmEF", "genHadEF", "genInvisibleEF", "genAuxiliaryEF"))
    check(ak.all(abs(s - 1) < 1e-3), "%s: gen energy fractions do not sum to 1 (min %g max %g)" % (tab, ak.min(s), ak.max(s)))

# the PF-named fractions of the fat jets are the generator values, not the -1 of the isPFJet() guard
for c in ("chHEF", "neHEF", "chEmEF", "neEmEF", "muEF"):
    v = col("PatGenJetAK8_" + c)
    check(ak.all(v >= 0) and ak.all(v <= 1.001), "PatGenJetAK8.%s outside [0,1]" % c)
for c in ("chMultiplicity", "neMultiplicity"):
    check(ak.all(col("PatGenJetAK8_" + c) >= 0), "PatGenJetAK8.%s < 0" % c)
check(ak.all(col("PatGenJetAK8_hfHEF") == -1), "PatGenJetAK8.hfHEF should stay at its guarded -1")
# the AK4 and the AK8 gen fractions agree with the PF-named ones (chHEF+neHEF = hadEF-like quantities are not identical; check chEmEF+neEmEF = genEmEF)
d = col("PatGenJetAK8_chEmEF") + col("PatGenJetAK8_neEmEF") - col("PatGenJetAK8_genEmEF")
check(ak.all(abs(d) < 1e-3), "PatGenJetAK8: chEmEF+neEmEF != genEmEF")

# M3: fat jets without a groomed match (table without the pt cut, PatGenJetAK8All): guarded ECFs, no exception, defaults
nomatch = col("PatGenJetAK8All_subJetIdx1") < 0
n_no = int(ak.sum(nomatch))
n_all = int(ak.sum(col("nPatGenJetAK8All")))
check(n_no > 0, "the test needs fat jets without a soft-drop match and has none")
check(ak.all(abs(col("PatGenJetAK8All_n2b1")[nomatch] + 99999.0) < 100), "unmatched fat jets: n2b1 is not the default")
check(ak.all(abs(col("PatGenJetAK8All_n3b1")[nomatch] + 99999.0) < 100), "unmatched fat jets: n3b1 is not the default")
check(ak.all(col("PatGenJetAK8All_msoftdrop")[nomatch] == -1.0), "unmatched fat jets: msoftdrop is not -1")
print("fat jets without a soft-drop match: %d of %d (n2b1 = n3b1 = -99999 (10-bit precision: -1e5), msoftdrop = -1 there)" % (n_no, n_all))
for c in ("tau1", "tau2", "tau3", "tau4", "mass", "pt"):
    check(not ak.any(np.isnan(ak.to_numpy(ak.flatten(col("PatGenJetAK8_" + c))))), "NaN in PatGenJetAK8_" + c)

# muon-subtraction columns read 0 and 0, 0; placeholders 0 (puIdDisc and the HF shape values)
mf = col("PatGenJet_muonSubtrFactor")
check(ak.all(abs(mf) < 1e-5), "PatGenJet.muonSubtrFactor != 0")
check(ak.all(abs(col("PatGenJet_muonSubtrDeltaEta")) < 1e-5) and ak.all(abs(col("PatGenJet_muonSubtrDeltaPhi")) < 1e-5), "muonSubtrDelta* != 0")
check(ak.all(col("PatGenJet_puIdDisc") == 0), "puIdDisc != 0")

for n in note:
    print("REPORTED (expected deviation):", n)
if fail:
    print("FAILED:")
    for f in fail:
        print("  ", f)
    sys.exit(1)
print("compare_gen_tables: OK")
