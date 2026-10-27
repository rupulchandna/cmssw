"""Opt-in customisations for pat::GenJet. Nothing here runs, and nothing of this package is imported into a
process, unless one of the functions is called (cmsDriver: --customise PhysicsTools/PatGenJet/customisePatGenJets.<name>).

    customisePatGenJetsMiniAOD(process, keepInOutput=False)
        mode A: adds patGenJetsTaskModeA (patGenJetsAK4, packedPatGenJetsAK8 and their helper modules) to the PAT tools
        task. With keepInOutput the two products are written to the MiniAOD output.
    customisePatGenJetsNano(process)
        adds the tables PatGenJet, PatGenJetAK8, PatSubGenJetAK8 to the NanoAOD table task (and the DQM entries).
        Call it AFTER nanoAOD_customizeCommon (cmsDriver appends the customisations in the order given): the column
        sets are copied from the tables of the running process. The tables read the mode-A products: same job as
        customisePatGenJetsMiniAOD, or a MiniAOD written with keepInOutput=True.

Mode B (NanoAOD from an existing MiniAOD) is dropped, so there is no `mode` argument.
"""
import FWCore.ParameterSet.Config as cms
from PhysicsTools.PatAlgos.tools.helpers import getPatAlgosToolsTask

# what MiniAOD makes and mode A reads (the AOD gen jets ak4GenJetsNoNu, ak8GenJetsNoNu come from the input file)
_MINIAOD_INPUTS = ('ak8GenJetsNoNuSoftDrop', 'ak4GenJetFlavourInfos', 'selectedHadronsAndPartonsForGenJetsFlavourInfos',
                   'packedGenParticles')
_KEEP = ('keep *_patGenJetsAK4_*_*', 'keep *_packedPatGenJetsAK8_*_*')


def customisePatGenJetsMiniAOD(process, keepInOutput=False):
    missing = [n for n in _MINIAOD_INPUTS if not hasattr(process, n)]
    if missing:
        raise RuntimeError("customisePatGenJetsMiniAOD: the process lacks %s (MiniAOD makes them); mode A needs them" % missing)
    process.load('PhysicsTools.PatGenJet.patGenJets_cff')
    getPatAlgosToolsTask(process).add(process.patGenJetsTaskModeA)
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


def customisePatGenJetsNano(process):
    process.load('PhysicsTools.PatGenJet.patGenJetTables_cff')
    from PhysicsTools.PatGenJet.patGenJetTables_cff import configurePatGenJetTables, addPatGenJetDQM
    if not configurePatGenJetTables(process):
        # data: the tables of the generator-level jets do not exist
        for label in ('patGenJetAK4Table', 'patGenJetAK8Table', 'patSubGenJetAK8Table'):
            delattr(process, label)
        del process.patGenJetTablesTask
        print("customisePatGenJetsNano: no MC jet tables in the process (data): nothing added")
        return process
    if hasattr(process, 'nanoTableTaskFS'):
        process.nanoTableTaskFS.add(process.patGenJetTablesTask)
    else:
        raise RuntimeError("customisePatGenJetsNano: the process has no nanoTableTaskFS (not a NanoAOD MC configuration)")
    # the labels the tables read are in the producers' placeholder lists, re-derived from the
    # running tables when the producers are in the same process
    if hasattr(process, 'patGenJetsAK4'):
        from PhysicsTools.PatGenJet.patGenJetHelpers import placeholderLabels
        derived = placeholderLabels([process.jetPuppiTable, process.jetMCTable])
        producer = process.patGenJetsAK4
        for name in ('placeholderUserFloats', 'placeholderUserInts'):
            current = list(getattr(producer, name))
            getattr(producer, name).extend([l for l in derived[name] if l not in current])
        if not list(producer.placeholderPtEtaPhiUserFloats) and derived['placeholderPtEtaPhiUserFloats']:
            producer.placeholderPtEtaPhiUserFloats = derived['placeholderPtEtaPhiUserFloats']
    addPatGenJetDQM(process)
    return process
