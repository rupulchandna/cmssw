#!/usr/bin/env python3
"""V1 row 8: the PatGen* NanoAOD tables against today's gen tables, in one NanoAOD file.

  python3 v1_compare_nano.py <nanoaod.root> [--report report.txt]

  PatGenJet        vs GenJet         same row count per event; pt, eta, phi, mass bitwise; partonFlavour, hadronFlavour,
                                     nBHadrons, nCHadrons equal
  PatGenJetAK8     vs GenJetAK8      same; the flavour columns are REPORTED, not failed (known deviation K4:
                                     the AK8 flavour is clustered on full constituents)
  PatSubGenJetAK8  vs SubGenJetAK8   same row count; pt, eta, phi, mass bitwise
The GenJet* tables come from the same job (standard NANO MC). Known deviations are printed separately.
Exit code 0: no DIFF.   Needs cmsenv (uproot, awkward).
"""
import argparse
import sys

import awkward as ak
import uproot

P4 = ["pt", "eta", "phi", "mass"]
FLAV = ["partonFlavour", "hadronFlavour", "nBHadrons", "nCHadrons"]

ap = argparse.ArgumentParser()
ap.add_argument("file")
ap.add_argument("--report", default="")
a = ap.parse_args()

t = uproot.open(a.file)["Events"]
keys = set(t.keys())
lines = ["V1 row 8 report: %s, %d events" % (a.file, t.num_entries)]
known = []
diffs = []
result = {}


def col(name):
    return t[name].array(library="ak")


def compare(new, old, columns, flavour_known):
    n_row = 0
    problems = []
    for tab in (new, old):
        if "n" + tab not in keys:
            return "MISSING", ["table %s is not in the file" % tab], 0
    if not ak.all(col("n" + new) == col("n" + old)):
        bad = int(ak.sum(col("n" + new) != col("n" + old)))
        problems.append("row counts differ in %d events" % bad)
    else:
        n_row = int(ak.sum(col("n" + new)))
        for c in columns:
            if "%s_%s" % (new, c) not in keys or "%s_%s" % (old, c) not in keys:
                problems.append("column %s missing in %s or %s" % (c, new, old))
                continue
            x, y = col("%s_%s" % (new, c)), col("%s_%s" % (old, c))
            nd = int(ak.sum(ak.flatten(x != y)))
            if nd:
                msg = "%s.%s: %d of %d rows differ" % (new, c, nd, n_row)
                if flavour_known and c in FLAV:
                    known.append("K4 " + msg + " (AK8 flavour)")
                else:
                    problems.append(msg)
    return ("DIFF" if problems else "PASS"), problems, n_row


failed = False
for new, old, cols, kn in (("PatGenJet", "GenJet", P4 + FLAV, False),
                           ("PatGenJetAK8", "GenJetAK8", P4 + FLAV, True),
                           ("PatSubGenJetAK8", "SubGenJetAK8", P4, False)):
    status, problems, n = compare(new, old, cols, kn)
    if n == 0 and status == "PASS":
        status = "NOT CHECKED (no rows)"
    if status != "PASS":
        failed = True
    lines.append("row 8 %-16s vs %-13s %-6s rows %d" % (new, old, status, n))
    lines.extend("      " + p for p in problems[:8])
lines.append("")
lines.append("KNOWN DEVIATIONS (reported, not failed)")
lines.extend("  " + k for k in known)
if not known:
    lines.append("  none seen")
lines.append("")
lines.append("RESULT: " + ("DIFF (see above)" if failed else "PASS"))
text = "\n".join(lines)
print(text)
if a.report:
    open(a.report, "w").write(text + "\n")
sys.exit(1 if failed else 0)
