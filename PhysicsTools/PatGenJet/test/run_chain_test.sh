#!/bin/bash
# C0, C1, C2, C3 on the offline Pythia sample (all in the test's working directory; the ROOT files are removed at the end)
set -e
T=${LOCALTOP}/src/PhysicsTools/PatGenJet/test
echo "== C0: 20 Pythia ttbar events, gen jets, pruned/packed gen particles"
cmsRun $T/gen_ttbar20_cfg.py output=ttbar20_gen.root > gen.log 2>&1 || { tail -30 gen.log; exit 1; }
echo "== C1/C3: pat::GenJet chain (mode A) and the three NanoAOD tables next to today's gen tables"
cmsRun $T/run_patgenjets_cfg.py input=ttbar20_gen.root output=ttbar20_nano.root edm=ttbar20_pat.root > run.log 2>&1 || { tail -40 run.log; exit 1; }
python3 $T/compare_gen_tables.py ttbar20_nano.root
python3 $T/fwlite_read.py ttbar20_pat.root --label packedPatGenJetsAK8 --max-events 2 > fwlite.log 2>&1 || { tail -20 fwlite.log; exit 1; }
tail -1 fwlite.log
echo "== C2: addPatGenJetCollection gives the same collections as patGenJets_cff"
cmsRun $T/test_tool_cfg.py input=ttbar20_gen.root output=tool.root > tool.log 2>&1 || { tail -40 tool.log; exit 1; }
python3 $T/check_tool_output.py tool.root
rm -f ttbar20_gen.root ttbar20_nano.root ttbar20_pat.root tool.root
echo "chain test OK"
