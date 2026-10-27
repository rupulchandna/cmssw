#!/usr/bin/env python3
"""V1 rows 1-7 and 9: pat::GenJet collections against today's MiniAOD collections, in ONE EDM file (FWLite).

  python3 v1_compare.py <miniaod.root> [--max-events N] [--report report.txt]

The file must come from a job with customisePatGenJetsMiniAOD(keepInOutput=True), daughterOrder 'key' on the three producers
(patGenJetsAK4, patGenJetsAK8, patGenJetsAK8SoftDropSubjets) and these extra products kept (V1_RUN.md does it):
  slimmedGenJets*, ak8GenJetsNoNuSoftDrop (all instances), *ForPatGenJets (value maps), patGenJets*.
Rows:
  1 patGenJetsAK4 vs slimmedGenJets: size; pt, eta, phi, mass, vertex, area, Specific bitwise; daughter (id, key) lists
  2 flavour of patGenJetsAK4 vs slimmedGenJetsFlavourInfos: flavours and the b, c, parton, lepton reference lists
  3 packedPatGenJetsAK8 vs slimmedGenJetsAK8 (as 1)
  4 packedPatGenJetsAK8:SubJets vs slimmedGenJetsAK8SoftDropSubJets: p4, vertex, daughters bitwise
  5 subjet links of the fat jets
  6 user floats against the value maps (bitwise)
  7 self links (backRef, ref, genJet())
  9 groomedMass('SoftDropPuppi') against the mass of the matched ak8GenJetsNoNuSoftDrop jet, relative 1e-6, or absolute 1e-5 GeV for light groomed jets
Row 8 (NANO) is v1_compare_nano.py.
Known deviations are counted and printed separately, they do not make the result DIFF:
  K1 a constituent without packed partner: the reference writes a null Ptr, this package drops it
  K2 subjet Specific: filled here, zero in slimmedGenJetsAK8SoftDropSubJets
  K3 row 9: the largest relative mass difference (tolerance relative 1e-6, or absolute 1e-5 GeV for light groomed jets; sum of subjets against fastjet)
Exit code 0: no DIFF.   Needs cmsenv.
"""
import argparse
import math
import sys
from collections import Counter, defaultdict

import ROOT

ROOT.gROOT.SetBatch(True)
ROOT.gSystem.Load("libFWCoreFWLite.so")
ROOT.FWLiteEnabler.enable()
ROOT.gSystem.Load("libPhysicsToolsPatGenJet.so")
import cppyy
from DataFormats.FWLite import Events, Handle

TOL_GROOMED = 1e-6
# Absolute floor in GeV: for a light groomed jet, m = sqrt(E^2 - p^2) loses relative precision when E is large
# (sum of subjet four-vectors against fastjet's own sum), so very small masses get an absolute tolerance too.
TOL_GROOMED_ABS = 1e-5
SPECIFIC = ("emEnergy", "hadEnergy", "invisibleEnergy", "auxiliaryEnergy")

# userFloat label -> (value-map module, instance, key of the map)
FAT_FLOATS = {"NjettinessAK8Puppi:tau%d" % i: ("NjettinessAK8PuppiForPatGenJets", "tau%d" % i, "orig") for i in (1, 2, 3, 4)}
FAT_FLOATS["ak8PFJetsPuppiSoftDropMass"] = ("ak8PFJetsPuppiSoftDropMassForPatGenJets", "", "orig")
GROOMED_FLOATS = {"nb%dAK8PuppiSoftDrop:ecfN%d" % (b, n): ("nb%dAK8PuppiSoftDropForPatGenJets" % b, "ecfN%d" % n, "groomed")
                  for b in (1, 2) for n in (2, 3)}
SUB_FLOATS = {"NjettinessAK8Subjets:tau%d" % i: ("NjettinessAK8SubjetsForPatGenJets", "tau%d" % i, "orig") for i in (1, 2, 3, 4)}
SUB_FLOATS.update({"nb%dAK8PuppiSoftDropSubjets:ecfN%d" % (b, n): ("nb%dAK8PuppiSoftDropSubjetsForPatGenJets" % b, "ecfN%d" % n, "orig")
                   for b in (1, 2) for n in (2, 3)})


class Report:
    def __init__(self):
        self.checked = Counter()
        self.diffs = defaultdict(list)
        self.known = Counter()
        self.notes = []

    def ok(self, row, n=1):
        self.checked[row] += n

    def diff(self, row, msg):
        self.checked[row] += 1
        self.diffs[row].append(msg)


R = Report()


def dau_list(j):
    # the constituents are dereferenced by pat::Jet-like classes: packedGenParticles must be in the file (MiniAOD keeps it)
    return [(p.id().id(), p.key(), p.isNull()) for p in j.daughterPtrVector()]


def compare_jets(row, mine, ref, what, ev, specific_must_match=True, tag=""):
    """p4, vertex, area, Specific, daughters of two jets. Returns True if no DIFF was recorded."""
    good = True
    pre = "%s event %d jet %d: " % (what, ev, tag)
    for name in ("pt", "eta", "phi", "mass", "vx", "vy", "vz", "jetArea"):
        a, b = getattr(mine, name)(), getattr(ref, name)()
        if a != b:
            R.diff(row, pre + "%s %r != %r" % (name, a, b))
            good = False
    nonzero_ref = any(getattr(ref, n)() != 0 for n in SPECIFIC)
    for name in SPECIFIC:
        a, b = getattr(mine, name)(), getattr(ref, name)()
        if specific_must_match:
            if a != b:
                R.diff(row, pre + "%s %r != %r" % (name, a, b))
                good = False
        elif a != b and not nonzero_ref:
            R.known["K2 subjet Specific filled here, zero in the reference (values)"] += 1
    da, db = dau_list(mine), dau_list(ref)
    nulls = [d for d in db if d[2]]
    if nulls:
        R.known["K1 reference constituents with a null Ptr (dropped here)"] += len(nulls)
        R.known["K1 jets with a null constituent in the reference"] += 1
    db = [d for d in db if not d[2]]
    if [d[:2] for d in da] != [d[:2] for d in db]:
        if sorted(d[:2] for d in da) == sorted(d[:2] for d in db):
            R.diff(row, pre + "daughters: same set, different order (is daughterOrder 'key'?)")
        else:
            R.diff(row, pre + "daughters differ: %d here, %d in the reference (non-null)" % (len(da), len(db)))
        good = False
    return good


def get(event, handle, label):
    event.getByLabel(label, handle)
    return handle.product() if handle.isValid() else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--max-events", type=int, default=-1)
    ap.add_argument("--report", default="")
    a = ap.parse_args()

    hp = {k: Handle("std::vector<pat::GenJet>") for k in ("ak4", "fat", "sub", "ak8raw")}
    hr = {k: Handle("std::vector<reco::GenJet>") for k in ("ak4", "fat", "sub", "subsrc")}
    h_groomed = Handle("std::vector<reco::BasicJet>")
    h_flav = Handle("reco::io_v1::JetFlavourInfoMatchingCollection")
    h_vm = Handle("edm::ValueMap<float>")

    nev = 0
    n_with = Counter()
    ids = defaultdict(set)
    missing = Counter()
    for ievent, ev in enumerate(Events(a.file)):
        if 0 <= a.max_events <= ievent:
            break
        nev += 1
        ak4 = get(ev, hp["ak4"], ("patGenJetsAK4",))
        fat = get(ev, hp["fat"], ("packedPatGenJetsAK8",))
        sub = get(ev, hp["sub"], ("packedPatGenJetsAK8", "SubJets"))
        s_ak4 = get(ev, hr["ak4"], ("slimmedGenJets",))
        s_fat = get(ev, hr["fat"], ("slimmedGenJetsAK8",))
        s_sub = get(ev, hr["sub"], ("slimmedGenJetsAK8SoftDropSubJets",))
        flav = get(ev, h_flav, ("slimmedGenJetsFlavourInfos",))
        groomed = get(ev, h_groomed, ("ak8GenJetsNoNuSoftDrop",))
        groomed_sub = get(ev, hr["subsrc"], ("ak8GenJetsNoNuSoftDrop", "SubJets"))
        for name, obj in (("patGenJetsAK4", ak4), ("packedPatGenJetsAK8", fat), ("packedPatGenJetsAK8:SubJets", sub),
                          ("slimmedGenJets", s_ak4), ("slimmedGenJetsAK8", s_fat), ("slimmedGenJetsAK8SoftDropSubJets", s_sub),
                          ("slimmedGenJetsFlavourInfos", flav), ("ak8GenJetsNoNuSoftDrop", groomed),
                          ("ak8GenJetsNoNuSoftDrop:SubJets", groomed_sub)):
            if obj is None:
                missing[name] += 1
        if missing:
            continue

        # ---------------- rows 1 and 3
        for row, mine, ref, what in ((1, ak4, s_ak4, "AK4"), (3, fat, s_fat, "AK8")):
            if len(mine) != len(ref):
                R.diff(row, "%s event %d: size %d != %d" % (what, ievent, len(mine), len(ref)))
                continue
            R.ok(row)
            n_with[row] += len(mine)
            for i in range(len(mine)):
                if compare_jets(row, mine[i], ref[i], what, ievent, True, i):
                    R.ok(row)
        # ---------------- row 2
        # entry k of the association vector belongs to slimmed jet flav.key(k); use key()/value(), the pair view of operator[] gives wrong values here
        by_key = {flav.key(k).key(): k for k in range(len(flav))}
        if len(flav) != len(s_ak4):
            R.diff(2, "event %d: %d flavour entries for %d slimmed jets" % (ievent, len(flav), len(s_ak4)))
        for i in range(len(ak4)):
            if i not in by_key:
                R.diff(2, "event %d jet %d: no entry in slimmedGenJetsFlavourInfos" % (ievent, i))
                continue
            fa, fb = ak4[i].jetFlavourInfo(), flav.value(by_key[i])
            bad = []
            if fa.getHadronFlavour() != fb.getHadronFlavour():
                bad.append("hadronFlavour %d != %d" % (fa.getHadronFlavour(), fb.getHadronFlavour()))
            if fa.getPartonFlavour() != fb.getPartonFlavour():
                bad.append("partonFlavour %d != %d" % (fa.getPartonFlavour(), fb.getPartonFlavour()))
            for name in ("getbHadrons", "getcHadrons", "getPartons", "getLeptons"):
                la = [(r.id().id(), r.key()) for r in getattr(fa, name)()]
                lb = [(r.id().id(), r.key()) for r in getattr(fb, name)()]
                if la != lb:
                    bad.append("%s %s != %s" % (name, la, lb))
            if bad:
                R.diff(2, "event %d jet %d: %s" % (ievent, i, "; ".join(bad)))
            else:
                R.ok(2)
        # ---------------- row 4
        if len(sub) != len(s_sub):
            R.diff(4, "event %d: %d subjets != %d slimmed subjets" % (ievent, len(sub), len(s_sub)))
        else:
            R.ok(4)
            for k in range(len(sub)):
                if compare_jets(4, sub[k], s_sub[k], "subjet", ievent, False, k):
                    R.ok(4)
                if any(getattr(sub[k], n)() != 0 for n in SPECIFIC):
                    R.known["K2 subjets with a filled Specific (reference: zero)"] += 1

        # ---------------- groomed jet of each fat jet (first within 0.8, as the packer) and the expected subjet offsets
        offset, base = 0, []
        for g in range(len(groomed)):
            base.append(offset)
            offset += groomed[g].numberOfDaughters()
        n_sub_expected = offset

        def groomed_index(jet):
            for g in range(len(groomed)):
                dphi = abs(jet.phi() - groomed[g].phi())
                dphi = dphi if dphi <= math.pi else 2 * math.pi - dphi
                if math.hypot(jet.eta() - groomed[g].eta(), dphi) < 0.8:
                    return g
            return -1

        if n_sub_expected != len(sub):
            R.notes.append("event %d: subjets expected from the groomed daughters %d, packed SubJets %d" % (ievent, n_sub_expected, len(sub)))

        # ---------------- rows 5, 6 (fat), 9
        for i in range(len(fat)):
            j = fat[i]
            g = groomed_index(j)
            links = list(j.subjets("SoftDropPuppi")) if j.hasSubjets("SoftDropPuppi") else []
            keys = [p.key() for p in links]
            if g < 0:
                (R.ok if not keys else (lambda r: R.diff(r, "event %d fat %d: no groomed jet but %d subjets" % (ievent, i, len(keys)))))(5)
                if j.hasUserFloat("nb1AK8PuppiSoftDrop:ecfN2"):
                    R.diff(6, "event %d fat %d: groomed ECF userFloat without a groomed jet" % (ievent, i))
                n_with["fat jets without groomed jet"] += 1
                if j.groomedMass("SoftDropPuppi") != -1:
                    R.diff(9, "event %d fat %d: groomedMass %r without subjets, expected -1" % (ievent, i, j.groomedMass("SoftDropPuppi")))
                else:
                    R.ok(9)
            else:
                n_with["fat jets with groomed jet"] += 1
                gj = groomed[g]
                want = []
                for d in range(gj.numberOfDaughters()):
                    want.append(gj.daughterPtr(d).key())
                bad = []
                if len(keys) != len(want):
                    bad.append("%d subjet links, matched groomed jet has %d daughters" % (len(keys), len(want)))
                else:
                    if keys != list(range(base[g], base[g] + len(want))):
                        bad.append("keys %s, expected %s (groomed jet %d)" % (keys, list(range(base[g], base[g] + len(want))), g))
                    for p, d in zip(links, want):
                        if p.key() >= len(sub):
                            bad.append("key %d out of range" % p.key())
                            continue
                        a_, b_ = sub[p.key()], groomed_sub[d]
                        if (a_.pt(), a_.eta(), a_.phi(), a_.mass()) != (b_.pt(), b_.eta(), b_.phi(), b_.mass()):
                            bad.append("subjet at key %d is not the groomed daughter %d (p4)" % (p.key(), d))
                    ids_here = {p.id().id() for p in links}
                    if len(ids_here) > 1:
                        bad.append("links with different product IDs %s" % sorted(ids_here))
                    ids["subjet link product id"] |= ids_here
                if bad:
                    R.diff(5, "event %d fat %d: %s" % (ievent, i, "; ".join(bad)))
                else:
                    R.ok(5)
                # row 9
                gm, ref_mass = j.groomedMass("SoftDropPuppi"), gj.mass()
                rel = abs(gm - ref_mass) / max(abs(ref_mass), 1e-30)
                R.known["K3 max relative groomedMass difference (x1e-9)"] = max(R.known["K3 max relative groomedMass difference (x1e-9)"], int(rel * 1e9))
                n_with["row 9 compared"] += 1
                if rel < TOL_GROOMED or abs(gm - ref_mass) < TOL_GROOMED_ABS:
                    R.ok(9)
                else:
                    R.diff(9, "event %d fat %d: groomedMass %r vs %r, rel %.3g" % (ievent, i, gm, ref_mass, rel))
            # row 6, fat and groomed user floats
            okey = j.originalObjectRef().key()
            for label, (mod, inst, how) in list(FAT_FLOATS.items()) + list(GROOMED_FLOATS.items()):
                if how == "groomed" and g < 0:
                    continue
                key = okey if how == "orig" else g
                vm = get(ev, h_vm, (mod, inst))
                if vm is None:
                    missing[mod + ":" + inst] += 1
                    continue
                if not j.hasUserFloat(label):
                    R.diff(6, "event %d fat %d: no userFloat %s" % (ievent, i, label))
                    continue
                v, w = j.userFloat(label), vm.get(key)
                if v != w:
                    R.diff(6, "event %d fat %d: %s %r != value map %r (key %d)" % (ievent, i, label, v, w, key))
                else:
                    R.ok(6)
        # ---------------- row 6, subjets
        for k in range(len(sub)):
            okey = sub[k].originalObjectRef().key()
            for label, (mod, inst, how) in SUB_FLOATS.items():
                vm = get(ev, h_vm, (mod, inst))
                if vm is None:
                    missing[mod + ":" + inst] += 1
                    continue
                if not sub[k].hasUserFloat(label):
                    R.diff(6, "event %d subjet %d: no userFloat %s" % (ievent, k, label))
                    continue
                v, w = sub[k].userFloat(label), vm.get(okey)
                if v != w:
                    R.diff(6, "event %d subjet %d: %s %r != value map %r" % (ievent, k, label, v, w))
                else:
                    R.ok(6)

        # ---------------- row 7
        for name, coll in (("patGenJetsAK4", ak4), ("packedPatGenJetsAK8", fat), ("packedPatGenJetsAK8:SubJets", sub)):
            for i in range(len(coll)):
                j = coll[i]
                br, rf = j.genJetFwdRef().backRef(), j.genJetFwdRef().ref()
                bad = []
                if br.isNull() or rf.isNull():
                    bad.append("null link")
                else:
                    if br.key() != i or rf.key() != i:
                        bad.append("keys backRef %d ref %d, expected %d" % (br.key(), rf.key(), i))
                    if br.id().id() != rf.id().id():
                        bad.append("ids differ: backRef %d ref %d" % (br.id().id(), rf.id().id()))
                    ids["self link id " + name].add(br.id().id())
                if cppyy.addressof(j.genJet()) != cppyy.addressof(j):
                    bad.append("genJet() is not the jet itself")
                if bad:
                    R.diff(7, "%s event %d jet %d: %s" % (name, ievent, i, "; ".join(bad)))
                else:
                    R.ok(7)

    # ---------------- report
    lines = []
    lines.append("V1 report (rows 1-7, 9): %s, %d events" % (a.file, nev))
    if missing:
        lines.append("MISSING INPUT (not in the file, rows not checked): %s" % dict(missing))
    for k in ("ak4", "fat", "sub"):
        pass
    names = {1: "patGenJetsAK4 vs slimmedGenJets", 2: "flavour vs slimmedGenJetsFlavourInfos", 3: "packed AK8 vs slimmedGenJetsAK8",
             4: "packed SubJets vs slimmedGenJetsAK8SoftDropSubJets", 5: "subjet links", 6: "user floats vs value maps",
             7: "self links", 9: "groomedMass vs groomed jet mass"}
    failed = bool(missing)
    for row in (1, 2, 3, 4, 5, 6, 7, 9):
        n, d = R.checked[row], len(R.diffs[row])
        status = "PASS" if (d == 0 and n > 0) else ("DIFF" if d else "NOT CHECKED (no data)")
        if status != "PASS":
            failed = True
        lines.append("row %d %-52s %-6s checks %d, DIFF %d" % (row, names[row], status, n, d))
        for m in R.diffs[row][:5]:
            lines.append("      " + m)
        if d > 5:
            lines.append("      ... %d more" % (d - 5))
    lines.append("jets/subjets looked at: " + ", ".join("%s %d" % (k, v) for k, v in sorted(n_with.items(), key=str)))
    lines.append("")
    lines.append("KNOWN DEVIATIONS (reported, not failed)")
    for k, v in sorted(R.known.items()):
        lines.append("  %s: %d" % (k, v))
    if not R.known:
        lines.append("  none seen")
    for n in R.notes[:5]:
        lines.append("  note: " + n)
    lines.append("")
    lines.append("RESULT: " + ("DIFF (see above)" if failed else "PASS"))
    text = "\n".join(lines)
    print(text)
    if a.report:
        open(a.report, "w").write(text + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
