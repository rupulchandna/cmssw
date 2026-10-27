#!/usr/bin/env python3
"""Member parity check for pat::GenJet.

Uses ROOT/cling reflection of the built dictionaries (no text parsing):
every public member name of pat::io_v1::Jet and of PATObject<reco::io_v1::Jet>
must exist as a public member (declared or inherited) of pat::io_v1::GenJet.
Constructor/destructor names are mapped Jet -> GenJet. The only allowed exception
is the scouting constructor Jet(const Run3ScoutingPFJet&), which
is checked to be absent. Member templates (userData<T>, addUserData<T>) and the
free operator<< are invisible to reflection; they are checked by compiling a snippet.

Informational: argument lists of pat::Jet members that pat::GenJet has under the same
name but with different argument types (expected: GenJet types instead of Jet ones).

Run under cmsenv:  python3 check_member_parity.py   (exit code 0 = pass)
"""
import sys
import ROOT

ROOT.gROOT.SetBatch(True)
ROOT.gErrorIgnoreLevel = ROOT.kError
for lib in ("libFWCoreFWLite.so", "libDataFormatsPatCandidates.so", "libPhysicsToolsPatGenJet.so"):
    if ROOT.gSystem.Load(lib) < 0:
        print(f"FAIL: cannot load {lib}")
        sys.exit(1)
ROOT.FWLiteEnabler.enable()


def public_methods(cname, inherited):
    cl = ROOT.TClass.GetClass(cname)
    if not cl or not cl.HasDictionary():
        print(f"FAIL: no dictionary for {cname}")
        sys.exit(1)
    lst = cl.GetListOfAllPublicMethods() if inherited else cl.GetListOfMethods()
    out = {}
    for m in lst:
        if not inherited and not (m.Property() & ROOT.kIsPublic):
            continue
        args = ",".join(a.GetFullTypeName() for a in m.GetListOfMethodArgs())
        const = " const" if (m.Property() & ROOT.kIsConstMethod) else ""
        out.setdefault(m.GetName(), set()).add(f"({args}){const}")
    return out


jet = public_methods("pat::io_v1::Jet", inherited=False)
patobj = public_methods("pat::PATObject<reco::io_v1::Jet>", inherited=False)
genjet = public_methods("pat::io_v1::GenJet", inherited=True)


def mapped(name, origin):
    if origin == "Jet":
        return {"Jet": "GenJet", "~Jet": "~GenJet"}.get(name, name)
    return {"PATObject<reco::io_v1::Jet>": "GenJet", "PATObject": "GenJet",
            "~PATObject<reco::io_v1::Jet>": "~GenJet", "~PATObject": "~GenJet"}.get(name, name)


failures = []
infos = []
for origin, names in (("Jet", jet), ("PATObject<reco::Jet>", patobj)):
    for name, sigs in sorted(names.items()):
        target = mapped(name, origin)
        if target not in genjet:
            failures.append(f"{origin}::{name} missing on pat::GenJet")
            continue
        if origin == "Jet":
            for sig in sorted(sigs):
                if "Run3ScoutingPFJet" in sig:
                    continue
                if sig not in genjet[target]:
                    infos.append(f"{name}{sig}")

# the scouting constructor exists on pat::Jet and must not exist on pat::GenJet
scouting_on_jet = any("Run3ScoutingPFJet" in s for s in jet.get("Jet", set()))
scouting_on_genjet = any("Run3ScoutingPFJet" in s for s in genjet.get("GenJet", set()))
if not scouting_on_jet:
    failures.append("reference pat::Jet has no scouting constructor (reflection problem?)")
if scouting_on_genjet:
    failures.append("pat::GenJet has a Run3ScoutingPFJet constructor")

# member templates and the free operator<<: compile a snippet
ok = ROOT.gInterpreter.Declare('''
#include "PhysicsTools/PatGenJet/interface/GenJet.h"
#include <sstream>
namespace parity_check {
  inline bool templatesAndStream() {
    pat::GenJet j;
    j.addUserData<reco::Candidate::LorentzVector>("x", reco::Candidate::LorentzVector(1., 2., 3., 4.));
    const auto* p = j.userData<reco::Candidate::LorentzVector>("x");
    std::ostringstream os;
    reco::operator<<(os, j);
    os << j;
    return p != nullptr && p->E() == 4. && !os.str().empty();
  }
}
''')
if not ok or not ROOT.parity_check.templatesAndStream():
    failures.append("userData<T>/addUserData<T>/operator<< check failed")

print(f"pat::io_v1::Jet: {len(jet)} public names; PATObject<reco::io_v1::Jet>: {len(patobj)} public names")
print(f"pat::io_v1::GenJet (with inherited): {len(genjet)} public names")
if infos:
    print(f"\nINFO: {len(infos)} pat::Jet argument lists with no identical overload on pat::GenJet (name exists; expected where the design swaps Jet types for GenJet types):")
    for i in infos:
        print("  " + i)
# The intended type swaps: exactly these pat::Jet argument lists may differ.
EXPECTED_SWAPS = {
    "Jet(const edm::Ptr<pat::Jet>&)", "Jet(const edm::Ptr<reco::Jet>&)", "Jet(const edm::RefToBase<pat::Jet>&)",
    "Jet(const edm::RefToBase<reco::Jet>&)", "Jet(const pat::io_v1::Jet&)", "Jet(const reco::Jet&)",
    "addSubjets(const pat::JetPtrCollection&,const string&)", "operator=(const pat::io_v1::Jet&)",
    "setGenJetRef(const edm::FwdRef<reco::GenJetCollection>&)", "updateFwdGenJetFwdRef(edm::Ref<reco::GenJetCollection>)",
}
for i in sorted(set(infos) - EXPECTED_SWAPS):
    failures.append("unexpected signature difference: " + i)
for i in sorted(EXPECTED_SWAPS - set(infos)):
    failures.append("expected type swap not seen (pat::GenJet now has the pat::Jet signature?): " + i)
if failures:
    print(f"\nFAIL: {len(failures)} problem(s)")
    for f in failures:
        print("  " + f)
    sys.exit(1)
print("\nPASS: every public name of pat::Jet and PATObject<reco::Jet> exists on pat::GenJet "
      "(scouting constructor absent)")
