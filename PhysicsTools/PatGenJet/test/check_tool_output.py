#!/usr/bin/env python3
"""C2: the collections made by addPatGenJetCollection (test_tool_cfg.py) equal the ones of patGenJets_cff, jet by jet.

Usage: python3 check_tool_output.py tool.root
  patGenJetsAK4Tool  == patGenJetsAK4                   p4, flavour, truth taggers, daughters, self links
  packedPatGenJetsAK8PuppiTool == packedPatGenJetsAK8    p4, flavour, daughters, user floats (tau1-4, soft-drop mass, ECFs),
                                                         subjets(SoftDropPuppi) keys, groomed mass
  packedPatGenJetsAK8PuppiTool:SubJets == packedPatGenJetsAK8:SubJets   p4, flavour, user floats (ECFs, tau under the other name)
Exit code 0 if everything is equal.
"""
import sys
import ROOT
ROOT.gROOT.SetBatch(True)
ROOT.gSystem.Load("libFWCoreFWLite.so")
ROOT.FWLiteEnabler.enable()
ROOT.gSystem.Load("libPhysicsToolsPatGenJet.so")
ROOT.gSystem.Load("libDataFormatsPatCandidates.so")
from DataFormats.FWLite import Events, Handle

H = Handle("std::vector<pat::GenJet>")
events = Events(sys.argv[1])
bad = []
count = {"ak4": 0, "fat": 0, "sub": 0}

def get(ev, label):
    ev.getByLabel(label, H) if ":" not in label else ev.getByLabel(*label.split(":"), H)
    return [j for j in H.product()]

def same_jet(a, b, what, floats=()):
    for f in ("pt", "eta", "phi", "mass"):
        if getattr(a, f)() != getattr(b, f)():
            bad.append("%s %s differs" % (what, f))
    if (a.hadronFlavour(), a.partonFlavour()) != (b.hadronFlavour(), b.partonFlavour()):
        bad.append("%s flavour differs" % what)
    if a.jetFlavourInfo().getbHadrons().size() != b.jetFlavourInfo().getbHadrons().size():
        bad.append("%s nB differs" % what)
    if [(d.first, d.second) for d in a.getPairDiscri()] != [(d.first, d.second) for d in b.getPairDiscri()]:
        bad.append("%s truth tagger values differ" % what)
    if [p.key() for p in a.daughterPtrVector()] != [p.key() for p in b.daughterPtrVector()]:
        bad.append("%s daughters differ" % what)
    for fa, fb in floats:
        if not a.hasUserFloat(fa) or not b.hasUserFloat(fb) or a.userFloat(fa) != b.userFloat(fb):
            bad.append("%s userFloat %s / %s: %s" % (what, fa, fb, (a.userFloat(fa) if a.hasUserFloat(fa) else None, b.userFloat(fb) if b.hasUserFloat(fb) else None)))

TAU = ["NjettinessAK8Puppi:tau%d" % i for i in (1, 2, 3, 4)]
ECF = ["nb%d%s:ecf%s" % (b, "AK8PuppiSoftDrop", n) for b in (1, 2) for n in ("N2", "N3")]
SECF = ["nb%d%s:ecf%s" % (b, "AK8PuppiSoftDropSubjets", n) for b in (1, 2) for n in ("N2", "N3")]
for ev in events:
    a, b = get(ev, "patGenJetsAK4Tool"), get(ev, "patGenJetsAK4")
    if len(a) != len(b):
        bad.append("AK4 sizes %d %d" % (len(a), len(b)))
    for i, (x, y) in enumerate(zip(a, b)):
        same_jet(x, y, "AK4[%d]" % i); count["ak4"] += 1
    fa, fb = get(ev, "packedPatGenJetsAK8PuppiTool"), get(ev, "packedPatGenJetsAK8")
    if len(fa) != len(fb):
        bad.append("fat sizes %d %d" % (len(fa), len(fb)))
    for i, (x, y) in enumerate(zip(fa, fb)):
        floats = [(t, t) for t in TAU] + [("groomedMassAK8Puppi", "ak8PFJetsPuppiSoftDropMass")]
        if x.hasUserFloat(ECF[0]) != y.hasUserFloat(ECF[0]):
            bad.append("fat[%d] ECF presence differs" % i)
        elif y.hasUserFloat(ECF[0]):
            floats += [(t, t) for t in ECF]
        same_jet(x, y, "fat[%d]" % i, floats)
        sx, sy = x.subjets("SoftDropPuppi"), y.subjets("SoftDropPuppi")
        if [p.key() for p in sx] != [p.key() for p in sy]:
            bad.append("fat[%d] subjet keys differ" % i)
        if x.groomedMass("SoftDropPuppi") != y.groomedMass("SoftDropPuppi"):
            bad.append("fat[%d] groomed mass differs" % i)
        count["fat"] += 1
    sa, sb = get(ev, "packedPatGenJetsAK8PuppiTool:SubJets"), get(ev, "packedPatGenJetsAK8:SubJets")
    if len(sa) != len(sb):
        bad.append("sub sizes %d %d" % (len(sa), len(sb)))
    for i, (x, y) in enumerate(zip(sa, sb)):
        floats = [(t, t) for t in SECF] + [("NjettinessAK8PuppiSubjets:tau%d" % k, "NjettinessAK8Subjets:tau%d" % k) for k in (1, 2, 3, 4)]
        same_jet(x, y, "sub[%d]" % i, floats); count["sub"] += 1
print("jets compared:", count)
if min(count.values()) == 0:
    bad.append("nothing compared in some collection")
if bad:
    print("FAILED (%d):" % len(bad)); print("\n".join("  " + b for b in bad[:30])); sys.exit(1)
print("check_tool_output: OK")
