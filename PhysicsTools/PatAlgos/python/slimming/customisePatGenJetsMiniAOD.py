"""Opt-in MiniAOD customisation for pat::GenJet. Nothing here runs, and no pat::GenJet module is added to a process,
unless the function is called (cmsDriver: --customise PhysicsTools/PatAlgos/slimming/customisePatGenJetsMiniAOD.customisePatGenJetsMiniAOD).

    customisePatGenJetsMiniAOD(process, keepInOutput=False)
        adds patGenJetsTask (patGenJetsAK4, packedPatGenJetsAK8 and their helper modules) to the PAT tools
        task. With keepInOutput the two products are written to the MiniAOD output.

The NanoAOD tables of these products are added by PhysicsTools/NanoAOD/python/customisePatGenJetsNano.py.
"""
import FWCore.ParameterSet.Config as cms
from PhysicsTools.PatAlgos.tools.helpers import getPatAlgosToolsTask

# what MiniAOD makes and the pat::GenJet chain reads (the AOD gen jets ak4GenJetsNoNu, ak8GenJetsNoNu come from the input file)
_MINIAOD_INPUTS = ('ak8GenJetsNoNuSoftDrop', 'ak4GenJetFlavourInfos', 'selectedHadronsAndPartonsForGenJetsFlavourInfos',
                   'packedGenParticles')
_KEEP = ('keep *_patGenJetsAK4_*_*', 'keep *_packedPatGenJetsAK8_*_*')


def customisePatGenJetsMiniAOD(process, keepInOutput=False):
    missing = [n for n in _MINIAOD_INPUTS if not hasattr(process, n)]
    if missing:
        raise RuntimeError("customisePatGenJetsMiniAOD: the process lacks %s (MiniAOD makes them); the pat::GenJet chain needs them" % missing)
    process.load('PhysicsTools.PatAlgos.producersLayer1.patGenJets_cff')
    getPatAlgosToolsTask(process).add(process.patGenJetsTask)
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

