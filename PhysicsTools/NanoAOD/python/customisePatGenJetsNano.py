"""Opt-in NanoAOD customisation for pat::GenJet. Nothing here runs, and no pat::GenJet table is added to a process,
unless the function is called (cmsDriver: --customise PhysicsTools/NanoAOD/customisePatGenJetsNano.customisePatGenJetsNano).

    customisePatGenJetsNano(process)
        adds the tables PatGenJet, PatGenJetAK8, PatSubGenJetAK8 to the NanoAOD table task (and the DQM entries).
        Call it AFTER nanoAOD_customizeCommon (cmsDriver appends the customisations in the order given): the column
        sets are copied from the tables of the running process. The tables read the pat::GenJet products: same job as
        customisePatGenJetsMiniAOD (PhysicsTools/PatAlgos/python/slimming/customisePatGenJetsMiniAOD.py), or a MiniAOD
        written with keepInOutput=True.
"""
import FWCore.ParameterSet.Config as cms


def customisePatGenJetsNano(process):
    process.load('PhysicsTools.NanoAOD.patGenJetTables_cff')
    from PhysicsTools.NanoAOD.patGenJetTables_cff import configurePatGenJetTables, addPatGenJetDQM
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
    addPatGenJetDQM(process)
    return process
