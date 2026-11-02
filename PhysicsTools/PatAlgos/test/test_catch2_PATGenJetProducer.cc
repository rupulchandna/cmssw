// PATGenJetProducer with TestProcessor and hand-made inputs.
// Checks: re-keying (both daughterOrder values), sorting, self links after sort, flavour from a
// JetFlavourInfoMatchingCollection, user data with stripped postfix,
// MC switched off for real data.

#include "DataFormats/PatCandidates/interface/GenJet.h"
#include "PhysicsTools/PatAlgos/plugins/PATGenJetPluginHelpers.h"
#include "PhysicsTools/PatAlgos/test/PatGenJetTestCommon.h"

#include "DataFormats/BTauReco/interface/BaseTagInfo.h"
#include "DataFormats/Common/interface/OwnVector.h"
#include "DataFormats/PatCandidates/interface/PackedGenParticle.h"
#include "FWCore/TestProcessor/interface/TestProcessor.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <cmath>
#include <string>
#include <vector>

// DataFormats/Math/interface/Error.h defines a STATIC_CHECK macro that clashes with Catch2
#undef STATIC_CHECK
#include <catch2/catch_all.hpp>

using Catch::Approx;
using patgenjettest::header;

namespace {
  const std::string kInputs = "genInputsForPatGenJets";

  // extra: more arguments of the module; post: python lines run after it (to override a base argument)
  std::string producerConfig(std::string const& extra, std::string const& post = "", bool unmapped = false) {
    return header(false, unmapped) +
           "process.toTest = cms.EDProducer('PATGenJetProducer',\n  jetSource = cms.InputTag('" + kInputs +
           ":jets'),\n  packedGenParticles = cms.InputTag('" + kInputs +
           "'),\n  JetFlavourInfoSource = cms.InputTag('" + kInputs + ":flavour')" + extra + ")\n" + post +
           "process.moduleToTest(process.toTest, cms.Task(process." + kInputs + "))\n";
  }

  std::vector<unsigned> keys(pat::GenJet const& j) {
    std::vector<unsigned> r;
    for (size_t i = 0; i < j.numberOfDaughters(); ++i)
      r.push_back(j.daughterPtr(i).key());
    return r;
  }

  pat::GenJet const& jetWithPt(std::vector<pat::GenJet> const& v, double pt) {
    for (auto const& j : v)
      if (std::abs(j.pt() - pt) < 1e-6)
        return j;
    throw std::runtime_error("no jet with this pt");
  }
}  // namespace

TEST_CASE("PATGenJetProducer: sorting, self links, re-keying in pt order, flavour, Specific", "[PATGenJetProducer]") {
  edm::test::TestProcessor tester(producerConfig(""));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();

  // sorting: input order is 30, 80, 50, 12
  REQUIRE(out.size() == 4);
  CHECK_THAT(patgenjettest::pts(out), Catch::Matchers::Approx(std::vector<double>{80., 50., 30., 12.}));
  // the original object is the input jet (input index of the "80" jet is 1)
  CHECK(out[0].originalObjectRef().key() == 1);

  // self links after sort
  CHECK(patgenjettest::badSelfLinks(out) == 0);

  // re-keying, daughterOrder "pt" (default): gens {1, 5, 8, 9} -> packed {7, 3, 0, 0}; duplicate dropped
  auto const& j80 = out[0];
  CHECK(keys(j80) == std::vector<unsigned>{7, 3, 0});
  const edm::ProductID origId = j80.originalObject()->sourceCandidatePtr(0).id();  // the genParticles product
  for (size_t i = 0; i < j80.numberOfDaughters(); ++i) {
    CHECK(j80.daughterPtr(i).id() != origId);
    CHECK(j80.daughterPtr(i).id() == j80.daughterPtr(0).id());
    CHECK(dynamic_cast<pat::PackedGenParticle const*>(j80.daughterPtr(i).get()) != nullptr);
  }
  // constituent kinematics preserved (packing precision); gen pt of gens 1, 5, 8 = 40, 30, 22
  CHECK(j80.daughterPtr(0)->pt() == Approx(40.).epsilon(0.01));
  CHECK(j80.daughterPtr(1)->pt() == Approx(30.).epsilon(0.01));
  CHECK(j80.daughterPtr(2)->pt() == Approx(22.).epsilon(0.01));
  CHECK(keys(jetWithPt(out, 50.)) == std::vector<unsigned>{1, 8});
  CHECK(keys(jetWithPt(out, 12.)) == std::vector<unsigned>{4, 2});
  // n90 uses the stored order: pt order 40, 30, 22 -> 3 constituents for 90 %
  CHECK(j80.n90() == 3);

  // flavour (addJetFlavourInfo default True)
  CHECK(j80.hadronFlavour() == 5);
  CHECK(j80.partonFlavour() == 5);
  CHECK(j80.jetFlavourInfo().getbHadrons().size() == 1);
  CHECK(jetWithPt(out, 50.).hadronFlavour() == 4);
  CHECK(jetWithPt(out, 50.).partonFlavour() == -4);
  CHECK(jetWithPt(out, 30.).partonFlavour() == 21);
  CHECK(jetWithPt(out, 12.).partonFlavour() == 1);

  // Specific kept (emEnergy = pt/4, hadEnergy = pt/2 in the inputs)
  CHECK(j80.emEnergy() == Approx(20.));
  CHECK(j80.hadEnergy() == Approx(40.));
  CHECK(j80.jetArea() == Approx(0.5));

  // tagInfos product written (empty here)
  CHECK(event.get<edm::OwnVector<reco::BaseTagInfo>>("tagInfos")->empty());
}

TEST_CASE("PATGenJetProducer: daughterOrder key reproduces PATGenJetSlimmer order", "[PATGenJetProducer]") {
  edm::test::TestProcessor tester(producerConfig(",\n  daughterOrder = cms.string('key')"));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 4);
  CHECK(keys(out[0]) == std::vector<unsigned>{0, 3, 7});
  CHECK(keys(jetWithPt(out, 12.)) == std::vector<unsigned>{2, 4});
  CHECK(patgenjettest::badSelfLinks(out) == 0);
}

TEST_CASE("PATGenJetProducer: empty packedGenParticles keeps the original constituents", "[PATGenJetProducer]") {
  edm::test::TestProcessor tester(producerConfig("", "process.toTest.packedGenParticles = ''\n"));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 4);
  CHECK(keys(out[0]) == std::vector<unsigned>{1, 5, 8, 9});
  CHECK(out[0].daughterPtr(0).id() == out[0].originalObject()->sourceCandidatePtr(0).id());
  CHECK(out[0].getGenConstituents().size() == 4);  // GenParticle constituents: works
}

TEST_CASE("PATGenJetProducer: a constituent without packed partner is dropped", "[PATGenJetProducer]") {
  // the input jet "30" has a third daughter (gen 10) whose association entry is a null reference
  for (const char* order : {"pt", "key"}) {
    edm::test::TestProcessor tester(
        producerConfig(std::string(",\n  daughterOrder = cms.string('") + order + "')", "", true));
    auto event = tester.test();
    auto const& out = *event.get<std::vector<pat::GenJet>>();
    REQUIRE(out.size() == 4);
    auto const& j30 = jetWithPt(out, 30.);
    CHECK(j30.originalObject()->numberOfDaughters() == 3);
    CHECK(j30.numberOfDaughters() == 2);  // the unmapped one is dropped, no null Ptr is stored
    for (size_t i = 0; i < j30.numberOfDaughters(); ++i)
      CHECK(j30.daughterPtr(i).isNonnull());
    CHECK(keys(j30) == std::vector<unsigned>{5, 6});  // gens {3, 2} -> packed {5, 6}
    // the other jets are untouched
    CHECK(jetWithPt(out, 80.).numberOfDaughters() == 3);
    CHECK(patgenjettest::badSelfLinks(out) == 0);
  }
}

TEST_CASE("PATGenJetProducer: user data with stripped postfix", "[PATGenJetProducer]") {
  const std::string extra = ",\n  userData = " + patgenjettest::userData("cms.InputTag('" + kInputs + "', 'tau1')",
                                                                         "cms.InputTag('" + kInputs + "', 'nconst')",
                                                                         "'ForPatGenJets'");
  edm::test::TestProcessor tester(producerConfig(extra));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 4);
  for (auto const& j : out) {
    const unsigned inputIndex = j.originalObjectRef().key();
    // label = encoded InputTag with the postfix stripped (PATUserDataMerger.h:104-118)
    CHECK(j.hasUserFloat("genInputs:tau1"));
    CHECK_FALSE(j.hasUserFloat("genInputsForPatGenJets:tau1"));
    // value from the map
    CHECK(j.userFloat("genInputs:tau1") == Approx(0.1 * (inputIndex + 1)));
    CHECK(j.userInt("genInputs:nconst") == static_cast<int>(j.originalObject()->numberOfDaughters()));
  }
}

TEST_CASE("PATGenJetProducer: no flavour input leaves the default flavour", "[PATGenJetProducer]") {
  edm::test::TestProcessor tester(producerConfig("", "process.toTest.JetFlavourInfoSource = ''\n"));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 4);
  for (auto const& j : out) {
    CHECK(j.hadronFlavour() == 0);
    CHECK(j.partonFlavour() == 0);
  }
}

TEST_CASE("PATGenJetProducer: addJetFlavourInfo False copies only the flavours", "[PATGenJetProducer]") {
  edm::test::TestProcessor tester(producerConfig(",\n  addJetFlavourInfo = cms.bool(False)"));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 4);
  CHECK(out[0].hadronFlavour() == 5);
  CHECK(out[0].partonFlavour() == 5);
  CHECK(out[0].jetFlavourInfo().getbHadrons().empty());
}

TEST_CASE("PATGenJetProducer: MC switched off on real data", "[PATGenJetProducer]") {
  // TestProcessor always makes MC events (FWCore/TestProcessor/src/TestProcessor.cc:554 isRealData=false),
  // so the per-event switch used by produce() is tested directly.
  pat::genjethelpers::MCSwitches on;
  on.getJetMCFlavour = on.useLegacyJetMCFlavour = on.addJetFlavourInfo = on.addGenPartonMatch = true;
  const auto data = on.forEvent(true);
  CHECK_FALSE(data.getJetMCFlavour);
  CHECK_FALSE(data.useLegacyJetMCFlavour);
  CHECK_FALSE(data.addJetFlavourInfo);
  CHECK_FALSE(data.addGenPartonMatch);
  const auto mc = on.forEvent(false);
  CHECK(mc.getJetMCFlavour);
  CHECK(mc.addJetFlavourInfo);
  CHECK(mc.addGenPartonMatch);
}

TEST_CASE("PATGenJetProducer: configuration checks", "[PATGenJetProducer]") {
  CHECK_THROWS_AS(edm::test::TestProcessor(producerConfig(",\n  daughterOrder = cms.string('eta')")), cms::Exception);
}
