# PhysicsTools/PatGenJet: `pat::GenJet`

Local package, built against `CMSSW_20_1_0_pre3`. It is not part of CMSSW and has not been pushed anywhere (see section 7).

## 1. What it is

`pat::GenJet` is to `reco::GenJet` what `pat::Jet` is to `reco::PFJet`: the analysis-level object, with flavour, subjets, user data and the full `pat::Jet` interface, built on the generator jet.

| Piece | CMSSW term | Name |
|---|---|---|
| Package | CMSSW package | `PhysicsTools/PatGenJet` (library `libPhysicsToolsPatGenJet.so`) |
| Data format | class, `pat::PATObject<reco::GenJet>` with dictionary and class version 3 | `pat::GenJet` (`pat::io_v1::GenJet`), header `PhysicsTools/PatGenJet/interface/GenJet.h`; also `pat::GenJetCollection`, `pat::GenJetRef`, `pat::GenJetFwdRef`, `pat::GenJetPtrCollection` |
| Producer | `edm::stream::EDProducer` `PATGenJetProducer` | `reco::GenJet` view in, `std::vector<pat::GenJet>` out (counterpart of `PATJetProducer`): flavour, user data, constituents re-keyed to `packedGenParticles`, sorting in pt, self links |
| Selector | `edm::stream::EDFilter` `PATGenJetSelector` | counterpart of `PATJetSelector`: string cut, `cutLoose`/`nLoose`, optional slimming |
| Packer | `edm::stream::EDProducer` `PATGenJetSubstructurePacker` | counterpart of `BoostedJetMerger` plus `JetSubstructurePacker` in one module: attaches soft-drop subjets and groomed-jet user floats to the fat jets, writes the subjets as instance `SubJets` |
| Table producer | `SimpleFlatTableProducer<pat::GenJet>` | `SimplePATGenJetFlatTableProducer` |
| Configuration fragments | `cff` files in `python/` | `PhysicsTools.PatGenJet.patGenJets_cff` (modules, task `patGenJetsTaskModeA`), `PhysicsTools.PatGenJet.patGenJetTables_cff` (NanoAOD tables) |
| Generated cfi | from `fillDescriptions` | `patGenJetProducer_cfi`, `patGenJetSelector_cfi`, `patGenJetSubstructurePacker_cfi`, `simplePATGenJetFlatTableProducer_cfi` |
| Tool function | `ConfigToolBase` instance | `PhysicsTools.PatGenJet.patGenJetTools.addPatGenJetCollection` |
| Customisation functions | cmsDriver `--customise` functions | `customisePatGenJetsMiniAOD`, `customisePatGenJetsNano` in `PhysicsTools.PatGenJet.customisePatGenJets` |
| NanoAOD tables | `FlatTable` names | `PatGenJet` (AK4), `PatGenJetAK8`, `PatSubGenJetAK8` (modules `patGenJetAK4Table`, `patGenJetAK8Table`, `patSubGenJetAK8Table`) |

It is opt-in: `slimmedGenJets`, `slimmedGenJetsAK8` and the existing NanoAOD tables `GenJet`, `GenJetAK8`, `SubGenJetAK8` are not changed or replaced.
No existing CMSSW package is edited.

## 2. What you get that today's generator jets do not give

Today's generator jets are plain `reco::GenJet`: no user data, flavour and subjets in separate collections, no substructure.

| Gain | Detail |
|---|---|
| Flavour on the jet | `partonFlavour()`, `hadronFlavour()`, `jetFlavourInfo()` (B and C hadron lists), from `JetFlavourClustering` on generator jets. AK8 and subjet flavour are new |
| Subjets on the jet | `subjets('SoftDropPuppi')`: `edm::Ptr<pat::GenJet>` to the soft-drop subjets, `nSubjetCollections()`, `hasSubjets(label)` |
| Substructure as user data | `userFloat(label)` with the same labels as the reco AK8 jets: `NjettinessAK8Puppi:tau1` to `:tau4`, `ak8PFJetsPuppiSoftDropMass`, `nb1AK8PuppiSoftDrop:ecfN2`, `:ecfN3`, `nb2AK8PuppiSoftDrop:ecfN2`, `:ecfN3` (fat jets); `NjettinessAK8Subjets:tau1..4`, `nb1AK8PuppiSoftDropSubjets:ecfN2/N3`, `nb2AK8PuppiSoftDropSubjets:ecfN2/N3` (subjets). `groomedMass('SoftDropPuppi')` is the mass of the subjet sum |
| Every `pat::Jet` method | the whole public interface exists (section 3), so code and string expressions written for `pat::Jet` compile and run on a generator jet |
| Every NanoAOD jet column | `PatGenJet`, `PatGenJetAK8`, `PatSubGenJetAK8` have all columns of the `Jet`, `FatJet`, `SubJet` tables (same name, type, precision, expression; the column sets are copied from the running process) |
| Energy fractions and counts in NanoAOD | `chHEF`, `neHEF`, `chEmEF`, `neEmEF`, `muEF`, `chMultiplicity`, `neMultiplicity`, `nConstituents`, plus `genEmEF`, `genHadEF`, `genInvisibleEF`, `genAuxiliaryEF` (`reco::GenJet::Specific`), in all three tables; `nBHadrons`, `nCHadrons` in `PatGenJet` and `PatGenJetAK8`, `partonFlavour` in `PatGenJetAK8` |
| Full-precision pt | `pt` is written without mantissa reduction in the three tables |

Plots (ttbar, Pythia 8 CP5, 1000 events, from `test/make_plots.py`; label "Private work"), made with the plotting tools in `test/` (section 6):

| File | Shows |
|---|---|
| `closure_ak4_pt`, `closure_ak4_eta`, `closure_ak8_pt`, `closure_ak8_mass`, `closure_subjet_pt` | new table against today's `GenJet`, `GenJetAK8`, `SubGenJetAK8` with a ratio panel: identical jet by jet |
| `ak8_softdrop_mass`, `ak8_nsubjettiness_ratios`, `ak8_energy_correlators` | substructure that did not exist on generator jets |
| `ak4_energy_fractions`, `constituent_counts` | PF-named energy fractions and constituent counts (AK4, AK8) |
| `ak4_flavour_composition` | b, c, light fractions against jet pt |
| `truth_ak4_btag`, `truth_ak8_classes` | truth tagger values: b-tag against hadron flavour; AK8 truth classes against pt |
| `all_new_branches.pdf` (+ `_summary.txt`) | every one of the 190 new branches, one histogram each, offline generator sample; the summary lists constant and NaN branches |
| `all_new_branches_reco1k.pdf` | the same on 1,000 fully simulated events |
| `gen_vs_reco/` | gen against reco, see section 6 |

## 3. How each `pat::Jet` member behaves on a gen jet

Rule: every public member of `pat::Jet` exists with the same name and signature (types adapted from `reco::PFJet`/`pat::Jet` to `reco::GenJet`/`pat::GenJet`). Where `pat::Jet` returns a value for missing data, the same value is returned. Where `pat::Jet` throws, a neutral value is returned. The generic PAT lookups keep throwing.

| Group | Members | On `pat::GenJet` |
|---|---|---|
| Kinematics, area, constituents | `p4()`, `jetArea()`, `daughter*`, `numberOfDaughters()`, `n90()`, `n60()` | real values. Constituents are `pat::PackedGenParticle` |
| Generator specific part | `emEnergy()`, `hadEnergy()`, `invisibleEnergy()`, `auxiliaryEnergy()`, `chargedEmMultiplicity()`, `neutralEmMultiplicity()` | real (inherited from `reco::GenJet`) |
| PF-named energies, fractions, multiplicities that `reco::GenJet::Specific` can fill | `chargedHadronEnergy`, `neutralHadronEnergy`, `chargedEmEnergy`, `neutralEmEnergy`, `muonEnergy`, their `*Fraction`, `photonEnergy`, `electronEnergy`, `chargedMuEnergy` (mapped), `chargedHadronMultiplicity`, `neutralHadronMultiplicity`, `muonMultiplicity`, `chargedMultiplicity`, `neutralMultiplicity`, `photonMultiplicity`, `electronMultiplicity` | real generator values. Mapping: photon = neutral EM, electron = charged EM, charged muon = muon; `chargedMultiplicity` = charged hadron + charged EM + muon; `neutralMultiplicity` = neutral hadron + neutral EM. Fractions divide by `energy()` |
| Calo-like | `energyFractionHadronic()`, `emEnergyFraction()` | `hadEnergy()/energy()`, `emEnergy()/energy()` (0 for zero energy) |
| Detector-only energies | `maxEInEmTowers`, `maxEInHadTowers`, `hadEnergyInHB/HO/HE/HF`, `emEnergyInEB/EE/HF`, `HFHadronEnergy`, `HFEMEnergy`, `hoEnergy` and their fractions, `towersArea`, `HFHadronMultiplicity`, `HFEMMultiplicity`, `elecMultiplicity` | 0 (`pat::Jet` throws) |
| Flavour | `partonFlavour`, `hadronFlavour`, `jetFlavourInfo` | real; 0 and default object if no flavour source was given |
| Subjets | `subjets(index)`, `subjets(label)`, `groomedMass(...)`, `hasSubjets`, `nSubjetCollections`, `subjetCollectionNames` | real. A missing index or label gives an empty collection (`pat::Jet` throws); `groomedMass` gives -1 for a missing label or an empty subjet list |
| User data | `userFloat`, `userInt`, `hasUserFloat`, `userFloatNames`, ... | real values for the substructure labels. Reco-only labels that NanoAOD reads (pileup ID discriminant, HF shower shape, muon subtraction) are written as placeholders: 0, except `muonSubtrRawPt/Eta/Phi`, which hold the jet's own pt, eta, phi so that the three `muonSubtr*` columns read 0. `userFloat`/`userInt` with an unknown key throw, as for any PAT object |
| JEC | `jecFactor(any level, flavour, set)`, `correctedJet(...)`, `correctedP4(...)`, `scaleEnergy` | the jet is its own uncorrected level: factor 1 at every level, `correctedJet` is a copy, `correctedP4` is `p4()`, `scaleEnergy` rescales `p4()` only. `availableJECSets()`, `availableJECLevels(...)` are empty, `jecSetsAvailable()` is false, `currentJECSet()` and `currentJECLevel()` are `"ERROR"` (as in `pat::Jet`), `currentJECFlavor()` is `NONE`. NanoAOD `rawFactor` is therefore 0 |
| Matching | `genJet()`, `genJetFwdRef()`, `genParton()` | `genJet()` returns the jet itself. `genJetFwdRef()` is a forward ref whose back ref points to the jet's own entry in the collection, so `genJetIdx` and similar columns hold the jet's own row |
| b-tag discriminators, tag infos | `bDiscriminator(label)`, `getPairDiscri`, `tagInfo*`, `hasTagInfo` | `bDiscriminator` returns -1000 for an unknown label, tag infos null/false, as `pat::Jet`. The tagger outputs that NanoAOD reads are filled with truth values (below) |
| Type-specific parts | `caloSpecific`, `jptSpecific`, `pfSpecific` and setters, `getCaloConstituent(s)`, `getPFConstituent(s)`, JPT track lists, `updateFwd*` out of range | static all-zero structs, null Ptrs, empty lists, setters do nothing (`pat::Jet` throws) |
| Type predicates | `isCaloJet`, `isJPTJet`, `isPFJet`, `hasPFSpecific`, `isBasicJet` | false, false, false, false, true. NanoAOD columns guarded by `isPFJet()` are un-guarded in `PatGenJetAK8` (they give the generator values); the HF ones (`hfHEF`, `hfEmEF`) stay -1 |
| Tracks, jet charge, jet ID | `associatedTracks`, `jetCharge`, `jetID` | empty, 0, all zero (never filled) |
| Other PAT members | trigger matches, efficiencies, resolutions (`getKinResolution`, `resol*`), overlaps | inherited from `PATObject`; `efficiency`, `getKinResolution`, `resol*` throw when missing, as for any PAT object |
| Constructors | `GenJet(const reco::GenJet&)`, from `RefToBase`/`Ptr` of `reco::GenJet` and `pat::GenJet` | implicit, as `pat::Jet`: `pat::GenJet j = genJet;` works |
| Scouting constructor | `pat::Jet(const Run3ScoutingPFJet&)` | not mirrored, the one documented exception |

### Tagger outputs

NanoAOD tagger columns of `Jet`, `FatJet` and `SubJet` read `bDiscriminator(label)` for network scores. On a generator jet the producer stores truth values under the reco tagger output names (the `bDiscriminator` labels), so every tagger column fills from the existing expressions. Four groups:

- AK4 flavour classes from hadron and parton flavour: b, bb (two or more B hadrons), c, gluon, light.
- Tau, electron and muon outputs from matched generator taus and leptons inside the jet.
- AK8 resonance classes (top, W, Z, Higgs and their decay modes; QCD with 0, 1 or 2 heavy-flavour hadrons) from generator resonances whose decay products all lie within ΔR < 0.8 of the jet.
- Regressions set to "no correction": factor 1, resolution 0.

Values are one-hot: 1 for the true class, 0 for every other. Every tagger column description reads "[truth-based on gen jets]".

Rules:

| group | labels | truth rule |
|---|---|---|
| AK4 lepton classes (first match wins) | PNet central and UParT `probtau{p,m}{1,3}h{0,1,2}p`, `probmu`, `probele` | generator hadronic tau (last copy, no e/mu daughter) whose visible momentum is within ΔR < 0.4: charge p/m, prongs 1 or 3 (charged visible descendants, ≥ 3 → 3), π0 count capped at 2 (1h) or 1 (3h); else a prompt (`isPrompt` or `isDirectPromptTauDecayProduct`) status-1 muon, then electron, within ΔR < 0.4 |
| AK4 flavour | DeepFlavour `probb/bb/lepb/c/uds/g`; PNet central `probb/c/uds/g`; UParT `probb/bb/lepb/c/s/u/d/g`; PNet forward `probq/g` | hadronFlavour 5: `bb` if ≥ 2 B hadrons, else `lepb` if a non-prompt e/mu with a B-hadron ancestor is within ΔR < 0.4, else `b` (PNet: all three → `probb`); 4: `c`; 0: by abs(partonFlavour) 21 `g`, 3 `s`, 1 `d`, anything else (2, 0, 4, 5) `u` (DeepFlavour/PNet: s/u/d → `uds`; PNet forward: everything but g → `q`). DeepFlavour and PNet forward have no lepton classes, so tau/lepton jets keep their flavour class there |
| AK8 resonance | GlobalParT `probX*`, `probTop*`, `probQCD`; PNet (legacy) `probT*`, `probW*`, `probZ*`, `probH*`; mass-decorrelated PNet `probXbb/cc/qq`; PNet from MiniAOD `probH*` | generator t, W, Z, H (last copy); decay products followed through W/Z/tau copies, neutrinos ignored. Top: b and both W quarks within ΔR < 0.8 → `TopbWqq` (PNet `Tbcq` if a c quark, else `Tbqq`); b and one W quark → `TopbWq` (`Tbc`/`Tbq`); b and the W lepton → `TopbWev`, `TopbWmv`, `TopbWtauhv` (leptonic taus by their lepton; PNet `Tbel`, `Tbmu`, `Tbta`). W/Z/H → two partons, both inside: bb → `Xbb` (Z `Zbb`, H `Hbb`), cc → `Xcc`, c + light → `Xcs` (W `Wcq`), light qq → `Xqq` (W `Wqq`, Z `Zqq`), gg → PNet-MiniAOD `Hgg` only, τhτh/τhτe/τhτμ → `Xtauhtau{h,e,m}` / `Htt`, `Hte`, `Htm`. H→VV: 4 quarks inside → `XWW4q` (PNet `Hqqqq`), exactly 3 of 4 → `XWW3q`, 2 quarks + e/μ → `XWWqq{e,m}v`. Mass-decorrelated PNet and PNet-MiniAOD use the generic X/H classes (cs → cc class). Several matches: top > H > Z > W, then nearest resonance. A mode a tagger has no class for falls back to that tagger's QCD class |
| AK8 QCD | GlobalParT `probQCD`; PNet and MD `probQCDbb/cc/b/c/others`; PNet-MiniAOD `probQCD0hf/1hf/2hf` | no resonance class: nB ≥ 2 `bb`, nB = 1 `b`, nC ≥ 2 `cc`, nC = 1 `c`, else `others`; hf = nB + nC capped at 2 |
| `probWithMass{Top,W,Z}vsQCD` | GlobalParT | 1 for a top class / hadronic W / hadronic Z, else 0 |
| regressions | `ptcorr`, `ptnu`, `masscorr`, `massCorrX2p`, `massCorrGeneric` → 1; `ptreshigh`, `ptreslow` → 0; `pfParticleNetMassRegressionJetTags:mass` → the jet mass | "no correction" |

Consequences of the unchanged expressions, by design: guarded columns give -1 where the truth value is 0 (e.g. `btagPNetB` of a non-b jet, `UParTAK4RegPtRawRes` because ptreshigh + ptreslow = 0); `btagPNetCvNotB` and `btagUParTAK4CvNotB` of a truth b jet are 0/(1-1) = **NaN** (567 of 2356 AK4 jets in 300 ttbar events).

Two consequences of one-hot values with the unchanged reco expressions:
- `btagPNetCvNotB` and `btagUParTAK4CvNotB` are NaN on truth b jets (the expression divides by `1 - probb` = 0).
- Columns whose reco expression is guarded by `score > 0` return -1 where the truth value is 0 (for example `btagPNetB` on a non-b jet); `UParTAK4RegPtRawRes` is always -1.

## 4. How to use it

All three ways need a MC job (generator information) and the opt-in: nothing of the package is imported into a process unless you call it.

### 4a. MiniAOD and NANO with cmsDriver (the validated recipe)

Run in a CMSSW area that has this package built (`cmsenv` first). Step 3 of workflow 11634.0, with the customisations:

```bash
cmsDriver.py step3 \
  -s RAW2DIGI,L1Reco,RECO,RECOSIM,PAT,NANO \
  --conditions auto:phase1_2022_realistic --geometry DB:Extended --era Run3 --mc \
  --datatier GEN-SIM-RECO,MINIAODSIM,NANOAODSIM \
  --eventcontent RECOSIM,MINIAODSIM,NANOAODSIM \
  -n 10 --nThreads 1 \
  --filein file:step2.root --fileout file:step3.root \
  --python_filename step3_v1_cfg.py \
  --customise_commands "import PhysicsTools.PatGenJet.customisePatGenJets as pgj; process = pgj.customisePatGenJetsMiniAOD(process, keepInOutput=True)\nimport PhysicsTools.PatGenJet.customisePatGenJets as pgj; process = pgj.customisePatGenJetsNano(process)\nfor m in (process.patGenJetsAK4, process.patGenJetsAK8, process.patGenJetsAK8SoftDropSubjets): m.daughterOrder = 'key'\nprocess.MINIAODSIMoutput.outputCommands.extend(['keep *_slimmedGenJets*_*_*', 'keep *_ak8GenJetsNoNuSoftDrop_*_*', 'keep *_*ForPatGenJets_*_*', 'keep *_patGenJets*_*_*', 'keep *_packedGenParticles_*_*'])" \
  --no_exec
cmsRun step3_v1_cfg.py
```

What the lines do:

| Call | Effect |
|---|---|
| `customisePatGenJetsMiniAOD(process, keepInOutput=False)` | adds `patGenJetsTaskModeA` to the PAT tools task. With `keepInOutput=True` it adds `keep *_patGenJetsAK4_*_*` and `keep *_packedPatGenJetsAK8_*_*` to the MiniAOD output module (the one that keeps `slimmedGenJets`). Raises an error if the process lacks `ak8GenJetsNoNuSoftDrop`, `ak4GenJetFlavourInfos`, `selectedHadronsAndPartonsForGenJetsFlavourInfos` or `packedGenParticles` |
| `customisePatGenJetsNano(process)` | adds the three tables to `nanoTableTaskFS` and DQM entries to `nanoDQM`. Call it after the MiniAOD customisation and after the standard NANO customisations, because the column sets are copied from the tables of the running process. On data it prints a message and adds nothing |
| `m.daughterOrder = 'key'` | only for the V1 comparison: reproduces the constituent order of `slimmedGenJets`. The default is `'pt'` (input order, pt-ordered). Leave it out for normal use |
| extra `keep` lines | only what `test/v1_compare.py` reads. Not needed for normal use |

Minimal form, with default arguments (no `keepInOutput`): `--customise PhysicsTools/PatGenJet/customisePatGenJets.customisePatGenJetsMiniAOD,PhysicsTools/PatGenJet/customisePatGenJets.customisePatGenJetsNano`.
When the MiniAOD and NANO steps run in the same job (as above) the tables read the products of the same event; for a separate NANO job from a MiniAOD file, the MiniAOD file must have been written with `keepInOutput=True` (building from MiniAOD alone is not supported).

### 4b. In your own configuration: `addPatGenJetCollection`

```python
from PhysicsTools.PatGenJet.patGenJetTools import addPatGenJetCollection
addPatGenJetCollection(process, labelName='AK8Puppi', genJetSource='ak8GenJetsNoNu', algo='AK', rParam=0.8,
                       groomedSource='ak8GenJetsNoNuSoftDrop')
```

Signature (all arguments keyword-capable; defaults in brackets):

`addPatGenJetCollection(process, labelName, genJetSource, algo='AK', rParam=0.4, groomedSource=None, subjetSource=None, substructure=True, flavour=True, packedGenParticles='packedGenParticles', postfix='', groomedName='SoftDrop', subjetLabel='SoftDropPuppi')`

| Argument | Meaning |
|---|---|
| `labelName` | required; collection `patGenJets<labelName><postfix>` |
| `genJetSource` | required; `View<reco::GenJet>` input (for example `ak4GenJetsNoNu`, `ak8GenJetsNoNu`) |
| `algo`, `rParam` | `'AK'`, `'CA'` or `'KT'` and the distance parameter; used by the flavour clustering and as N-subjettiness `R0` |
| `groomedSource` | groomed jets (for example `ak8GenJetsNoNuSoftDrop`). If given: subjets, selector and packer are added, and the fat jets carry `subjets(subjetLabel)` |
| `subjetSource` | default `<groomedSource>:SubJets` |
| `substructure` | N-subjettiness (jets, subjets), ECFs, groomed mass |
| `flavour` | `JetFlavourClustering`; creates `selectedHadronsAndPartonsForGenJetsFlavourInfos` if the process has none |
| `packedGenParticles` | constituents re-keyed through this association; empty label = no re-keying |
| `postfix`, `groomedName`, `subjetLabel` | label postfix, name of the grooming in module labels, label of the subjets on the fat jet |

Without `groomedSource` only `patGenJets<labelName><postfix>` (and its substructure and flavour modules) are made. With it, also `selectedPatGenJets<labelName><postfix>` (cut `''`: set `.cut`, `.cutLoose`, `.nLoose` afterwards) and `packedPatGenJets<labelName><postfix>` with the subjets in instance `SubJets`. The tool only adds modules, never modifies existing ones, and raises `ValueError` on a label clash. It does not add NanoAOD tables.

Modules directly, from the fragment (the chain used by the customisation):

```python
process.load('PhysicsTools.PatGenJet.patGenJets_cff')
process.p = cms.Path(process.patGenJetsTaskModeA)   # or add the task to the PAT tools task
process.patGenJetsAK4.addTagInfos = False            # any parameter of the cfi can be changed
```

### 4c. Reading it

C++ / FWLite, with the `edm::EDGetToken` of your analyser or in `Events`:

```cpp
#include "PhysicsTools/PatGenJet/interface/GenJet.h"
edm::EDGetTokenT<std::vector<pat::GenJet>> tok = consumes<std::vector<pat::GenJet>>(edm::InputTag("patGenJetsAK4"));
// ...
for (const pat::GenJet& j : iEvent.get(tok)) {
  j.hadronFlavour(); j.userFloat("NjettinessAK8Puppi:tau1");   // as pat::Jet
  const pat::GenJet& ref = j;                                   // use a const reference to call daughter(i), see section 7
}
```

Add `<use name="PhysicsTools/PatGenJet"/>` to your `BuildFile.xml`. Python (`test/fwlite_read.py` is a complete example; `--label` takes the module label):

```python
import ROOT
ROOT.gSystem.Load("libFWCoreFWLite.so"); ROOT.FWLiteEnabler.enable()
ROOT.gSystem.Load("libPhysicsToolsPatGenJet.so")
from DataFormats.FWLite import Events, Handle
h = Handle("std::vector<pat::GenJet>")
for ev in Events("step3_inMINIAODSIM.root"):
    ev.getByLabel("packedPatGenJetsAK8", h)
    for j in h.product():
        print(j.pt(), j.userFloat("NjettinessAK8Puppi:tau1"), j.groomedMass("SoftDropPuppi"))
```

`using namespace pat; using namespace reco;` makes an unqualified `GenJet` ambiguous; write `pat::GenJet`.

NanoAOD: branches `PatGenJet_*`, `PatGenJetAK8_*`, `PatSubGenJetAK8_*`, counts `nPatGenJet`, `nPatGenJetAK8`, `nPatSubGenJetAK8`: 190 branches in total (74 + 85 + 28 columns and 3 counts), against 23 in today's gen tables; 20 columns repeat today's information, 167 are new, 88 of them truth tagger columns. On a ttbar sample 70 branches are constant (about 55 of them on any sample: fixed reco-only quantities; plus `area`, which is 0, and truth classes absent from ttbar). Columns have the names of `Jet_*`, `FatJet_*`, `SubJet_*`. Rows: `PatGenJet` is `patGenJetsAK4` with `pt > 10`; `PatGenJetAK8` is `packedPatGenJetsAK8` with `pt > 100.`; `PatSubGenJetAK8` is `packedPatGenJetsAK8:SubJets`, no cut. Row order equals today's `GenJet`, `GenJetAK8`, `SubGenJetAK8`, so `PatGenJetAK8_subJetIdx1/2` index `PatSubGenJetAK8`.

## 5. Inputs and outputs

Inputs, all made by a standard Run 3 MC job with the MiniAOD step (mode A, the only mode):

| Input | Made by | Used for |
|---|---|---|
| `ak4GenJetsNoNu`, `ak8GenJetsNoNu` | RECO (gen jets, neutrino-free) | the jets |
| `ak8GenJetsNoNuSoftDrop`, and instance `SubJets` | MiniAOD (`applySubstructure_cff`) | groomed jets, subjets |
| `ak4GenJetFlavourInfos`, `selectedHadronsAndPartonsForGenJetsFlavourInfos` | MiniAOD (`miniAOD_tools`) | flavour |
| `packedGenParticles` (+ its `Association` to `genParticles`) | MiniAOD | constituents of the new jets |
| `NjettinessAK8PuppiForPatGenJets`, `nb1/nb2AK8PuppiSoftDrop*ForPatGenJets`, `ak8PFJetsPuppiSoftDropMassForPatGenJets`, `ak8GenJetFlavourInfosForPatGenJets` | this package (`patGenJetsTaskModeA`) | value maps and flavour; the postfix `ForPatGenJets` is stripped from the user-float labels |
| `jetPuppiTable`, `fatJetTable`, `subJetTable` (+ MC tables) | NanoAOD | column sets of the new tables |

Products (labels as in `patGenJets_cff`; `PROCESS` is your process name):

| Label | Type | Content |
|---|---|---|
| `patGenJetsAK4` | `std::vector<pat::GenJet>` | AK4 gen jets, flavour, placeholders; also instance `tagInfos` (`OwnVector<reco::BaseTagInfo>`) |
| `patGenJetsAK8` | `std::vector<pat::GenJet>` | all AK8 gen jets with tau1-4 and soft-drop mass (intermediate) |
| `selectedPatGenJetsAK8` | `std::vector<pat::GenJet>` | AK8 selection of `slimmedGenJetsAK8` (`pt > 80`, `cutLoose pt > 10.`, `nLoose 3`); also `tagInfos` and an `Association` old to new |
| `patGenJetsAK8SoftDropSubjets` | `std::vector<pat::GenJet>` | soft-drop subjets with ECFs and tau (intermediate) |
| `packedPatGenJetsAK8` | `std::vector<pat::GenJet>` | the AK8 jets with subjets and ECFs; instance `SubJets`: the subjets (what `PatSubGenJetAK8` reads); an `Association` old to new |
| `patGenJetAK4Table`, `patGenJetAK8Table`, `patSubGenJetAK8Table` | `nanoaod::FlatTable` | NanoAOD tables |

The producer parameters (see `PATGenJetProducer::fillDescriptions`, or `edmPluginHelp -p PATGenJetProducer`) follow those of `PATJetProducer`: `jetSource`, `packedGenParticles`, `daughterOrder` (`"pt"` or `"key"`), `getJetMCFlavour`, `JetFlavourInfoSource`, `addJetFlavourInfo` (default True), `addTagInfos`, `addBTagInfo`, `addDiscriminators`, `discriminatorSources`, `addJetCharge`, `userData`, `placeholderUserFloats`, `placeholderUserInts`, `placeholderPtEtaPhiUserFloats`, efficiencies and resolutions. Dropped on purpose: embedding of calo towers and PF candidates, JEC factors, gen-jet match (replaced by the self link), jet ID, associated tracks. The selector (`PATGenJetSelector`) takes `src`, `cut`, `cutLoose`, `filter`, `nLoose` and the slimming options `dropSpecific`, `dropDaughters`, `clearDaughters`, `dropJetVars`, `modifyJets`, `modifierConfig` (all off by default). The packer (`PATGenJetSubstructurePacker`) takes `jetSrc`, `distMax`, `fixDaughters`, `labelPostfixesToStrip` and a VPSet `algos` (`label`, `groomedJets`, `subjets`, `groomedUserFloats`, `subjetInstance`).

On real data the producer switches the MC inputs off; the NanoAOD customisation adds nothing for data.

## 6. Validation

Unit tests run offline; the V1 job is run by a person on lxplus.

**V1: equivalence with today's collections** (workflow 11634.0, ttbar, 10 events, scripts `test/v1_compare.py` and `test/v1_compare_nano.py`). Result: every row PASS, no DIFF.

| Row | Comparison | Result |
|---|---|---|
| 1 | `patGenJetsAK4` against `slimmedGenJets` (pt, eta, phi, mass, vertex, area, Specific bitwise; constituent lists) | PASS, 250 checks |
| 2 | flavour against `slimmedGenJetsFlavourInfos` | PASS, 240 |
| 3 | `packedPatGenJetsAK8` against `slimmedGenJetsAK8` | PASS, 42 |
| 4 | `packedPatGenJetsAK8:SubJets` against `slimmedGenJetsAK8SoftDropSubJets` | PASS, 42 |
| 5 | subjet links of the fat jets | PASS, 32 |
| 6 | user floats against the value maps (bitwise) | PASS, 480 |
| 7 | self links | PASS, 304 |
| 8 | NanoAOD: `PatGenJet` against `GenJet`, `PatGenJetAK8` against `GenJetAK8`, `PatSubGenJetAK8` against `SubGenJetAK8` | PASS, 80, 16 and 32 rows |
| 9 | `groomedMass('SoftDropPuppi')` against the mass of the matched groomed jet | PASS, 32 |

Known deviations (reported, do not fail a row):

| ID | Deviation | Explanation | Seen |
|---|---|---|---|
| K1 | a constituent without packed partner | `slimmedGenJets` stores a null Ptr, this package drops it, so `nConstituents` can differ by one | 1 jet |
| K2 | `Specific` of soft-drop subjets | MiniAOD drops it for subjets (`dropSpecific`); here it is kept, so the subjet energies and multiplicities are real | 32 subjets, 75 values |
| K3 | `groomedMass` against the groomed jet mass | the sum of subjet four-momenta against fastjet's mass; tolerance is relative 1e-6 or absolute 1e-5 GeV (light groomed jets lose relative precision) | largest relative difference 1.0e-6 (passes through the absolute bound) |
| K4 | AK8 flavour columns of the NANO tables | clustered on full constituents here, on packed ones in `GenJetAK8` | none seen |

The closure plots of `test/make_plots.py` show the closure jet by jet on a 1000-event ttbar sample.

**Unit tests** (`scram b runtests`, all offline):

| Test | Covers |
|---|---|
| `testGenJet` | construction, copy, every group of section 3 (JEC, b-tag, specific structs, neutral energies, JPT, constituent getters, `updateFwd*`, PF-named members against their formulas, `n90`/`n60`, subjets, `genJet()` and self link, flavour, generic PAT lookups still throw, zero-energy jets), ROOT write/read of `std::vector<pat::GenJet>`, class-version read-back |
| `testGenJetExpressions` | every NanoAOD jet expression (`test/genjet_nano_expressions.txt`) evaluates through `StringObjectFunction<pat::GenJet>` |
| `testGenJetMemberParity` | every `pat::Jet` member name exists on `pat::GenJet` (ROOT reflection of the built dictionary) |
| `testGenJetClassVersion` | `edmCheckClassVersion` on the library and `src/classes_def.xml` |
| `testPATGenJetProducer` | sorting, self links, re-keying, `daughterOrder`, dropped constituent without partner, user data with stripped postfix and placeholders, flavour inputs, MC off on data, configuration checks |
| `testPATGenJetSelector` | same selection as `PATGenJetSlimmer`, self links and association, `filter`, slimming options |
| `testPATGenJetPacker` | subjet output, matching, ECFs, `groomedMass`, `distMax`, `fixDaughters`, duplicate subjet instance rejected |
| `testPatGenJetCffLoad` | the cffs load and every module is built against its `fillDescriptions` |
| `testPatGenJetChain` | 20 Pythia events: chain, tables against today's gen tables, FWLite read, tool against `patGenJets_cff` |
| `testPatGenJetColumns` | every `Jet`/`FatJet`/`SubJet` column exists in the matching table (same name, type, precision; documented changes only) |
| `testPatGenJetExpansion` | without a call the standard MiniAOD and NANO configurations are unchanged; with the customisations only the documented modules and `keep` lines are added |

**V1 repeated with the truth taggers** (same workflow, 10 events): every row PASS.

**V2: nothing else changes**: the test-run kit ran the quick set (7 workflows) with the plain release and with an area where this package is built but not used. 32 output files, 290 events, 10,366 jet-product contents: IDENTICAL.

**V3: size and timing** (workflow 11634.0, 100 events, step 3 run twice from the same input, without and with the customisations; script `test/v3_report.py`):

| | without | with | extra |
|---|---|---|---|
| MiniAOD (compressed) | 58.3 kB/event | 64.1 kB/event | +5.8 kB (+10 %) |
| NanoAOD (compressed) | 5.2 kB/event | 6.2 kB/event | +1.0 kB (+19 %) |
| Time | | | +0.034 s/event (2.7 % of the job), mostly N-subjettiness and energy correlators |

Truth taggers on the same 100 events: `btagDeepFlavB` is 1 for 197 of 197 truth b jets and 0 for 612 of 612 other jets.

**Gen against reco** (workflow 11634.0, 1,000 events, full simulation to NanoAOD with `customisePatGenJetsNano`; script `test/compare_gen_reco.py`, which also writes every shared column to `gen_vs_reco_all.pdf`):

| Check | Result |
|---|---|
| Reco AK4 jets with a matched `PatGenJet` (`Jet_genJetIdx`) | 99.3 % |
| Response pt(reco)/pt(gen), median (half-width of 68 %) | 1.008 (0.168) for gen pt 20-50 GeV, 1.016 (0.125) for 50-100 GeV, 1.011 (0.099) above 100 GeV |
| Reco b-tag score > 0.5 by truth flavour of the matched gen jet (b / c / light; illustration, not a working point) | DeepJet 0.74 / 0.07 / 0.003; ParticleNet 0.75 / 0.04 / 0.005; UParT 0.71 / 0.02 / 0.002 |
| Constituents per AK4 jet, mean | 15.1 gen, 13.0 reco PUPPI |

The reco tables keep AK4 jets above 15 GeV and AK8 jets above 170 GeV, the gen tables above 10 and 100 GeV: inclusive distributions differ at low pt and in the AK8 jet mix (the reco soft-drop mass shows the W peak more strongly); matched comparisons are not affected.

Plotting tools: `test/make_plots.py` (closure and new quantities), `test/plot_all_branches.py NANO.root OUT.pdf` (one histogram per new branch and a summary), `test/compare_gen_reco.py NANO.root OUTDIR` (needs a NanoAOD with both reco and `PatGenJet` tables).

## 7. Known limitations and deviations

| Point | Detail |
|---|---|
| Constituents without packed partner are dropped | `PATGenJetProducer` re-keys through `packedGenParticles`; a constituent that has no packed partner is dropped (the release slimmer writes a null Ptr). `nConstituents` can then differ from `slimmedGenJets` (K1) |
| Jet `area` is 0 | the standard generator-jet clustering does not compute jet areas, so `area` is 0 for every gen jet |
| `getGenConstituent(s)` | inherited from `reco::GenJet`; they throw `Invalid Constituent` on packed constituents. Use `daughterPtr(i)` |
| Subjet `Specific` is filled | differs from `slimmedGenJetsAK8SoftDropSubJets` by design (K2) |
| `groomedMass` tolerance | sum of subjets against the groomed jet mass agrees to 1e-6 relative or 1e-5 GeV absolute, not bitwise (K3) |
| AK8 flavour | clustered on full constituents; may differ from today's `GenJetAK8`; none seen in V1 |
| Mode B dropped | no build from MiniAOD alone; a NANO-only job needs a MiniAOD written with `keepInOutput=True` |
| `daughter(i)` on a non-const jet | returns `nullptr` (the non-const `LeafCandidate` overload is chosen), as for `pat::Jet`; use a const reference or `daughterPtr(i)` |
| PF fractions of a zero-energy jet | NaN, as in `pat::Jet`; `energyFractionHadronic()` and `emEnergyFraction()` return 0 |
| Name hiding | `PATObject::genParticle(size_t)` hides the static `reco::GenJet::genParticle(const reco::Candidate*)`; call `reco::GenJet::genParticle(c)` |
| `genJetFwdRef()` type | `edm::FwdRef<pat::GenJetCollection>`, not `<reco::GenJetCollection>`; code that spells the reco type does not compile. The self link is only valid in the collection written by this package's modules (a generic copy module keeps links into its parent), and tables read a pt-sorted collection with only a pt cut, so key = row |
| Unqualified `GenJet` | ambiguous with `using namespace pat; using namespace reco;` |
| Values with no generator meaning | section 3: neutral values (JEC 1, energies 0, `groomedMass` -1); `rawFactor` is 0; HF columns -1; reco-only user data are 0 placeholders |
| Truth taggers are not network outputs | one-hot ideal values |
| Nothing kept in MiniAOD by default | `keepInOutput=False`. Measured cost when kept (V3, 100 ttbar events): MiniAOD +5.8 kB/event (+10 %), NanoAOD +1.0 kB/event (+19 %) |
| Slimming is off | `dropSpecific`, `dropDaughters` and the others are available in the selector and switched off; whether to use them is open (V3 sizes above) |
| Not in a release | a new package; no existing package (for example `DataFormats/PatCandidates`) is changed, so the code shared with `pat::Jet` is copied here |
| Gen jets | only the neutrino-free AK4 and AK8 jets of MiniAOD; no heavy-ion generator jets |

## 8. Status and open points

| Item | State |
|---|---|
| Data format, modules, configuration, NanoAOD tables, truth taggers | done |
| Validation: equivalence, no side effects, size and timing, gen against reco | done (section 6) |
| Size of the NanoAOD tables | open: about 55 branches are constant on any sample and could be dropped; precision could be lowered |
| jetToolbox support (a `pat::GenJet` collection for each reco collection, with `addPatGenJetCollection`) | open: branch and release to target |
| Integration into a CMSSW release | open: placement (for example `DataFormats/PatCandidates`, `PhysicsTools/PatAlgos`, `PhysicsTools/NanoAOD`) and a data-format review |
