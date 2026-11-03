"""Opt-in MiniAOD customisation for pat::GenJet. Nothing here runs, and no pat::GenJet module is added to a process,
unless the function is called (cmsDriver: --customise PhysicsTools/PatAlgos/slimming/customisePatGenJetsMiniAOD.customisePatGenJetsMiniAOD).

    customisePatGenJetsMiniAOD(process, keepInOutput=False)
        adds patGenJetsTask (patGenJetsAK4, packedPatGenJetsAK8 and their helper modules) to the PAT tools
        task, and points the forward links of the pat::GenJets to MiniAOD collections: genJetFwdRef() to the slimmed gen
        jets (slimmedGenJets, slimmedGenJetsAK8, slimmedGenJetsAK8SoftDropSubJets) and, if embedGenCandidates is
        switched on, the stored generator particles to prunedGenParticles. A jet or particle without partner (below the
        slimming cut, dropped by the pruning) keeps its link to the AOD collection, which MiniAOD does not store. With keepInOutput the two products are written to the MiniAOD output.

The NanoAOD tables of these products are added by PhysicsTools/NanoAOD/python/customisePatGenJetsNano.py.
"""
import FWCore.ParameterSet.Config as cms
from PhysicsTools.PatAlgos.tools.helpers import getPatAlgosToolsTask

# what MiniAOD makes and the pat::GenJet chain reads (the AOD gen jets ak4GenJetsNoNu, ak8GenJetsNoNu come from the input file)
_MINIAOD_INPUTS = ('ak8GenJetsNoNuSoftDrop', 'packedGenParticles', 'prunedGenParticlesWithStatusOne', 'prunedGenParticles',
                   'slimmedGenJets', 'slimmedGenJetsAK8', 'slimmedGenJetsAK8SoftDropSubJets')
# forward link of genJetFwdRef: Association of the slimmer, from the AOD gen jets to the slimmed ones
_GENJET_FORWARD = {'patGenJetsAK4': 'slimmedGenJets', 'patGenJetsAK8': 'slimmedGenJetsAK8',
                   'patGenJetsAK8SoftDropSubjets': 'slimmedGenJetsAK8SoftDropSubJets'}
# forward Ptrs of the generator particles: genParticles -> prunedGenParticlesWithStatusOne -> prunedGenParticles
_GENCANDIDATES_FORWARD = ('prunedGenParticlesWithStatusOne', 'prunedGenParticles')
_KEEP = ('keep *_patGenJetsAK4_*_*', 'keep *_packedPatGenJetsAK8_*_*')


def customisePatGenJetsMiniAOD(process, keepInOutput=False):
    missing = [n for n in _MINIAOD_INPUTS if not hasattr(process, n)]
    if missing:
        raise RuntimeError("customisePatGenJetsMiniAOD: the process lacks %s (MiniAOD makes them); the pat::GenJet chain needs them" % missing)
    process.load('PhysicsTools.PatAlgos.producersLayer1.patGenJets_cff')
    getPatAlgosToolsTask(process).add(process.patGenJetsTask)
    for label, slimmed in _GENJET_FORWARD.items():
        producer = getattr(process, label)
        producer.genJetForwardMap = cms.InputTag(slimmed, 'slimmedGenJetAssociation')
        producer.genCandidatesForwardMaps = cms.VInputTag(*_GENCANDIDATES_FORWARD)
    if keepInOutput:
        done = 0
        for label, out in process.outputModules_().items():
            commands = out.outputCommands
            if not any('slimmedGenJets' in c for c in commands):
                continue  # not a MiniAOD output module
            for k in _KEEP:
                if k not in commands:
                    commands.append(k)
            done += 1
        if done == 0:
            raise RuntimeError("customisePatGenJetsMiniAOD: keepInOutput=True but no output module keeps slimmedGenJets")
    return process

