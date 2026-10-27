#!/usr/bin/env python3
"""V3 (size and timing) and a truth-tagger check, from one step-3 job run with the PatGenJet customisations.

    python3 v3_report.py --log step3.log --miniaod step3_inMINIAODSIM.root --nano step3_inNANOAODSIM.root [--report v3_report.txt]

- Timing: the framework's "TimeReport ---------- Module Summary" (process.options.wantSummary = True), per event,
  for the package's modules against all modules of the job.
- Size: compressed bytes per event of the pat::GenJet branches in the MiniAOD file (edmEventSize), and of the three
  NanoAOD tables (PatGenJet, PatGenJetAK8, PatSubGenJetAK8 and their counts), each against the whole file.
- Truth taggers: on PatGenJet, btagDeepFlavB is 1 exactly for hadronFlavour 5 and 0 otherwise;
  on PatGenJetAK8, the truth classes it reads are reported per class.
"""
import argparse, re, subprocess, sys
import numpy as np

OURS = re.compile(r"(PatGenJet|patGenJet|ForPatGenJets)")


def timing(log):
    rows, inside = {}, False
    for line in open(log, errors="replace"):
        if "TimeReport ---------- Module Summary" in line:
            inside, rows = True, {}
            continue
        if inside:
            m = re.match(r"TimeReport\s+([0-9.eE+-]+)\s+([0-9.eE+-]+)\s+([0-9.eE+-]+)\s+(\S+)\s*$", line)
            if m:
                rows[m.group(4)] = float(m.group(1))
            elif rows and not line.startswith("TimeReport"):
                inside = False
    return rows


def edm_sizes(path):
    out = subprocess.run(["edmEventSize", "-v", path], capture_output=True, text=True).stdout
    nev, sizes = 0, {}
    for line in out.splitlines():
        m = re.match(r"File .* Events (\d+)", line)
        if m:
            nev = int(m.group(1))
        parts = line.split()
        if len(parts) == 3 and parts[0].endswith("."):
            try:
                sizes[parts[0].rstrip(".")] = float(parts[2])
            except ValueError:
                pass
    return nev, sizes


def nano_sizes(path):
    import uproot
    t = uproot.open(path)["Events"]
    n = t.num_entries
    per = {}
    for b in t.branches:
        per[b.name] = b.compressed_bytes / max(n, 1)
    return n, per, t


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log", required=True)
    ap.add_argument("--miniaod", required=True)
    ap.add_argument("--nano", required=True)
    ap.add_argument("--report", default="")
    ap.add_argument("--baseline-miniaod", default="", help="same job without the customisations: whole-file comparison")
    ap.add_argument("--baseline-nano", default="")
    a = ap.parse_args()
    L, problems = [], []

    t = timing(a.log)
    if not t:
        problems.append("no TimeReport module summary in %s (was process.options.wantSummary set?)" % a.log)
    else:
        mine = {k: v for k, v in t.items() if OURS.search(k)}
        tot, own = sum(t.values()), sum(mine.values())
        L.append("TIMING (real seconds per event, framework module summary)")
        L.append("  all modules of the job       %10.4f s" % tot)
        L.append("  PatGenJet package modules    %10.4f s  (%.2f %% of the job, %d modules)" % (own, 100 * own / tot if tot else 0, len(mine)))
        for k, v in sorted(mine.items(), key=lambda kv: -kv[1])[:12]:
            L.append("    %-45s %.5f s" % (k, v))

    nev, sz = edm_sizes(a.miniaod)
    if sz:
        tot = sum(sz.values())
        mine = {k: v for k, v in sz.items() if k.startswith("patGenJets_") or OURS.search(k)}
        own = sum(mine.values())
        L.append("")
        L.append("MINIAOD SIZE (compressed bytes per event, %d events; the run keeps the collections with keepInOutput=True)" % nev)
        L.append("  whole file branches          %10.0f B/event" % tot)
        L.append("  pat::GenJet branches         %10.0f B/event  (%.2f %% of the file)" % (own, 100 * own / tot if tot else 0))
        for k, v in sorted(mine.items(), key=lambda kv: -kv[1]):
            L.append("    %-60s %8.0f" % (k, v))
    else:
        problems.append("edmEventSize gave no branches for %s" % a.miniaod)

    n, per, tree = nano_sizes(a.nano)
    tot = sum(per.values())
    groups = {g: sum(v for k, v in per.items() if k.startswith(g + "_") or k == "n" + g) for g in ("PatGenJet", "PatGenJetAK8", "PatSubGenJetAK8")}
    own = sum(groups.values())
    L.append("")
    L.append("NANOAOD SIZE (compressed bytes per event, %d events)" % n)
    L.append("  whole file                   %10.0f B/event" % tot)
    L.append("  three PatGenJet tables       %10.0f B/event  (%.2f %% of the file)" % (own, 100 * own / tot if tot else 0))
    for g, v in groups.items():
        L.append("    %-20s %8.0f B/event" % (g, v))

    if a.baseline_miniaod or a.baseline_nano:
        L.append("")
        L.append("WHOLE-FILE COMPARISON WITH THE BASELINE JOB (same input, no customisation)")
        if a.baseline_miniaod:
            bn, bsz = edm_sizes(a.baseline_miniaod)
            wn, wsz = edm_sizes(a.miniaod)
            bt, wt = sum(bsz.values()), sum(wsz.values())
            L.append("  MiniAOD: %8.0f -> %8.0f B/event  (+%.0f B, +%.2f %%)" % (bt, wt, wt - bt, 100 * (wt - bt) / bt if bt else 0))
        if a.baseline_nano:
            bn2, bper, _ = nano_sizes(a.baseline_nano)
            bt2 = sum(bper.values())
            L.append("  NanoAOD: %8.0f -> %8.0f B/event  (+%.0f B, +%.2f %%)" % (bt2, tot, tot - bt2, 100 * (tot - bt2) / bt2 if bt2 else 0))
    L.append("")
    L.append("TRUTH TAGGERS")
    keys = tree.keys()
    if "PatGenJet_btagDeepFlavB" in keys and "PatGenJet_hadronFlavour" in keys:
        import awkward as ak
        b = ak.to_numpy(ak.flatten(tree["PatGenJet_btagDeepFlavB"].array()))
        hf = ak.to_numpy(ak.flatten(tree["PatGenJet_hadronFlavour"].array()))
        isb = hf == 5
        ok_b = np.all(b[isb] == 1) if isb.any() else True
        ok_o = np.all(b[~isb] == 0) if (~isb).any() else True
        L.append("  AK4: btagDeepFlavB == 1 for %d of %d b jets; == 0 for %d of %d other jets  %s"
                 % (int((b[isb] == 1).sum()), int(isb.sum()), int((b[~isb] == 0).sum()), int((~isb).sum()), "PASS" if ok_b and ok_o else "DIFF"))
        if not (ok_b and ok_o):
            problems.append("truth b-tag does not follow hadronFlavour")
    else:
        problems.append("PatGenJet_btagDeepFlavB or PatGenJet_hadronFlavour missing in the NanoAOD file")
    cls = [k for k in keys if k.startswith("PatGenJetAK8_globalParT3_") and not k.endswith(("massCorrX2p", "massCorrGeneric"))]
    if cls:
        import awkward as ak
        L.append("  AK8 (GlobalParT truth classes, jets with value 1):")
        for k in sorted(cls):
            v = ak.to_numpy(ak.flatten(tree[k].array()))
            c = int((v == 1).sum())
            if c:
                L.append("    %-40s %d" % (k.replace("PatGenJetAK8_", ""), c))

    L.append("")
    L.append("RESULT: %s" % ("OK" if not problems else "PROBLEMS"))
    for p in problems:
        L.append("  PROBLEM: " + p)
    text = "\n".join(L)
    print(text)
    if a.report:
        open(a.report, "w").write(text + "\n")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
