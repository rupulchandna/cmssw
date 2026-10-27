// Unit tests for pat::GenJet.
// Hand-made objects only (input "H"): no event data, no conditions.

#include "PhysicsTools/PatGenJet/interface/GenJet.h"

#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/Provenance/interface/ProductID.h"
#include "DataFormats/Common/interface/OrphanHandle.h"
#include "DataFormats/Common/interface/RefToBase.h"
#include "DataFormats/PatCandidates/interface/PackedGenParticle.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "TClass.h"
#include "TFile.h"
#include "TList.h"
#include "TStreamerInfo.h"
#include "TTree.h"

#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// DataFormats/Math/interface/Error.h defines a STATIC_CHECK macro that clashes with Catch2
#undef STATIC_CHECK
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

using Catch::Approx;

namespace {
  // Access to the transient caches (protected) for the "caches reset after read" check.
  struct Peek : pat::GenJet {
    static bool daughtersCached(const pat::GenJet& j) { return (j.*(&Peek::daughtersTemp_)).isSet(); }
    static bool towersCached(const pat::GenJet& j) { return (j.*(&Peek::caloTowersTemp_)).isSet(); }
    static bool pfCached(const pat::GenJet& j) { return (j.*(&Peek::pfCandidatesTemp_)).isSet(); }
  };

  const edm::ProductID kConstituentsPID(1, 1);
  const edm::ProductID kGenJetsPID(1, 2);
  const edm::ProductID kSubjetsPID(1, 3);
  const edm::ProductID kBHadronPID(1, 4);

  reco::GenJet::Specific makeSpecific() {
    reco::GenJet::Specific s;
    s.m_EmEnergy = 30.f;
    s.m_HadEnergy = 60.f;
    s.m_InvisibleEnergy = 5.f;
    s.m_AuxiliaryEnergy = 1.f;
    s.m_ChargedHadronEnergy = 40.f;
    s.m_NeutralHadronEnergy = 20.f;
    s.m_ChargedEmEnergy = 4.f;
    s.m_NeutralEmEnergy = 26.f;
    s.m_MuonEnergy = 3.f;
    s.m_ChargedHadronMultiplicity = 7;
    s.m_NeutralHadronMultiplicity = 2;
    s.m_ChargedEmMultiplicity = 1;
    s.m_NeutralEmMultiplicity = 5;
    s.m_MuonMultiplicity = 1;
    return s;
  }

  // constituents live here so that the CandidatePtrs of the test jets stay valid
  std::vector<reco::GenParticle> const& constituents() {
    static const std::vector<reco::GenParticle> parts = [] {
      std::vector<reco::GenParticle> v;
      const double energies[] = {50., 30., 15., 5.};
      for (double e : energies) {
        reco::Particle::LorentzVector p4(e, 0., 0., e);  // transverse: Et = E, as nCarrying uses Et
        v.emplace_back(0, p4, reco::Particle::Point(0, 0, 0), 22, 1, true);
      }
      return v;
    }();
    return parts;
  }

  reco::GenJet makeRecoGenJet(double pt = 50., double eta = 1.0, double phi = 0.5, double mass = 10.) {
    reco::Particle::PolarLorentzVector pp4(pt, eta, phi, mass);
    reco::Particle::LorentzVector p4(pp4.px(), pp4.py(), pp4.pz(), pp4.energy());
    reco::GenJet rj(p4, reco::Particle::Point(0.1, 0.2, 0.3), makeSpecific());
    rj.setJetArea(0.5);
    auto const& parts = constituents();
    for (size_t i = 0; i < parts.size(); ++i)
      rj.addDaughter(reco::CandidatePtr(kConstituentsPID, &parts[i], i));
    return rj;
  }

  bool isZero(const pat::CaloSpecific& s) {
    return s.mMaxEInEmTowers == 0 && s.mMaxEInHadTowers == 0 && s.mHadEnergyInHB == 0 && s.mEmEnergyInEB == 0 &&
           s.mTowersArea == 0 && s.mEnergyFractionHadronic == 0;
  }
  bool isZero(const pat::PFSpecific& s) {
    return s.mChargedHadronEnergy == 0 && s.mNeutralHadronEnergy == 0 && s.mPhotonEnergy == 0 &&
           s.mHFHadronEnergy == 0 && s.mChargedMultiplicity == 0 && s.mHOEnergy == 0;
  }
  bool isZero(const pat::JPTSpecific& s) {
    return s.mResponseOfChargedWithEff == 0 && s.mChargedHadronEnergy == 0 && s.pionsInVertexInCalo.empty() &&
           s.elecsOutVertexInCalo.empty();
  }
}  // namespace

// --------------------------------------------------------------------------------------------------
TEST_CASE("construction from reco::GenJet keeps kinematics and Specific", "[GenJet][construction]") {
  const reco::GenJet rj = makeRecoGenJet();
  static_assert(std::is_convertible_v<const reco::GenJet&, pat::GenJet>, "implicit constructor");
  pat::GenJet j = rj;  // implicit, as pat::Jet

  CHECK(j.p4() == rj.p4());
  CHECK(j.vertex() == rj.vertex());
  CHECK(j.jetArea() == Approx(0.5));
  CHECK(j.numberOfDaughters() == constituents().size());
  CHECK(j.emEnergy() == 30.f);
  CHECK(j.hadEnergy() == 60.f);
  CHECK(j.invisibleEnergy() == 5.f);
  CHECK(j.auxiliaryEnergy() == 1.f);
  CHECK(j.getSpecific().m_NeutralEmMultiplicity == 5);

  SECTION("Ptr and RefToBase constructors") {
    std::vector<reco::GenJet> coll{rj};
    edm::Ptr<reco::GenJet> ptr(kGenJetsPID, &coll[0], 0);
    pat::GenJet fromPtr = ptr;
    CHECK(fromPtr.p4() == rj.p4());
    CHECK(fromPtr.originalObjectRef().id() == kGenJetsPID);

    std::vector<pat::GenJet> pcoll{j};
    edm::Ptr<pat::GenJet> pptr(kGenJetsPID, &pcoll[0], 0);
    pat::GenJet fromPatPtr = pptr;
    CHECK(fromPatPtr.p4() == rj.p4());
    CHECK(fromPatPtr.originalObjectRef().key() == 0);
  }
}

TEST_CASE("jet type predicates", "[GenJet][type]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK_FALSE(j.isCaloJet());
  CHECK_FALSE(j.isJPTJet());
  CHECK_FALSE(j.isPFJet());
  CHECK_FALSE(j.hasPFSpecific());
  CHECK(j.isBasicJet());
  CHECK(j.isJet());
}

// ---------------------------------------------------------------- behaviour groups, one block per group
TEST_CASE("2.3 JEC: factor 1, copy, empty lists, ERROR, NONE", "[GenJet][2.3][JEC]") {
  pat::GenJet j(makeRecoGenJet());
  // row: jecFactor(any) = 1
  CHECK(j.jecFactor("Uncorrected") == 1.f);
  CHECK(j.jecFactor("L3Absolute", "none", "") == 1.f);
  CHECK(j.jecFactor("L5Flavor", "bottom", "otherSet") == 1.f);
  CHECK(j.jecFactor(0) == 1.f);
  CHECK(j.jecFactor(3, pat::JetCorrFactors::BOTTOM, 2) == 1.f);
  // row: correctedJet / correctedP4
  CHECK(j.correctedP4("Uncorrected") == j.p4());
  CHECK(j.correctedP4(0) == j.p4());
  CHECK(j.correctedJet("Uncorrected").p4() == j.p4());
  // row: availableJECLevels(int) empty
  CHECK(j.availableJECLevels(0).empty());
  CHECK(j.availableJECLevels(5).empty());
  // row: availableJECSets, availableJECLevels(string), jecSet(s)Available
  CHECK(j.availableJECSets().empty());
  CHECK(j.availableJECLevels(std::string("patJetCorrFactors")).empty());
  CHECK_FALSE(j.jecSetsAvailable());
  CHECK_FALSE(j.jecSetAvailable(std::string("patJetCorrFactors")));
  CHECK_FALSE(j.jecSetAvailable(0u));
  // row: currentJECSet / currentJECLevel
  CHECK(j.currentJECSet() == "ERROR");
  CHECK(j.currentJECLevel() == "ERROR");
  // row: currentJECFlavor defined NONE
  CHECK(j.currentJECFlavor() == pat::JetCorrFactors::NONE);
  // rawFactor column expression: 1 - jecFactor('Uncorrected') = 0
  CHECK(1.f - j.jecFactor("Uncorrected") == 0.f);
}

TEST_CASE("2.3 b tagging: -1000, empty, nullptr, false", "[GenJet][2.3][btag]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK(j.bDiscriminator("pfDeepFlavourJetTags:probb") == -1000.f);
  CHECK(j.getPairDiscri().empty());
  CHECK(j.tagInfoLabels().empty());
  CHECK(j.tagInfo("pfImpactParameter") == nullptr);
  CHECK_FALSE(j.hasTagInfo("pfImpactParameter"));
  CHECK(j.tagInfoCandIP() == nullptr);
  CHECK(j.tagInfoTrackIP() == nullptr);
  CHECK(j.tagInfoCandSoftLepton() == nullptr);
  CHECK(j.tagInfoSoftLepton() == nullptr);
  CHECK(j.tagInfoCandSecondaryVertex() == nullptr);
  CHECK(j.tagInfoSecondaryVertex() == nullptr);
  CHECK(j.tagInfoBoostedDoubleSV() == nullptr);
  CHECK(j.tagInfoPixelCluster("x") == nullptr);
  // stored values behave as in pat::Jet (last added wins)
  j.addBDiscriminatorPair({"myTagger", 0.25f});
  j.addBDiscriminatorPair({"myTagger", 0.75f});
  CHECK(j.bDiscriminator("myTagger") == 0.75f);
  CHECK(j.bDiscriminator("other") == -1000.f);
  CHECK(j.getPairDiscri().size() == 2);
}

TEST_CASE("2.3 specific structs: static all-zero, setters no-op", "[GenJet][2.3][specific]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK(isZero(j.caloSpecific()));
  CHECK(isZero(j.jptSpecific()));
  CHECK(isZero(j.pfSpecific()));
  pat::CaloSpecific cs;
  cs.mMaxEInEmTowers = 7.f;
  pat::PFSpecific ps;
  ps.mChargedHadronEnergy = 7.f;
  pat::JPTSpecific js;
  js.mResponseOfChargedWithEff = 7.f;
  j.setCaloSpecific(cs);
  j.setPFSpecific(ps);
  j.setJPTSpecific(js);
  CHECK(isZero(j.caloSpecific()));
  CHECK(isZero(j.jptSpecific()));
  CHECK(isZero(j.pfSpecific()));
  CHECK(j.chargedHadronEnergy() == 40.f);  // the gen value is not touched by the no-op setter
}

TEST_CASE("2.3 calo, HF and HO energies 0", "[GenJet][2.3][neutral]") {
  pat::GenJet j(makeRecoGenJet());
  for (float v : {j.maxEInEmTowers(),
                  j.maxEInHadTowers(),
                  j.hadEnergyInHB(),
                  j.hadEnergyInHO(),
                  j.hadEnergyInHE(),
                  j.hadEnergyInHF(),
                  j.emEnergyInEB(),
                  j.emEnergyInEE(),
                  j.emEnergyInHF(),
                  j.HFHadronEnergy(),
                  j.HFEMEnergy(),
                  j.hoEnergy()})
    CHECK(v == 0.f);
}

TEST_CASE("2.3 HF/HO fractions, towersArea, HF multiplicities, elecMultiplicity 0", "[GenJet][2.3][neutral]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK(j.HFHadronEnergyFraction() == 0.f);
  CHECK(j.HFEMEnergyFraction() == 0.f);
  CHECK(j.hoEnergyFraction() == 0.f);
  CHECK(j.towersArea() == 0.f);
  CHECK(j.HFHadronMultiplicity() == 0);
  CHECK(j.HFEMMultiplicity() == 0);
  CHECK(j.elecMultiplicity() == 0.f);
}

TEST_CASE("2.3 JPT track lists empty", "[GenJet][2.3][JPT]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK(j.pionsInVertexInCalo().empty());
  CHECK(j.pionsInVertexOutCalo().empty());
  CHECK(j.pionsOutVertexInCalo().empty());
  CHECK(j.muonsInVertexInCalo().empty());
  CHECK(j.muonsInVertexOutCalo().empty());
  CHECK(j.muonsOutVertexInCalo().empty());
  CHECK(j.elecsInVertexInCalo().empty());
  CHECK(j.elecsInVertexOutCalo().empty());
  CHECK(j.elecsOutVertexInCalo().empty());
}

TEST_CASE("2.3 calo/PF constituent getters: null / empty, no throw", "[GenJet][2.3][constituents]") {
  pat::GenJet j(makeRecoGenJet());  // has 4 GenParticle daughters: pat::Jet would throw "Invalid Constituent"
  CHECK_NOTHROW(j.getCaloConstituent(0));
  CHECK(j.getCaloConstituent(0).isNull());
  CHECK(j.getCaloConstituents().empty());
  CHECK_NOTHROW(j.getPFConstituent(0));
  CHECK(j.getPFConstituent(0).isNull());
  CHECK(j.getPFConstituents().empty());
  CHECK(j.caloTowersFwdPtr().empty());
  CHECK(j.pfCandidatesFwdPtr().empty());
  // generator constituents remain available through reco::GenJet
  CHECK(j.getGenConstituents().size() == constituents().size());
}

TEST_CASE("2.3 updateFwd* out of range: no-op", "[GenJet][2.3][fwd]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK_NOTHROW(j.updateFwdCaloTowerFwdPtr(3, edm::Ptr<CaloTower>()));
  CHECK_NOTHROW(j.updateFwdPFCandidateFwdPtr(3, edm::Ptr<reco::PFCandidate>()));
  CHECK_NOTHROW(j.updateFwdTagInfoFwdPtr(3, edm::Ptr<reco::BaseTagInfo>()));
  CHECK(j.caloTowersFwdPtr().empty());
  CHECK(j.pfCandidatesFwdPtr().empty());
  CHECK(j.tagInfosFwdPtr().empty());
}

TEST_CASE("2.3 PF-named: inherited 8 equal the Specific", "[GenJet][2.3][PF]") {
  const auto s = makeSpecific();
  pat::GenJet j(makeRecoGenJet());
  CHECK(j.chargedHadronEnergy() == s.m_ChargedHadronEnergy);
  CHECK(j.neutralHadronEnergy() == s.m_NeutralHadronEnergy);
  CHECK(j.chargedEmEnergy() == s.m_ChargedEmEnergy);
  CHECK(j.neutralEmEnergy() == s.m_NeutralEmEnergy);
  CHECK(j.muonEnergy() == s.m_MuonEnergy);
  CHECK(j.chargedHadronMultiplicity() == s.m_ChargedHadronMultiplicity);
  CHECK(j.neutralHadronMultiplicity() == s.m_NeutralHadronMultiplicity);
  CHECK(j.muonMultiplicity() == s.m_MuonMultiplicity);
}

TEST_CASE("2.3 PF-named: fractions and mapped members equal their formulas", "[GenJet][2.3][PF]") {
  const auto s = makeSpecific();
  pat::GenJet j(makeRecoGenJet());
  const double E = j.energy();
  REQUIRE(E > 0);
  // fractions (copy of pat::Jet with jecSetsAvailable() false)
  CHECK(j.chargedHadronEnergyFraction() == Approx(s.m_ChargedHadronEnergy / E));
  CHECK(j.neutralHadronEnergyFraction() == Approx(s.m_NeutralHadronEnergy / E));
  CHECK(j.chargedEmEnergyFraction() == Approx(s.m_ChargedEmEnergy / E));
  CHECK(j.neutralEmEnergyFraction() == Approx(s.m_NeutralEmEnergy / E));
  CHECK(j.muonEnergyFraction() == Approx(s.m_MuonEnergy / E));
  // mapped (D1 table, )
  CHECK(j.photonEnergy() == s.m_NeutralEmEnergy);
  CHECK(j.photonEnergyFraction() == Approx(s.m_NeutralEmEnergy / E));
  CHECK(j.electronEnergy() == s.m_ChargedEmEnergy);
  CHECK(j.electronEnergyFraction() == Approx(s.m_ChargedEmEnergy / E));
  CHECK(j.chargedMuEnergy() == s.m_MuonEnergy);
  CHECK(j.chargedMuEnergyFraction() == Approx(s.m_MuonEnergy / E));
  CHECK(j.photonMultiplicity() == s.m_NeutralEmMultiplicity);
  CHECK(j.electronMultiplicity() == s.m_ChargedEmMultiplicity);
  CHECK(j.chargedMultiplicity() ==
        s.m_ChargedHadronMultiplicity + s.m_ChargedEmMultiplicity + s.m_MuonMultiplicity);
  CHECK(j.neutralMultiplicity() == s.m_NeutralHadronMultiplicity + s.m_NeutralEmMultiplicity);
  CHECK(j.energyFractionHadronic() == Approx(s.m_HadEnergy / E));
  CHECK(j.emEnergyFraction() == Approx(s.m_EmEnergy / E));
}

TEST_CASE("2.3 n90, n60 real", "[GenJet][2.3][nCarrying]") {
  const reco::GenJet rj = makeRecoGenJet();
  pat::GenJet j(rj);
  // constituent energies 50, 30, 15, 5
  CHECK(j.n90() == rj.nCarrying(0.9));
  CHECK(j.n60() == rj.nCarrying(0.6));
  CHECK(j.n90() == 3);
  CHECK(j.n60() == 2);
}

TEST_CASE("2.3 subjets: empty collection, groomedMass -1, counters", "[GenJet][2.3][subjets]") {
  pat::GenJet j(makeRecoGenJet());
  // rows: subjets(index), subjets(label) -> static empty collection
  CHECK_NOTHROW(j.subjets(0));
  CHECK(j.subjets(0).empty());
  CHECK(j.subjets(7).empty());
  CHECK(j.subjets("SoftDropPuppi").empty());
  // row: groomedMass -1
  CHECK(j.groomedMass() == -1.0);
  CHECK(j.groomedMass(3) == -1.0);
  CHECK(j.groomedMass("SoftDropPuppi") == -1.0);
  // row: hasSubjets / nSubjetCollections / subjetCollectionNames
  CHECK_FALSE(j.hasSubjets("SoftDropPuppi"));
  CHECK(j.nSubjetCollections() == 0);
  CHECK(j.subjetCollectionNames().empty());

  SECTION("an empty collection under a label also gives -1") {
    j.addSubjets(pat::GenJetPtrCollection(), "SoftDropPuppi");
    CHECK(j.hasSubjets("SoftDropPuppi"));
    CHECK(j.groomedMass("SoftDropPuppi") == -1.0);
    CHECK(j.groomedMass(0) == -1.0);
  }
  SECTION("with subjets: mass of the sum of subjet p4, label lookup") {
    std::vector<pat::GenJet> subs{pat::GenJet(makeRecoGenJet(30., 1.0, 0.4, 3.)),
                                  pat::GenJet(makeRecoGenJet(20., 1.1, 0.7, 2.))};
    pat::GenJetPtrCollection ptrs{edm::Ptr<pat::GenJet>(kSubjetsPID, &subs[0], 0),
                                  edm::Ptr<pat::GenJet>(kSubjetsPID, &subs[1], 1)};
    j.addSubjets(ptrs, "SoftDropPuppi");
    const double expected = (subs[0].p4() + subs[1].p4()).mass();
    CHECK(j.groomedMass("SoftDropPuppi") == Approx(expected));
    CHECK(j.groomedMass(0) == Approx(expected));
    CHECK(j.groomedMass("Pruned") == -1.0);
    CHECK(j.subjets("SoftDropPuppi").size() == 2);
    CHECK(j.subjets("SoftDropPuppi")[1].key() == 1);
    CHECK(j.nSubjetCollections() == 1);
    CHECK(j.subjetCollectionNames() == std::vector<std::string>{"SoftDropPuppi"});
    // daughters flattened through the subjets (copy of Jet.cc:616): the fat jet's own daughters are
    // GenParticles, not jets, so they stay as they are
    CHECK(j.numberOfDaughters() == constituents().size());
  }
}

TEST_CASE("2.3 gen matching: genJet() is the jet, genJetFwdRef self link", "[GenJet][2.3][genJet]") {
  std::vector<pat::GenJet> coll{pat::GenJet(makeRecoGenJet(50.)), pat::GenJet(makeRecoGenJet(40.))};
  // row: genJet() returns this
  CHECK(coll[1].genJet() == &coll[1]);
  CHECK(static_cast<const reco::GenJet*>(&coll[0]) == coll[0].genJet());
  // row: genJetFwdRef null until the writing module sets it
  CHECK(coll[1].genJetFwdRef().isNull());
  CHECK(coll[1].genJetFwdRef().backRef().isNull());
  // what the producer does: forward and back ref of entry i point to entry i
  for (size_t i = 0; i < coll.size(); ++i) {
    pat::GenJetRef self(kGenJetsPID, &coll[i], i);
    coll[i].setGenJetRef(pat::GenJetFwdRef(self, self));
  }
  CHECK(coll[1].genJetFwdRef().backRef().key() == 1);
  CHECK(coll[1].genJetFwdRef().backRef().id() == kGenJetsPID);
  CHECK(coll[1].genJetFwdRef().backRef()->pt() == Approx(40.));
  CHECK(coll[1].genJetFwdRef()->pt() == Approx(40.));
  // updateFwdGenJetFwdRef keeps the back ref (as pat::Jet)
  coll[1].updateFwdGenJetFwdRef(pat::GenJetRef(kGenJetsPID, &coll[0], 0));
  CHECK(coll[1].genJetFwdRef().ref().key() == 0);
  CHECK(coll[1].genJetFwdRef().backRef().key() == 1);
}

TEST_CASE("2.3 flavour, jetID, jetCharge, tracks", "[GenJet][2.3][flavour]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK(j.partonFlavour() == 0);
  CHECK(j.hadronFlavour() == 0);
  CHECK(j.jetFlavourInfo().getbHadrons().empty());
  j.setPartonFlavour(-4);
  j.setHadronFlavour(4);
  CHECK(j.partonFlavour() == -4);
  CHECK(j.hadronFlavour() == 4);
  CHECK(j.jetID().fHPD == 0.f);
  CHECK(j.jetID().n90Hits == 0);
  CHECK(j.jetCharge() == 0.f);
  CHECK(j.associatedTracks().empty());
  CHECK(j.genParton() == nullptr);
}

TEST_CASE("2.3 generic PAT lookups keep throwing", "[GenJet][2.3][PATObject]") {
  pat::GenJet j(makeRecoGenJet());
  CHECK_THROWS_AS(j.userFloat("pileupJetIdPuppi:fullDiscriminant"), cms::Exception);
  CHECK_THROWS_AS(j.userInt("hfcentralEtaStripSize"), cms::Exception);
  CHECK_THROWS(j.efficiency("trigger"));
  CHECK_THROWS(j.resolEt());
  CHECK_FALSE(j.hasUserFloat("x"));
  j.addUserFloat("x", 1.5f);
  CHECK(j.userFloat("x") == 1.5f);
}

TEST_CASE("scaleEnergy rescales p4 only", "[GenJet][kinematics]") {
  pat::GenJet j(makeRecoGenJet());
  const auto p4 = j.p4();
  j.scaleEnergy(2.0);
  CHECK(j.energy() == Approx(2.0 * p4.energy()));
  CHECK(j.jecFactor("Uncorrected") == 1.f);
  CHECK(j.chargedHadronEnergy() == 40.f);
}

TEST_CASE("copy and correctedJet equal the jet", "[GenJet][copy]") {
  pat::GenJet j(makeRecoGenJet());
  j.setPartonFlavour(5);
  j.setHadronFlavour(5);
  j.addUserFloat("NjettinessAK8Puppi:tau1", 0.3f);
  j.addUserInt("nConst", 4);
  j.addBDiscriminatorPair({"t", 0.5f});
  std::vector<pat::GenJet> subs{pat::GenJet(makeRecoGenJet(30.))};
  j.addSubjets({edm::Ptr<pat::GenJet>(kSubjetsPID, &subs[0], 0)}, "SoftDropPuppi");

  auto same = [&](const pat::GenJet& c) {
    CHECK(c.p4() == j.p4());
    CHECK(c.vertex() == j.vertex());
    CHECK(c.jetArea() == j.jetArea());
    CHECK(c.chargedHadronEnergy() == j.chargedHadronEnergy());
    CHECK(c.neutralEmMultiplicity() == j.neutralEmMultiplicity());
    CHECK(c.partonFlavour() == 5);
    CHECK(c.hadronFlavour() == 5);
    CHECK(c.userFloat("NjettinessAK8Puppi:tau1") == 0.3f);
    CHECK(c.userInt("nConst") == 4);
    CHECK(c.bDiscriminator("t") == 0.5f);
    CHECK(c.subjets("SoftDropPuppi").size() == 1);
    CHECK(c.groomedMass("SoftDropPuppi") == Approx(j.groomedMass("SoftDropPuppi")));
    CHECK(c.numberOfDaughters() == j.numberOfDaughters());
  };
  pat::GenJet copy(j);
  same(copy);
  pat::GenJet assigned;
  assigned = j;
  same(assigned);
  same(j.correctedJet("Uncorrected"));
  same(j.correctedJet(0));
  std::unique_ptr<pat::GenJet> cl(j.clone());
  same(*cl);
}

// ---------------------------------------------------------------- ROOT I/O
namespace {
  const char* const kRoundTripFile = "testGenJet_roundtrip.root";
  const char* const kVersionFile = "testGenJet_classversion.root";

  // the collection is returned by value: the transient cached pointers of the self links are not used
  // after the move, only (product id, key) are checked
  std::vector<pat::GenJet> makeCollectionForIO(std::vector<pat::GenJet>& subjetStore) {
    subjetStore = {pat::GenJet(makeRecoGenJet(30.)), pat::GenJet(makeRecoGenJet(20.))};
    std::vector<pat::GenJet> coll{pat::GenJet(makeRecoGenJet(120.)), pat::GenJet(makeRecoGenJet(60.))};
    reco::GenParticleRefVector bHadrons;
    static const reco::GenParticle bHadron;
    bHadrons.push_back(reco::GenParticleRef(kBHadronPID, &bHadron, 3));
    coll[0].setJetFlavourInfo(
        reco::JetFlavourInfo(bHadrons, reco::GenParticleRefVector(), reco::GenParticleRefVector(),
                             reco::GenParticleRefVector(), 5, -5));
    coll[0].addSubjets({edm::Ptr<pat::GenJet>(kSubjetsPID, &subjetStore[0], 0),
                        edm::Ptr<pat::GenJet>(kSubjetsPID, &subjetStore[1], 1)},
                       "SoftDropPuppi");
    coll[0].addUserFloat("NjettinessAK8Puppi:tau1", 0.31f);
    coll[0].addUserInt("hfcentralEtaStripSize", 2);
    coll[1].setPartonFlavour(21);
    for (size_t i = 0; i < coll.size(); ++i) {
      pat::GenJetRef self(kGenJetsPID, &coll[i], i);
      coll[i].setGenJetRef(pat::GenJetFwdRef(self, self));
    }
    // fill the transient caches before writing
    (void)coll[0].numberOfDaughters();
    (void)coll[0].getCaloConstituents();
    (void)coll[0].getPFConstituents();
    return coll;
  }

  void writeCollection(const char* fname, const std::vector<pat::GenJet>& coll) {
    std::unique_ptr<TFile> f(TFile::Open(fname, "RECREATE"));
    REQUIRE(f);
    TTree t("t", "pat::GenJet test");
    const std::vector<pat::GenJet>* ptr = &coll;
    REQUIRE(t.Branch("jets", &ptr) != nullptr);
    REQUIRE(t.Fill() > 0);
    t.Write();
    f->Close();
  }

  std::vector<pat::GenJet> readCollection(const char* fname) {
    std::unique_ptr<TFile> f(TFile::Open(fname, "READ"));
    REQUIRE(f);
    TTree* t = f->Get<TTree>("t");
    REQUIRE(t);
    std::vector<pat::GenJet>* ptr = nullptr;
    t->SetBranchAddress("jets", &ptr);
    REQUIRE(t->GetEntry(0) > 0);
    REQUIRE(ptr);
    std::vector<pat::GenJet> out(*ptr);
    delete ptr;
    return out;
  }
}  // namespace

TEST_CASE("ROOT write/read of std::vector<pat::GenJet>", "[GenJet][io]") {
  std::vector<pat::GenJet> subjetStore;
  const auto coll = makeCollectionForIO(subjetStore);
  REQUIRE(Peek::daughtersCached(coll[0]));
  REQUIRE(Peek::towersCached(coll[0]));
  writeCollection(kRoundTripFile, coll);
  const auto back = readCollection(kRoundTripFile);

  REQUIRE(back.size() == 2);
  // caches are transient and reset by the ioread rules
  CHECK_FALSE(Peek::daughtersCached(back[0]));
  CHECK_FALSE(Peek::towersCached(back[0]));
  CHECK_FALSE(Peek::pfCached(back[0]));
  for (size_t i = 0; i < 2; ++i) {
    CHECK(back[i].p4() == coll[i].p4());
    CHECK(back[i].vertex().x() == Approx(coll[i].vertex().x()));  // vertex is stored with reduced precision
    CHECK(back[i].vertex().z() == Approx(coll[i].vertex().z()));
    CHECK(back[i].jetArea() == coll[i].jetArea());
    // Specific survives (the reason for PATObject<reco::GenJet>)
    CHECK(back[i].chargedHadronEnergy() == 40.f);
    CHECK(back[i].neutralEmMultiplicity() == 5);
    CHECK(back[i].emEnergy() == 30.f);
    // stored daughters: product id and key. The stored vector is read without the subjet flattening of
    // daughterPtrVector(), which dereferences the daughters and therefore needs the event (as in pat::Jet).
    auto const& daus = back[i].reco::CompositePtrCandidate::daughterPtrVector();
    REQUIRE(daus.size() == constituents().size());
    CHECK(daus[2].id() == kConstituentsPID);
    CHECK(daus[2].key() == 2);
    // self link: id and key
    CHECK(back[i].genJetFwdRef().backRef().id() == kGenJetsPID);
    CHECK(back[i].genJetFwdRef().backRef().key() == i);
    CHECK(back[i].genJetFwdRef().ref().key() == i);
    CHECK(back[i].genJet() == &back[i]);
  }
  // flavour
  CHECK(back[0].hadronFlavour() == 5);
  CHECK(back[0].partonFlavour() == -5);
  REQUIRE(back[0].jetFlavourInfo().getbHadrons().size() == 1);
  CHECK(back[0].jetFlavourInfo().getbHadrons()[0].id() == kBHadronPID);
  CHECK(back[0].jetFlavourInfo().getbHadrons()[0].key() == 3);
  CHECK(back[1].partonFlavour() == 21);
  // subjets with label (id, key)
  CHECK(back[0].subjetCollectionNames() == std::vector<std::string>{"SoftDropPuppi"});
  REQUIRE(back[0].subjets("SoftDropPuppi").size() == 2);
  CHECK(back[0].subjets("SoftDropPuppi")[1].id() == kSubjetsPID);
  CHECK(back[0].subjets("SoftDropPuppi")[1].key() == 1);
  CHECK(back[1].nSubjetCollections() == 0);
  // user data
  CHECK(back[0].userFloat("NjettinessAK8Puppi:tau1") == 0.31f);
  CHECK(back[0].userInt("hfcentralEtaStripSize") == 2);
  CHECK_FALSE(back[1].hasUserFloat("NjettinessAK8Puppi:tau1"));
  // neutral values still neutral after I/O
  CHECK(back[0].jecFactor("Uncorrected") == 1.f);
  CHECK(back[0].bDiscriminator("x") == -1000.f);
  CHECK(back[1].groomedMass("SoftDropPuppi") == -1.0);
}

TEST_CASE("class version: a file written by this build is read back", "[GenJet][io][version]") {
  TClass* cl = TClass::GetClass("pat::io_v1::GenJet");
  REQUIRE(cl);
  CHECK(cl->GetClassVersion() == 3);
  {
    std::vector<pat::GenJet> subjetStore;
    writeCollection(kVersionFile, makeCollectionForIO(subjetStore));
  }
  // reopen: the streamer info stored in the file must be version 3 with this build's checksum
  {
    std::unique_ptr<TFile> f(TFile::Open(kVersionFile, "READ"));
    REQUIRE(f);
    std::unique_ptr<TList> infos(f->GetStreamerInfoList());
    REQUIRE(infos);
    TStreamerInfo* si = dynamic_cast<TStreamerInfo*>(infos->FindObject("pat::io_v1::GenJet"));
    REQUIRE(si);
    CHECK(si->GetClassVersion() == 3);
    CHECK(si->GetCheckSum() == cl->GetCheckSum());
  }
  const auto back = readCollection(kVersionFile);
  REQUIRE(back.size() == 2);
  CHECK(back[0].pt() == Approx(120.));
  CHECK(back[0].subjets("SoftDropPuppi").size() == 2);
  CHECK(back[0].hadronFlavour() == 5);
}

// ==================================================================================================
// Further behaviour checks: constructors, embedded getters, tag infos, update setters, zero-energy jet
// ==================================================================================================

TEST_CASE("RefToBase constructors (reco::GenJet and pat::GenJet)", "[GenJet][construction]") {
  const reco::GenJet rj = makeRecoGenJet(42.);
  std::vector<reco::GenJet> rcoll{makeRecoGenJet(10.), rj};
  edm::OrphanHandle<std::vector<reco::GenJet>> rh(&rcoll, kGenJetsPID);
  edm::RefToBase<reco::GenJet> rref(edm::Ref<std::vector<reco::GenJet>>(rh, 1));
  pat::GenJet fromRef = rref;  // implicit
  CHECK(fromRef.p4() == rj.p4());
  CHECK(fromRef.emEnergy() == 30.f);  // Specific kept
  CHECK(fromRef.originalObjectRef().id() == kGenJetsPID);
  CHECK(fromRef.originalObjectRef().key() == 1);
  CHECK(fromRef.originalObject() == &rcoll[1]);

  pat::GenJet pj(rj);
  pj.addUserFloat("x", 2.5f);
  std::vector<pat::GenJet> pcoll{pat::GenJet(makeRecoGenJet(11.)), pj};
  edm::OrphanHandle<std::vector<pat::GenJet>> ph(&pcoll, kSubjetsPID);
  edm::RefToBase<pat::GenJet> pref(edm::Ref<std::vector<pat::GenJet>>(ph, 1));
  pat::GenJet fromPatRef = pref;
  CHECK(fromPatRef.p4() == rj.p4());
  CHECK(fromPatRef.userFloat("x") == 2.5f);  // pat content copied
  // as pat::Jet(RefToBase<pat::Jet>): the original object becomes the pat jet it was made from
  CHECK(fromPatRef.originalObjectRef().id() == kSubjetsPID);
  CHECK(fromPatRef.originalObjectRef().key() == 1);
}

TEST_CASE("2.3 embedded towers / PF candidates returned when set", "[GenJet][2.3][constituents]") {
  std::vector<CaloTower> towers(2);
  std::vector<reco::PFCandidate> cands(3);
  const edm::ProductID towerPID(1, 7), candPID(1, 8);
  pat::CaloTowerFwdPtrCollection tfp;
  for (size_t i = 0; i < towers.size(); ++i) {
    edm::Ptr<CaloTower> p(towerPID, &towers[i], i);
    tfp.emplace_back(p, p);
  }
  pat::PFCandidateFwdPtrCollection cfp;
  for (size_t i = 0; i < cands.size(); ++i) {
    edm::Ptr<reco::PFCandidate> p(candPID, &cands[i], i);
    cfp.emplace_back(p, p);
  }
  pat::GenJet j(makeRecoGenJet());
  j.setCaloTowers(tfp);
  j.setPFCandidates(cfp);
  REQUIRE(j.caloTowersFwdPtr().size() == 2);
  REQUIRE(j.pfCandidatesFwdPtr().size() == 3);
  CHECK(j.getCaloConstituent(1).key() == 1);
  CHECK(j.getCaloConstituent(1).get() == &towers[1]);
  CHECK(j.getCaloConstituent(2).isNull());  // out of range: null, no throw
  CHECK(j.getCaloConstituents().size() == 2);
  CHECK(j.getCaloConstituents()[0].get() == &towers[0]);
  CHECK(j.getPFConstituent(2).get() == &cands[2]);
  CHECK(j.getPFConstituent(3).isNull());
  CHECK(j.getPFConstituents().size() == 3);
  // a second set appends (copy of pat::Jet) and resets the cache
  REQUIRE(Peek::towersCached(j));
  j.setCaloTowers(tfp);
  CHECK_FALSE(Peek::towersCached(j));
  CHECK(j.getCaloConstituents().size() == 4);
  // daughters (generator constituents) are unaffected
  CHECK(j.numberOfDaughters() == constituents().size());
}

TEST_CASE("2.3 addTagInfo with data", "[GenJet][2.3][btag]") {
  std::vector<reco::CandIPTagInfo> infos(1);
  const edm::ProductID infoPID(1, 9);
  edm::Ptr<reco::BaseTagInfo> p(infoPID, &infos[0], 0);
  pat::GenJet j(makeRecoGenJet());
  j.addTagInfo("pfImpactParameterTagInfos", pat::TagInfoFwdPtrCollection::value_type(p, p));
  REQUIRE(j.tagInfoLabels().size() == 1);
  CHECK(j.tagInfoLabels()[0] == "pfImpactParameter");  // "TagInfos" stripped, as pat::Jet
  CHECK(j.hasTagInfo("pfImpactParameter"));
  CHECK(j.tagInfo("pfImpactParameter") == &infos[0]);
  CHECK(j.tagInfo("pfImpactParameterTagInfos") == nullptr);
  CHECK(j.tagInfoCandIP() == &infos[0]);                     // by type
  CHECK(j.tagInfoCandIP("pfImpactParameter") == &infos[0]);  // by label
  CHECK(j.tagInfoTrackIP() == nullptr);                      // other type
  CHECK(j.tagInfoCandSecondaryVertex("pfImpactParameter") == nullptr);
  CHECK(j.tagInfosFwdPtr().size() == 1);
}

TEST_CASE("2.3 updateFwd* in range: forward updated, back kept, cache reset", "[GenJet][2.3][fwd]") {
  std::vector<CaloTower> towers(2), towersNew(2);
  std::vector<reco::PFCandidate> cands(2), candsNew(2);
  std::vector<reco::CandIPTagInfo> infos(1), infosNew(1);
  const edm::ProductID tOld(1, 7), tNew(1, 17), cOld(1, 8), cNew(1, 18), iOld(1, 9), iNew(1, 19);
  pat::CaloTowerFwdPtrCollection tfp;
  pat::PFCandidateFwdPtrCollection cfp;
  for (size_t i = 0; i < 2; ++i) {
    edm::Ptr<CaloTower> tp(tOld, &towers[i], i);
    tfp.emplace_back(tp, tp);
    edm::Ptr<reco::PFCandidate> cp(cOld, &cands[i], i);
    cfp.emplace_back(cp, cp);
  }
  edm::Ptr<reco::BaseTagInfo> ip(iOld, &infos[0], 0);
  pat::GenJet j(makeRecoGenJet());
  j.setCaloTowers(tfp);
  j.setPFCandidates(cfp);
  j.addTagInfo("ip", pat::TagInfoFwdPtrCollection::value_type(ip, ip));

  // fill the caches, then update: the extra reset (not in pat::Jet) must clear them
  REQUIRE(j.getCaloConstituents().size() == 2);
  REQUIRE(j.getPFConstituents().size() == 2);
  REQUIRE(Peek::towersCached(j));
  REQUIRE(Peek::pfCached(j));

  j.updateFwdCaloTowerFwdPtr(1, edm::Ptr<CaloTower>(tNew, &towersNew[1], 1));
  CHECK_FALSE(Peek::towersCached(j));
  CHECK(j.caloTowersFwdPtr()[1].ptr().id() == tNew);
  CHECK(j.caloTowersFwdPtr()[1].backPtr().id() == tOld);
  CHECK(j.caloTowersFwdPtr()[0].ptr().id() == tOld);  // other entry untouched
  CHECK(j.getCaloConstituents()[1].get() == &towersNew[1]);  // rebuilt cache sees the new pointer
  CHECK(j.getCaloConstituent(1).get() == &towersNew[1]);

  j.updateFwdPFCandidateFwdPtr(0, edm::Ptr<reco::PFCandidate>(cNew, &candsNew[0], 0));
  CHECK_FALSE(Peek::pfCached(j));
  CHECK(j.pfCandidatesFwdPtr()[0].ptr().id() == cNew);
  CHECK(j.pfCandidatesFwdPtr()[0].backPtr().id() == cOld);
  CHECK(j.getPFConstituents()[0].get() == &candsNew[0]);

  j.updateFwdTagInfoFwdPtr(0, edm::Ptr<reco::BaseTagInfo>(iNew, &infosNew[0], 0));
  CHECK(j.tagInfosFwdPtr()[0].ptr().id() == iNew);
  CHECK(j.tagInfosFwdPtr()[0].backPtr().id() == iOld);
  CHECK(j.tagInfo("ip") == &infosNew[0]);
}

TEST_CASE("2.3 getGenConstituent(s): inherited, throws on packed constituents", "[GenJet][2.3][constituents]") {
  // kept as in reco::GenJet; works on GenParticle constituents, throws on packed ones
  std::vector<pat::PackedGenParticle> packed;
  for (auto const& gp : constituents())
    packed.emplace_back(gp, reco::GenParticleRef());  // 1-arg ctor reads motherRef(0)
  const edm::ProductID packedPID(1, 10);
  reco::GenJet rj(constituents()[0].p4(), reco::Particle::Point(0, 0, 0), makeSpecific());
  for (size_t i = 0; i < packed.size(); ++i)
    rj.addDaughter(reco::CandidatePtr(packedPID, &packed[i], i));
  pat::GenJet j(rj);
  REQUIRE(j.numberOfDaughters() == packed.size());
  CHECK(j.daughterPtr(2).key() == 2);  // the documented replacement
  const pat::GenJet& cj = j;  // the non-const daughter(i) of reco candidates returns nullptr
  CHECK(cj.daughter(2)->pt() == Approx(packed[2].pt()));
  CHECK_THROWS_WITH(j.getGenConstituents(), Catch::Matchers::ContainsSubstring("Invalid Constituent"));
  CHECK_THROWS_AS(j.getGenConstituent(0), cms::Exception);
  CHECK(j.getGenConstituent(99) == nullptr);  // out of range: nullptr (reco::GenJet)
  // GenParticle constituents: works
  pat::GenJet ok(makeRecoGenJet());
  CHECK(ok.getGenConstituent(1) == &constituents()[1]);
}

TEST_CASE("zero-energy jet: what each fraction returns", "[GenJet][2.3][zeroE]") {
  // A jet with E = 0 and an all-zero Specific (no generator jet should have it).
  // PF-named fractions keep pat::Jet's unguarded body x/energy(): 0/0 = NaN (no new guard, as decided).
  // Calo-like fractions energyFractionHadronic, emEnergyFraction are guarded: 0.
  // HF/HO fractions are constants: 0.
  reco::GenJet rj(reco::Particle::LorentzVector(0, 0, 0, 0), reco::Particle::Point(0, 0, 0), reco::GenJet::Specific());
  pat::GenJet j(rj);
  REQUIRE(j.energy() == 0.);
  CHECK(std::isnan(j.chargedHadronEnergyFraction()));
  CHECK(std::isnan(j.neutralHadronEnergyFraction()));
  CHECK(std::isnan(j.chargedEmEnergyFraction()));
  CHECK(std::isnan(j.neutralEmEnergyFraction()));
  CHECK(std::isnan(j.muonEnergyFraction()));
  CHECK(std::isnan(j.photonEnergyFraction()));
  CHECK(std::isnan(j.electronEnergyFraction()));
  CHECK(std::isnan(j.chargedMuEnergyFraction()));
  CHECK(j.energyFractionHadronic() == 0.f);
  CHECK(j.emEnergyFraction() == 0.f);
  CHECK(j.HFHadronEnergyFraction() == 0.f);
  CHECK(j.HFEMEnergyFraction() == 0.f);
  CHECK(j.hoEnergyFraction() == 0.f);
  CHECK(j.n90() == 0);
  CHECK(j.n60() == 0);
  CHECK(j.jecFactor(0) == 1.f);
  CHECK(j.groomedMass() == -1.0);
}
