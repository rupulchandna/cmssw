# C2: addPatGenJetCollection on the C0 sample. Configuration asserts at load time, then the tool-made collections run next to
# the ones of patGenJets_cff; check_tool_output.py compares them.
#
#   cmsRun test_tool_cfg.py input=ttbar20_gen.root output=tool.root
import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")
options.register("input", "ttbar20_gen.root", VarParsing.multiplicity.singleton, VarParsing.varType.string, "C0 sample")
options.register("output", "tool.root", VarParsing.multiplicity.singleton, VarParsing.varType.string, "EDM output")
options.parseArguments()

process = cms.Process("TOOL")
process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 10
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(-1))
process.source = cms.Source("PoolSource", fileNames=cms.untracked.vstring("file:" + options.input))
process.load("PhysicsTools.JetMCAlgos.AK4GenJetFlavourInfos_cfi")
process.load("PhysicsTools.PatGenJet.patGenJets_cff")
from PhysicsTools.PatAlgos.tools.helpers import getPatAlgosToolsTask
task = getPatAlgosToolsTask(process)
task.add(process.ak4GenJetFlavourInfos, process.patGenJetsTaskModeA)

before = {l: m.dumpPython() for d in (process.producers_(), process.filters_()) for l, m in d.items()}

from PhysicsTools.PatGenJet.patGenJetTools import addPatGenJetCollection
# the hadron selector is not in the process: the tool creates it (new module only)
# postfix Tool: the modules of patGenJets_cff already use the labels without postfix; the user-data labels are the same after stripping
addPatGenJetCollection(process, labelName='AK8Puppi', genJetSource='ak8GenJetsNoNu', algo='AK', rParam=0.8,
                       groomedSource='ak8GenJetsNoNuSoftDrop', postfix='Tool')
process.selectedPatGenJetsAK8PuppiTool.cut = 'pt > 80'
process.selectedPatGenJetsAK8PuppiTool.cutLoose = 'pt > 10.'
process.selectedPatGenJetsAK8PuppiTool.nLoose = 3
addPatGenJetCollection(process, 'AK4Tool', 'ak4GenJetsNoNu', 'AK', 0.4, substructure=False)
addPatGenJetCollection(process, labelName='AK8Puppi', genJetSource='ak8GenJetsNoNu', algo='AK', rParam=0.8,
                       groomedSource='ak8GenJetsNoNuSoftDrop', postfix='Test', flavour=False, substructure=False)

# ---- configuration asserts
after = {l: m.dumpPython() for d in (process.producers_(), process.filters_()) for l, m in d.items()}
assert all(after[l] == before[l] for l in before), "the tool modified an existing module"
new = sorted(set(after) - set(before))
expected = sorted(n + 'Tool' for n in [
    'NjettinessAK8PuppiForPatGenJets', 'groomedMassAK8PuppiForPatGenJets', 'nb1AK8PuppiSoftDropForPatGenJets',
    'nb2AK8PuppiSoftDropForPatGenJets', 'nb1AK8PuppiSoftDropSubjetsForPatGenJets', 'nb2AK8PuppiSoftDropSubjetsForPatGenJets',
    'NjettinessAK8PuppiSubjetsForPatGenJets', 'AK8PuppiFlavourInfosForPatGenJets', 'patGenJetsAK8Puppi',
    'patGenJetsAK8PuppiSubjets', 'selectedPatGenJetsAK8Puppi', 'packedPatGenJetsAK8Puppi']) + [
    'selectedHadronsAndPartonsForGenJetsFlavourInfos', 'AK4ToolFlavourInfosForPatGenJets', 'patGenJetsAK4Tool',
    'patGenJetsAK8PuppiTest', 'patGenJetsAK8PuppiSubjetsTest', 'selectedPatGenJetsAK8PuppiTest', 'packedPatGenJetsAK8PuppiTest']
expected = sorted(expected)
# the three calls collide on the hadron selector name only if the tool re-created it
assert new == expected, "modules added by the tool: %s, expected %s" % (new, expected)
tags = lambda v: [t.value() for t in v]
p = process.patGenJetsAK8PuppiTool
assert tags(p.userData.userFloats.src) == ['NjettinessAK8PuppiForPatGenJetsTool:tau1', 'NjettinessAK8PuppiForPatGenJetsTool:tau2',
    'NjettinessAK8PuppiForPatGenJetsTool:tau3', 'NjettinessAK8PuppiForPatGenJetsTool:tau4', 'groomedMassAK8PuppiForPatGenJetsTool'], tags(p.userData.userFloats.src)
assert list(p.userData.userFloats.labelPostfixesToStrip) == ['ForPatGenJetsTool']
assert p.JetFlavourInfoSource.moduleLabel == 'AK8PuppiFlavourInfosForPatGenJetsTool'
assert process.patGenJetsAK8PuppiSubjetsTool.JetFlavourInfoSource.value() == 'AK8PuppiFlavourInfosForPatGenJetsTool:SubJets'
assert process.patGenJetsAK8PuppiSubjetsTool.jetSource.value() == 'ak8GenJetsNoNuSoftDrop:SubJets'
assert process.AK8PuppiFlavourInfosForPatGenJetsTool.groomedJets.moduleLabel == 'ak8GenJetsNoNuSoftDrop'
assert process.AK8PuppiFlavourInfosForPatGenJetsTool.rParam.value() == 0.8 and process.AK8PuppiFlavourInfosForPatGenJetsTool.jetAlgorithm.value() == 'AntiKt'
k = process.packedPatGenJetsAK8PuppiTool
assert k.jetSrc.value() == 'selectedPatGenJetsAK8PuppiTool' and k.algos[0].label.value() == 'SoftDropPuppi'
assert tags(k.algos[0].groomedUserFloats) == ['nb1AK8PuppiSoftDropForPatGenJetsTool:ecfN2', 'nb1AK8PuppiSoftDropForPatGenJetsTool:ecfN3',
    'nb2AK8PuppiSoftDropForPatGenJetsTool:ecfN2', 'nb2AK8PuppiSoftDropForPatGenJetsTool:ecfN3']
assert len(process.patGenJetsAK4Tool.userData.userFloats.src) == 0 and not hasattr(process, 'NjettinessAK4ToolForPatGenJets')
assert process.patGenJetsAK8PuppiTest.JetFlavourInfoSource.moduleLabel == ''  # flavour=False
assert list(process.packedPatGenJetsAK8PuppiTest.labelPostfixesToStrip) == ['ForPatGenJetsTest']
for l in new:
    assert hasattr(process, l)
# all new modules are in the PAT tools task
inTask = set()
class V:
    def enter(self, m):
        if hasattr(m, 'label_'):
            try: inTask.add(m.label_())
            except Exception: pass
    def leave(self, m): pass
task.visit(V())
assert set(new) <= inTask, set(new) - inTask
# errors
for kw, msg in ((dict(labelName='AK8Puppi', genJetSource='ak8GenJetsNoNu', groomedSource='ak8GenJetsNoNuSoftDrop', postfix='Tool'), 'clash'),
                (dict(labelName='X1', genJetSource='ak8GenJetsNoNu', algo='XX'), 'algo'),
                (dict(labelName='X2'), 'source')):
    try:
        addPatGenJetCollection(process, **kw)
    except (ValueError, KeyError, Exception) as e:
        pass
    else:
        raise AssertionError("no error for " + msg)

process.p = cms.Path(task)
keep = ["drop *", "keep *_patGenJets*_*_*", "keep *_packedPatGenJets*_*_*", "keep *_selectedPatGenJets*_*_*", "keep *_packedGenParticles_*_*"]
process.out = cms.OutputModule("PoolOutputModule", fileName=cms.untracked.string(options.output), outputCommands=cms.untracked.vstring(*keep))
process.e = cms.EndPath(process.out)
