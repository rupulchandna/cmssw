// P4: PATGenJetSelector with TestProcessor and hand-made inputs.
// Checks: cut / cutLoose / nLoose select the same jets as PATGenJetSlimmer on the same (pt-sorted) jets;
// self links re-pointed to the selector's output; Association old -> new; filter; slimming options
// off by default and working when switched on.

#include "PhysicsTools/PatGenJet/interface/GenJet.h"
#include "PhysicsTools/PatGenJet/test/PatGenJetTestCommon.h"

#include "DataFormats/BTauReco/interface/BaseTagInfo.h"
#include "DataFormats/Common/interface/Association.h"
#include "DataFormats/Common/interface/OwnVector.h"
#include "FWCore/TestProcessor/interface/TestProcessor.h"

#include <string>
#include <vector>

// DataFormats/Math/interface/Error.h defines a STATIC_CHECK macro that clashes with Catch2
#undef STATIC_CHECK
#include <catch2/catch_all.hpp>

using Catch::Approx;

namespace {
  const std::string kInputs = "genInputsForPatGenJets";
  const std::string kCuts = "cut = cms.string('pt > 40'), cutLoose = cms.string('pt > 10.'), nLoose = cms.uint32(3)";

  // the reference: PATGenJetSlimmer (PhysicsTools/PatAlgos) on the input reco::GenJets, sorted by pt as AOD jets
  std::string slimmerConfig(std::string const& cuts) {
    return patgenjettest::header(true) + "process.toTest = cms.EDProducer('PATGenJetSlimmer', src = cms.InputTag('" +
           kInputs + ":jets'), packedGenParticles = cms.InputTag('" + kInputs + "'), " + cuts +
           ", clearDaughters = cms.bool(False), dropSpecific = cms.bool(False))\n"
           "process.moduleToTest(process.toTest, cms.Task(process." +
           kInputs + "))\n";
  }

  // PATGenJetProducer -> PATGenJetSelector on the same inputs
  std::string selectorConfig(std::string const& args) {
    return patgenjettest::header(true) + "process.patGenJets = cms.EDProducer('PATGenJetProducer', jetSource = cms.InputTag('" +
           kInputs + ":jets'), packedGenParticles = cms.InputTag('" + kInputs + "'), daughterOrder = cms.string('key'))\n" +
           "process.toTest = cms.EDFilter('PATGenJetSelector', src = cms.InputTag('patGenJets'), " + args + ")\n" +
           "process.moduleToTest(process.toTest, cms.Task(process." + kInputs + ", process.patGenJets))\n";
  }

  std::vector<double> slimmerPts(std::string const& cuts) {
    edm::test::TestProcessor tester(slimmerConfig(cuts));
    auto event = tester.test();
    return patgenjettest::ptsOf(*event.get<std::vector<reco::GenJet>>());
  }
}  // namespace

TEST_CASE("PATGenJetSelector: same selection as PATGenJetSlimmer", "[PATGenJetSelector]") {
  // jets 80, 50, 30, 12: cut keeps 80, 50; the first 3 loose jets add 30; 12 is the 4th loose jet
  for (std::string const& cuts :
       {kCuts,
        std::string("cut = cms.string('pt > 40'), cutLoose = cms.string('pt > 10.'), nLoose = cms.uint32(0)"),
        std::string("cut = cms.string(''), cutLoose = cms.string(''), nLoose = cms.uint32(0)"),
        std::string("cut = cms.string('pt > 1000'), cutLoose = cms.string('pt > 20.'), nLoose = cms.uint32(1)")}) {
    INFO(cuts);
    const auto ref = slimmerPts(cuts);
    edm::test::TestProcessor tester(selectorConfig(cuts));
    auto event = tester.test();
    auto const& out = *event.get<std::vector<pat::GenJet>>();
    CHECK_THAT(patgenjettest::pts(out), Catch::Matchers::Approx(ref));
  }
  CHECK_THAT(slimmerPts(kCuts), Catch::Matchers::Approx(std::vector<double>{80., 50., 30.}));
}

TEST_CASE("PATGenJetSelector: self links, association, content", "[PATGenJetSelector]") {
  edm::test::TestProcessor tester(selectorConfig(kCuts));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 3);

  // the copies pointed into the producer output; now they point into the selector output
  CHECK(patgenjettest::badSelfLinks(out) == 0);

  // Association old -> new, as PATGenJetSlimmer: input i -> output i for i < 3, input 3 (pt 12) -> null
  auto const& assoc = *event.get<edm::Association<std::vector<pat::GenJet>>>();
  REQUIRE(assoc.ids().size() == 1);
  const edm::ProductID inputId = assoc.ids()[0].first;
  CHECK(inputId != out[0].genJetFwdRef().ref().id());
  for (unsigned i = 0; i < 3; ++i) {
    auto const r = assoc.get(inputId, i);
    REQUIRE(r.isNonnull());
    CHECK(r.key() == i);
    CHECK(r.get() == &out[i]);
  }
  CHECK(assoc.get(inputId, 3).isNull());

  // defaults: no slimming, content is the producer's (Specific, daughters, flavour defaults)
  CHECK(out[0].emEnergy() == Approx(20.));
  CHECK(out[0].numberOfDaughters() == 3);
  CHECK(out[0].originalObjectRef().key() == 0);  // sorted inputs: the "80" jet is input 0
  CHECK(event.get<edm::OwnVector<reco::BaseTagInfo>>("tagInfos")->empty());
  CHECK(event.modulePassed());
}

TEST_CASE("PATGenJetSelector: filter", "[PATGenJetSelector]") {
  const std::string none = "cut = cms.string('pt > 1000'), cutLoose = cms.string(''), nLoose = cms.uint32(0)";
  {
    edm::test::TestProcessor tester(selectorConfig(none + ", filter = cms.bool(True)"));
    auto event = tester.test();
    CHECK(event.get<std::vector<pat::GenJet>>()->empty());
    CHECK_FALSE(event.modulePassed());
  }
  {
    edm::test::TestProcessor tester(selectorConfig(none));
    auto event = tester.test();
    CHECK(event.get<std::vector<pat::GenJet>>()->empty());
    CHECK(event.modulePassed());  // filter False by default
  }
}

TEST_CASE("PATGenJetSelector: slimming options", "[PATGenJetSelector]") {
  edm::test::TestProcessor tester(
      selectorConfig(kCuts + ", dropSpecific = cms.string('1'), dropDaughters = cms.string('pt > 60')"));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 3);
  CHECK(out[0].numberOfDaughters() == 0);  // pt 80 > 60
  CHECK(out[1].numberOfDaughters() == 2);  // pt 50 kept
  for (auto const& j : out) {
    CHECK(j.emEnergy() == 0.f);
    CHECK(j.hadEnergy() == 0.f);
    CHECK(j.chargedHadronMultiplicity() == 0);
  }
  CHECK(patgenjettest::badSelfLinks(out) == 0);
}

TEST_CASE("PATGenJetSelector: clearDaughters alias of PATGenJetSlimmer (review M2)", "[PATGenJetSelector]") {
  {
    edm::test::TestProcessor tester(selectorConfig(kCuts + ", clearDaughters = cms.bool(True)"));
    auto event = tester.test();
    auto const& out = *event.get<std::vector<pat::GenJet>>();
    REQUIRE(out.size() == 3);
    for (auto const& j : out)
      CHECK(j.numberOfDaughters() == 0);
    CHECK(patgenjettest::badSelfLinks(out) == 0);
  }
  {
    edm::test::TestProcessor tester(selectorConfig(kCuts + ", clearDaughters = cms.bool(False)"));
    auto event = tester.test();
    auto const& out = *event.get<std::vector<pat::GenJet>>();
    REQUIRE(out.size() == 3);
    CHECK(out[0].numberOfDaughters() > 0);
  }
}
