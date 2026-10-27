"""pat::GenJet chain, mode A.

Input: the generator jets MiniAOD already uses, made in the same job:
  ak4GenJetsNoNu, ak8GenJetsNoNu, ak8GenJetsNoNuSoftDrop (+ SubJets), ak4GenJetFlavourInfos,
  selectedHadronsAndPartonsForGenJetsFlavourInfos, packedGenParticles (+ its association to genParticles),
  prunedGenParticles (truth taggers, taus, leptons, top/W/Z/H decays).
None of them is defined here, nothing existing is modified.

Names: every helper module is called <reco label>ForPatGenJets; the producers strip that postfix (labelPostfixesToStrip), so the
user-float labels equal the reco ones after stripping, e.g. NjettinessAK8Puppi:tau1, nb1AK8PuppiSoftDrop:ecfN2.

Mode B (NanoAOD from an existing MiniAOD) is dropped: only patGenJetsTaskModeA exists.
"""
import FWCore.ParameterSet.Config as cms

from PhysicsTools.PatGenJet.patGenJetProducer_cfi import patGenJetProducer as _patGenJetProducer
from PhysicsTools.PatGenJet.patGenJetSelector_cfi import patGenJetSelector as _patGenJetSelector
from PhysicsTools.PatGenJet.patGenJetSubstructurePacker_cfi import patGenJetSubstructurePacker as _patGenJetSubstructurePacker

# prototypes are imported under underscore names: process.load adds every public cms object of this file to the process
from RecoJets.JetProducers.nJettinessAdder_cfi import Njettiness as _Njettiness
from RecoJets.JetProducers.ECF_cff import ecfNbeta1 as _ecfNbeta1, ecfNbeta2 as _ecfNbeta2

from PhysicsTools.PatGenJet.patGenJetHelpers import STRIP, userDataPSet, placeholderLabels

# helpers (userDataPSet, placeholderLabels) are in patGenJetHelpers.py

def _ak4Tables():
    # unmodified definitions (no process involved); the customisation re-derives from the running process
    from PhysicsTools.NanoAOD.jetsAK4_Puppi_cff import jetPuppiTable
    from PhysicsTools.NanoAOD.jetMC_cff import jetMCTable
    return [jetPuppiTable, jetMCTable]

# ---------------------------------------------------------------------------------------------------------
# substructure and flavour modules: weights off (gen jets are unweighted)

NjettinessAK8PuppiForPatGenJets = _Njettiness.clone(src='ak8GenJetsNoNu', srcWeights='')
nb1AK8PuppiSoftDropForPatGenJets = _ecfNbeta1.clone(src='ak8GenJetsNoNuSoftDrop', srcWeights='', cuts=cms.vstring('', '', 'pt > 250'))
nb2AK8PuppiSoftDropForPatGenJets = _ecfNbeta2.clone(src='ak8GenJetsNoNuSoftDrop', srcWeights='', cuts=cms.vstring('', '', 'pt > 250'))
NjettinessAK8SubjetsForPatGenJets = _Njettiness.clone(src=cms.InputTag('ak8GenJetsNoNuSoftDrop', 'SubJets'), srcWeights='')
nb1AK8PuppiSoftDropSubjetsForPatGenJets = _ecfNbeta1.clone(src=cms.InputTag('ak8GenJetsNoNuSoftDrop', 'SubJets'), srcWeights='')
nb2AK8PuppiSoftDropSubjetsForPatGenJets = _ecfNbeta2.clone(src=cms.InputTag('ak8GenJetsNoNuSoftDrop', 'SubJets'), srcWeights='')

ak8PFJetsPuppiSoftDropMassForPatGenJets = cms.EDProducer("RecoJetDeltaRValueMapProducer",
    src=cms.InputTag('ak8GenJetsNoNu'),
    matched=cms.InputTag('ak8GenJetsNoNuSoftDrop'),
    distMax=cms.double(0.8),
    value=cms.string('mass'),
)

# flavour of the fat jets and, in the "SubJets" instance, of the soft-drop subjets (JetFlavourClustering.cc:231, :241)
ak8GenJetFlavourInfosForPatGenJets = cms.EDProducer("JetFlavourClustering",
    jets=cms.InputTag('ak8GenJetsNoNu'),
    groomedJets=cms.InputTag('ak8GenJetsNoNuSoftDrop'),
    subjets=cms.InputTag('ak8GenJetsNoNuSoftDrop', 'SubJets'),
    bHadrons=cms.InputTag('selectedHadronsAndPartonsForGenJetsFlavourInfos', 'bHadrons'),
    cHadrons=cms.InputTag('selectedHadronsAndPartonsForGenJetsFlavourInfos', 'cHadrons'),
    partons=cms.InputTag('selectedHadronsAndPartonsForGenJetsFlavourInfos', 'physicsPartons'),
    leptons=cms.InputTag('selectedHadronsAndPartonsForGenJetsFlavourInfos', 'leptons'),
    jetAlgorithm=cms.string('AntiKt'),
    rParam=cms.double(0.8),
    ghostRescaling=cms.double(1e-18),
    hadronFlavourHasPriority=cms.bool(False),
)

# ---------------------------------------------------------------------------------------------------------
# pat::GenJet producers

# AK4: no generator-side user data; the labels the Jet table reads are placeholders
patGenJetsAK4 = _patGenJetProducer.clone(
    jetSource='ak4GenJetsNoNu',
    packedGenParticles='packedGenParticles',
    JetFlavourInfoSource='ak4GenJetFlavourInfos',
    genParticles='prunedGenParticles',  # truth taggers
    userData=userDataPSet(),
    **placeholderLabels(_ak4Tables()))

# AK8 fat jets: N-subjettiness 1..4 and soft-drop mass as in applySubstructure_cff.py:126-133
patGenJetsAK8 = _patGenJetProducer.clone(
    jetSource='ak8GenJetsNoNu',
    packedGenParticles='packedGenParticles',
    JetFlavourInfoSource='ak8GenJetFlavourInfosForPatGenJets',
    truthJetType='AK8', genParticles='prunedGenParticles',  # resonance classes
    userData=userDataPSet(floats=[
        cms.InputTag('NjettinessAK8PuppiForPatGenJets', 'tau1'),
        cms.InputTag('NjettinessAK8PuppiForPatGenJets', 'tau2'),
        cms.InputTag('NjettinessAK8PuppiForPatGenJets', 'tau3'),
        cms.InputTag('NjettinessAK8PuppiForPatGenJets', 'tau4'),
        cms.InputTag('ak8PFJetsPuppiSoftDropMassForPatGenJets'),
    ]))

# slimmedGenJetsAK8 selection (slimmedGenJets_cfi.py:16-18)
selectedPatGenJetsAK8 = _patGenJetSelector.clone(
    src='patGenJetsAK8',
    cut='pt > 80',
    cutLoose='pt > 10.',
    nLoose=3)

# soft-drop subjets: N-subjettiness and ECFs as in applySubstructure_cff.py:90-92, :94-96
patGenJetsAK8SoftDropSubjets = _patGenJetProducer.clone(
    jetSource=cms.InputTag('ak8GenJetsNoNuSoftDrop', 'SubJets'),
    packedGenParticles='packedGenParticles',
    JetFlavourInfoSource=cms.InputTag('ak8GenJetFlavourInfosForPatGenJets', 'SubJets'),
    genParticles='prunedGenParticles',  # AK4 classes (the SubJet table reads AK4 labels)
    userData=userDataPSet(floats=[
        cms.InputTag('nb1AK8PuppiSoftDropSubjetsForPatGenJets', 'ecfN2'),
        cms.InputTag('nb1AK8PuppiSoftDropSubjetsForPatGenJets', 'ecfN3'),
        cms.InputTag('nb2AK8PuppiSoftDropSubjetsForPatGenJets', 'ecfN2'),
        cms.InputTag('nb2AK8PuppiSoftDropSubjetsForPatGenJets', 'ecfN3'),
        cms.InputTag('NjettinessAK8SubjetsForPatGenJets', 'tau1'),
        cms.InputTag('NjettinessAK8SubjetsForPatGenJets', 'tau2'),
        cms.InputTag('NjettinessAK8SubjetsForPatGenJets', 'tau3'),
        cms.InputTag('NjettinessAK8SubjetsForPatGenJets', 'tau4'),
    ]))

# one module for BoostedJetMerger + JetSubstructurePacker; label SoftDropPuppi as reco
packedPatGenJetsAK8 = _patGenJetSubstructurePacker.clone(
    jetSrc='selectedPatGenJetsAK8',
    distMax=0.8,
    fixDaughters=False,
    labelPostfixesToStrip=[STRIP],
    algos=cms.VPSet(cms.PSet(
        label=cms.string('SoftDropPuppi'),
        groomedJets=cms.InputTag('ak8GenJetsNoNuSoftDrop'),
        subjets=cms.InputTag('patGenJetsAK8SoftDropSubjets'),
        groomedUserFloats=cms.VInputTag(
            cms.InputTag('nb1AK8PuppiSoftDropForPatGenJets', 'ecfN2'),
            cms.InputTag('nb1AK8PuppiSoftDropForPatGenJets', 'ecfN3'),
            cms.InputTag('nb2AK8PuppiSoftDropForPatGenJets', 'ecfN2'),
            cms.InputTag('nb2AK8PuppiSoftDropForPatGenJets', 'ecfN3')),
        subjetInstance=cms.string('SubJets'))))

patGenJetsTaskModeA = cms.Task(
    NjettinessAK8PuppiForPatGenJets,
    nb1AK8PuppiSoftDropForPatGenJets,
    nb2AK8PuppiSoftDropForPatGenJets,
    NjettinessAK8SubjetsForPatGenJets,
    nb1AK8PuppiSoftDropSubjetsForPatGenJets,
    nb2AK8PuppiSoftDropSubjetsForPatGenJets,
    ak8PFJetsPuppiSoftDropMassForPatGenJets,
    ak8GenJetFlavourInfosForPatGenJets,
    patGenJetsAK4,
    patGenJetsAK8,
    selectedPatGenJetsAK8,
    patGenJetsAK8SoftDropSubjets,
    packedPatGenJetsAK8,
)
