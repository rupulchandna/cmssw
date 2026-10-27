#!/bin/bash
# Offline check of v1_compare.py and v1_compare_nano.py on the C0 sample (not part of scram runtests; needs cmsenv, ~1 minute).
# The C0 sample has no real MiniAOD step: v1_offline_cfg.py makes the same products in one job. Row 5 and 9 numbers are small here.
#   bash run_v1_offline_test.sh [work_dir]
set -e
W=${1:-$(mktemp -d /tmp/$USER/v1off_XXXX)}; mkdir -p $W; cd $W
T=$CMSSW_BASE/src/PhysicsTools/PatGenJet/test
cmsRun $T/gen_ttbar20_cfg.py output=ttbar20_gen.root > gen.log 2>&1
cmsRun $T/v1_offline_cfg.py input=ttbar20_gen.root output=v1_nano.root edm=v1_mini.root > run.log 2>&1
python3 $T/v1_compare.py v1_mini.root --report v1_report.txt
python3 $T/v1_compare_nano.py v1_nano.root --report v1_nano_report.txt
echo "work dir: $W (remove it when done)"
