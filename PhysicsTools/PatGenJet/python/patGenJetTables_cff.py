"""NanoAOD tables of the pat::GenJet collections.

Three tables, one per mode-A product, with module labels ending in "Table" (the NanoAOD output keeps only
nanoaodFlatTable_*Table_*_*, review S6):

    patGenJetAK4Table     name PatGenJet         patGenJetsAK4
    patGenJetAK8Table     name PatGenJetAK8      packedPatGenJetsAK8
    patSubGenJetAK8Table  name PatSubGenJetAK8   packedPatGenJetsAK8:SubJets

The column sets are COPIED from the tables of the running NanoAOD process (jetPuppiTable + jetMCTable, fatJetTable +
fatJetMCTable, subJetTable + subjetMCTable) by configurePatGenJetTables(process); this must run after
nanoAOD_customizeCommon and the era modifiers, so that every Jet/FatJet/SubJet column exists with the same name, type,
precision and expression. Importing this file defines the modules with the columns of the unmodified NanoAOD
definitions (no era), which is only a baseline.

Changes on top of the copy (documentedChanges, used by test/check_columns.py):
  - pt precision -1 (full precision) in all three tables
  - added: nBHadrons, nCHadrons (PatGenJet, PatGenJetAK8), partonFlavour (PatGenJetAK8), genEmEF/genHadEF/
    genInvisibleEF/genAuxiliaryEF (all three)
  - PatGenJetAK8: the three externalVariables (lepInAK8JetVars, reco only) become Var("0", ...) with the same name, type
    and precision, the value SimpleFlatTableProducer writes for a missing map
  - PatGenJetAK8: columns guarded by isPFJet() are un-guarded, so they give the generator values;
    the HF fractions have no generator counterpart and stay guarded (-1)
  - all three: every tagger column (expression reads bDiscriminator) gets the doc suffix TRUTH_DOC: the producer stores
    truth values under the reco tagger labels
  - PatGenJetAK8: any unguarded userFloat read of a label that only the packer writes (groomed-jet ECFs, absent on fat
    jets without a groomed match) is wrapped in hasUserFloat (review M3)
"""
import re
import FWCore.ParameterSet.Config as cms

from PhysicsTools.NanoAOD.common_cff import Var  # a function: process.load ignores it
from PhysicsTools.PatGenJet.simplePATGenJetFlatTableProducer_cfi import simplePATGenJetFlatTableProducer as _simplePATGenJetFlatTableProducer

# labels that only the packer writes (groomed-jet user floats); a fat jet without a groomed match does not have them
PACKER_USER_FLOATS = ('nb1AK8PuppiSoftDrop:ecfN2', 'nb1AK8PuppiSoftDrop:ecfN3',
                      'nb2AK8PuppiSoftDrop:ecfN2', 'nb2AK8PuppiSoftDrop:ecfN3')

# PF-named columns without a generator counterpart stay guarded
_KEEP_GUARDED = ('hfHEF', 'hfEmEF')

documentedChanges = {
    'PatGenJet': dict(
        added=['nBHadrons', 'nCHadrons', 'genEmEF', 'genHadEF', 'genInvisibleEF', 'genAuxiliaryEF'],
        precision=['pt'], replaced=[], unguarded=[]),
    'PatGenJetAK8': dict(
        added=['partonFlavour', 'nBHadrons', 'nCHadrons', 'genEmEF', 'genHadEF', 'genInvisibleEF', 'genAuxiliaryEF'],
        precision=['pt'], replaced=['lsf3', 'muonIdx3SJ', 'electronIdx3SJ'],
        unguarded=['chMultiplicity', 'neMultiplicity', 'chHEF', 'neHEF', 'chEmEF', 'neEmEF', 'muEF']),
    'PatSubGenJetAK8': dict(
        added=['genEmEF', 'genHadEF', 'genInvisibleEF', 'genAuxiliaryEF'],
        precision=['pt'], replaced=[], unguarded=[]),
}

# ---------------------------------------------------------------------------------------------------------
# column construction (plain functions)

def _copy(*tables):
    """Deep copy of the variables (and, as Var("0") placeholders, the externalVariables) of the given tables."""
    out = cms.PSet()
    for table in tables:
        for name in table.variables.parameterNames_():
            setattr(out, name, getattr(table.variables, name).clone())
    return out

def _replaceExternal(out, table):
    """Reco-only externalVariables -> Var("0", type, doc, precision): what SimpleFlatTableProducer writes for a
    missing map with skipNonExistingSrc (SimpleFlatTableProducer.h:152)."""
    if not hasattr(table, 'externalVariables'):
        return
    for name in table.externalVariables.parameterNames_():
        ext = getattr(table.externalVariables, name)
        setattr(out, name, Var("0", ext.type.value(), doc=ext.doc.value() + " (reco only: 0)",
                               precision=ext.precision.value() if hasattr(ext, 'precision') else -1))

_GUARD = re.compile(r"^\?isPFJet\(\)\?(.*):-1$")

def _unguardPF(out):
    for name in out.parameterNames_():
        if name in _KEEP_GUARDED:
            continue
        var = getattr(out, name)
        if hasattr(var, 'expr'):
            m = _GUARD.match(var.expr.value())
            if m:
                var.expr = m.group(1)

def _guardUserFloats(out, labels):
    """Wrap an unguarded userFloat('label') read of one of the labels in hasUserFloat (review M3)."""
    for name in out.parameterNames_():
        var = getattr(out, name)
        if not hasattr(var, 'expr'):
            continue
        expr = var.expr.value()
        for label in labels:
            if "userFloat('%s')" % label in expr and "hasUserFloat('%s')" % label not in expr:
                expr = "?hasUserFloat('%s')?%s:-99999." % (label, expr)
        if expr != var.expr.value():
            var.expr = expr

TRUTH_DOC = " [truth-based on gen jets]"

def _truthDoc(out):
    """the tagger columns hold truth values; say so in the column description."""
    for name in out.parameterNames_():
        var = getattr(out, name)
        if hasattr(var, 'expr') and 'bDiscriminator(' in var.expr.value() and not var.doc.value().endswith(TRUTH_DOC):
            var.doc = var.doc.value() + TRUTH_DOC

def _fractions():
    return cms.PSet(
        genEmEF=Var("emEnergy()/energy()", float, doc="generator electromagnetic energy fraction (reco::GenJet::Specific)", precision=10),
        genHadEF=Var("hadEnergy()/energy()", float, doc="generator hadronic energy fraction (reco::GenJet::Specific)", precision=10),
        genInvisibleEF=Var("invisibleEnergy()/energy()", float, doc="generator invisible energy fraction (reco::GenJet::Specific)", precision=10),
        genAuxiliaryEF=Var("auxiliaryEnergy()/energy()", float, doc="generator auxiliary energy fraction (reco::GenJet::Specific)", precision=10),
    )

def _flavourCounts():
    return cms.PSet(
        nBHadrons=Var("jetFlavourInfo().getbHadrons().size()", "uint8", doc="number of b-hadrons"),
        nCHadrons=Var("jetFlavourInfo().getcHadrons().size()", "uint8", doc="number of c-hadrons"),
    )

def _extend(out, extra):
    for name in extra.parameterNames_():
        setattr(out, name, getattr(extra, name))

def ak4Variables(jetTable, jetMCTable):
    out = _copy(jetTable, jetMCTable)
    _truthDoc(out)
    out.pt.precision = -1
    _extend(out, _flavourCounts())
    _extend(out, _fractions())
    return out

def ak8Variables(fatJetTable, fatJetMCTable):
    out = _copy(fatJetTable, fatJetMCTable)
    _truthDoc(out)
    _replaceExternal(out, fatJetTable)
    _unguardPF(out)
    _guardUserFloats(out, PACKER_USER_FLOATS)
    out.pt.precision = -1
    out.partonFlavour = Var("partonFlavour()", "int16", doc="flavour from parton matching")
    _extend(out, _flavourCounts())
    _extend(out, _fractions())
    return out

def subjetVariables(subJetTable, subjetMCTable):
    out = _copy(subJetTable, subjetMCTable)
    _truthDoc(out)
    out.pt.precision = -1
    _extend(out, _fractions())
    return out

# ---------------------------------------------------------------------------------------------------------
# modules (baseline columns: the unmodified NanoAOD definitions)

from PhysicsTools.NanoAOD.jetsAK4_Puppi_cff import jetPuppiTable as _jetPuppiTable
from PhysicsTools.NanoAOD.jetMC_cff import jetMCTable as _jetMCTable, fatJetMCTable as _fatJetMCTable, subjetMCTable as _subjetMCTable
from PhysicsTools.NanoAOD.jetsAK8_cff import fatJetTable as _fatJetTable, subJetTable as _subJetTable

patGenJetAK4Table = _simplePATGenJetFlatTableProducer.clone(
    src=cms.InputTag("patGenJetsAK4"),
    cut=cms.string("pt > 10"),  # as genJetTable (jetMC_cff.py)
    name=cms.string("PatGenJet"),
    doc=cms.string("patGenJetsAK4: pat::GenJet from ak4GenJetsNoNu (visible generator particles), reco Jet columns"),
    variables=ak4Variables(_jetPuppiTable, _jetMCTable),
)

patGenJetAK8Table = _simplePATGenJetFlatTableProducer.clone(
    src=cms.InputTag("packedPatGenJetsAK8"),
    cut=cms.string("pt > 100."),  # as genJetAK8Table
    name=cms.string("PatGenJetAK8"),
    doc=cms.string("packedPatGenJetsAK8: pat::GenJet AK8 from ak8GenJetsNoNu with soft-drop subjets, reco FatJet columns"),
    variables=ak8Variables(_fatJetTable, _fatJetMCTable),
    externalVariables=cms.PSet(),
)

patSubGenJetAK8Table = _simplePATGenJetFlatTableProducer.clone(
    src=cms.InputTag("packedPatGenJetsAK8", "SubJets"),
    cut=cms.string(""),
    name=cms.string("PatSubGenJetAK8"),
    doc=cms.string("packedPatGenJetsAK8:SubJets: pat::GenJet soft-drop subjets of the AK8 gen jets, reco SubJet columns"),
    variables=subjetVariables(_subJetTable, _subjetMCTable),
)

patGenJetTablesTask = cms.Task(patGenJetAK4Table, patGenJetAK8Table, patSubGenJetAK8Table)

# ---------------------------------------------------------------------------------------------------------
# configuration from the running process

def configurePatGenJetTables(process):
    """Copy the column sets from the tables of the running NanoAOD process into the three modules of `process`
    (call after nanoAOD_customizeCommon). Returns True if the MC tables were found."""
    for needed in ('jetPuppiTable', 'fatJetTable', 'subJetTable'):
        if not hasattr(process, needed):
            raise RuntimeError("patGenJetTables: process has no %s; load PhysicsTools.NanoAOD.nano_cff and call "
                               "nanoAOD_customizeCommon first" % needed)
    if not all(hasattr(process, n) for n in ('jetMCTable', 'fatJetMCTable', 'subjetMCTable')):
        return False  # data: no MC tables, no generator-jet tables
    process.patGenJetAK4Table.variables = ak4Variables(process.jetPuppiTable, process.jetMCTable)
    process.patGenJetAK8Table.variables = ak8Variables(process.fatJetTable, process.fatJetMCTable)
    process.patGenJetAK8Table.externalVariables = cms.PSet()
    process.patSubGenJetAK8Table.variables = subjetVariables(process.subJetTable, process.subjetMCTable)
    return True

# ---------------------------------------------------------------------------------------------------------
# DQM (modelled on nanoDQM_cfi.py GenJetAK8): added from here, no file of PhysicsTools/NanoAOD is edited (review B1)

def _plots(doc, fat=False, sub=False):
    from PhysicsTools.NanoAOD.nanoDQM_tools_cff import Plot1D, Count1D
    plots = [
        Count1D('_size', 25, -0.5, 24.5, doc),
        Plot1D('eta', 'eta', 20, -7, 7, 'eta'),
        Plot1D('genAuxiliaryEF', 'genAuxiliaryEF', 20, 0, 1, 'generator auxiliary energy fraction'),
        Plot1D('genEmEF', 'genEmEF', 20, 0, 1, 'generator electromagnetic energy fraction'),
        Plot1D('genHadEF', 'genHadEF', 20, 0, 1, 'generator hadronic energy fraction'),
        Plot1D('genInvisibleEF', 'genInvisibleEF', 20, 0, 1, 'generator invisible energy fraction'),
        Plot1D('hadronFlavour', 'hadronFlavour', 6, -0.5, 5.5, 'flavour from hadron ghost clustering'),
        Plot1D('mass', 'mass', 20, 0, 200, 'mass'),
        Plot1D('nBHadrons', 'nBHadrons', 4, -0.5, 3.5, 'number of b-hadrons'),
        Plot1D('nCHadrons', 'nCHadrons', 4, -0.5, 3.5, 'number of c-hadrons'),
        Plot1D('phi', 'phi', 20, -3.14159, 3.14159, 'phi'),
        Plot1D('pt', 'pt', 20, 0, 200, 'pt'),
    ]
    if not sub:
        plots.append(Plot1D('partonFlavour', 'partonFlavour', 40, -9.5, 30.5, 'flavour from parton matching'))
    if fat:
        plots += [Plot1D('msoftdrop', 'msoftdrop', 20, 0, 200, 'Soft drop mass'),
                  Plot1D('tau1', 'tau1', 20, 0, 1, 'Nsubjettiness (1 axis)'),
                  Plot1D('tau2', 'tau2', 20, 0, 1, 'Nsubjettiness (2 axis)')]
    return cms.PSet(sels=cms.PSet(), plots=cms.VPSet(*plots))

def addPatGenJetDQM(process):
    """Add the three tables to process.nanoDQM.vplots, if the process has nanoDQM. Returns True if added."""
    if not hasattr(process, 'nanoDQM'):
        return False
    vplots = process.nanoDQM.vplots
    vplots.PatGenJet = _plots('patGenJetsAK4, i.e. ak4 pat::GenJet made with visible genparticles')
    vplots.PatGenJetAK8 = _plots('packedPatGenJetsAK8, i.e. ak8 pat::GenJet made with visible genparticles', fat=True)
    vplots.PatSubGenJetAK8 = _plots('packedPatGenJetsAK8:SubJets, i.e. subjets of ak8 pat::GenJet', sub=True)
    return True
