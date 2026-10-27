# Offline stand-in for the W job, for testing v1_compare.py and v1_compare_nano.py on the C0 sample (not part of the real V1 run).
# run_patgenjets_cfg.py plus: daughterOrder "key" (as PATGenJetSlimmer) and an EDM file that holds what V1 needs.
#   cmsRun v1_offline_cfg.py input=ttbar20_gen.root output=v1_nano.root edm=v1_mini.root
import os
exec(open(os.path.join(os.environ["CMSSW_BASE"], "src/PhysicsTools/PatGenJet/test/run_patgenjets_cfg.py")).read())
for m in (process.patGenJetsAK4, process.patGenJetsAK8, process.patGenJetsAK8SoftDropSubjets):
    m.daughterOrder = "key"
process.out.outputCommands.extend([
    "keep *_slimmedGenJets*_*_*", "keep *_packedGenParticles_*_*", "keep *_ak8GenJetsNoNuSoftDrop_*_*", "keep *_*ForPatGenJets_*_*"])
