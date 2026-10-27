// P4: PATGenJetProducer with TestProcessor and hand-made inputs.
// Checks: re-keying (both daughterOrder values), sorting, self links after sort, flavour from a
// JetFlavourInfoMatchingCollection, user data with stripped postfix, placeholders,
// MC switched off for real data; truth taggers.

#include "PhysicsTools/PatGenJet/interface/GenJet.h"
#include "PhysicsTools/PatGenJet/plugins/PATGenJetPluginHelpers.h"
#include "PhysicsTools/PatGenJet/test/PatGenJetTestCommon.h"

#include "DataFormats/BTauReco/interface/BaseTagInfo.h"
#include "DataFormats/Common/interface/OwnVector.h"
#include "DataFormats/PatCandidates/interface/PackedGenParticle.h"
#include "CommonTools/Utils/interface/StringObjectFunction.h"
#include "FWCore/TestProcessor/interface/TestProcessor.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
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
    return header(false, unmapped) + "process.toTest = cms.EDProducer('PATGenJetProducer',\n  jetSource = cms.InputTag('" + kInputs +
           ":jets'),\n  packedGenParticles = cms.InputTag('" + kInputs + "'),\n  JetFlavourInfoSource = cms.InputTag('" +
           kInputs + ":flavour')" + extra + ")\n" + post + "process.moduleToTest(process.toTest, cms.Task(process." + kInputs +
           "))\n";
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

TEST_CASE("PATGenJetProducer: sorting, self links, re-keying in pt order, flavour, Specific",
          "[PATGenJetProducer]") {
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

TEST_CASE("PATGenJetProducer: a constituent without packed partner is dropped (review M1)", "[PATGenJetProducer]") {
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

TEST_CASE("PATGenJetProducer: user data with stripped postfix and placeholders", "[PATGenJetProducer]") {
  const std::string extra =
      ",\n  userData = " +
      patgenjettest::userData("cms.InputTag('" + kInputs + "', 'tau1')",
                              "cms.InputTag('" + kInputs + "', 'nconst')",
                              "'ForPatGenJets'") +
      ",\n  placeholderUserFloats = cms.vstring('pileupJetIdPuppi:fullDiscriminant', 'hfsigmaEtaEta', "
      "'genInputs:tau1'),\n"
      "  placeholderUserInts = cms.vstring('hfcentralEtaStripSize'),\n"
      "  placeholderPtEtaPhiUserFloats = cms.vstring('muonSubtrRawPt', 'muonSubtrRawEta', 'muonSubtrRawPhi')";
  edm::test::TestProcessor tester(producerConfig(extra));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 4);
  for (auto const& j : out) {
    const unsigned inputIndex = j.originalObjectRef().key();
    // label = encoded InputTag with the postfix stripped (PATUserDataMerger.h:104-118)
    CHECK(j.hasUserFloat("genInputs:tau1"));
    CHECK_FALSE(j.hasUserFloat("genInputsForPatGenJets:tau1"));
    // value from the map, NOT overwritten by the placeholder of the same name
    CHECK(j.userFloat("genInputs:tau1") == Approx(0.1 * (inputIndex + 1)));
    CHECK(j.userInt("genInputs:nconst") == static_cast<int>(j.originalObject()->numberOfDaughters()));
    // reco-only labels = 0
    CHECK(j.userFloat("pileupJetIdPuppi:fullDiscriminant") == 0.f);
    CHECK(j.userFloat("hfsigmaEtaEta") == 0.f);
    CHECK(j.userInt("hfcentralEtaStripSize") == 0);
    // muon-subtraction floats = the jet's own pt, eta, phi
    CHECK(j.userFloat("muonSubtrRawPt") == static_cast<float>(j.pt()));
    CHECK(j.userFloat("muonSubtrRawEta") == static_cast<float>(j.eta()));
    CHECK(j.userFloat("muonSubtrRawPhi") == static_cast<float>(j.phi()));
    // the NanoAOD columns (jetsAK4_Puppi_cff.py:213-215) then read 0
    CHECK(std::abs(1. - j.userFloat("muonSubtrRawPt") / (j.pt() * j.jecFactor("Uncorrected"))) < 1e-6);
    CHECK(std::abs(j.userFloat("muonSubtrRawEta") - j.eta()) < 1e-6);
    CHECK(std::abs(j.userFloat("muonSubtrRawPhi") - j.phi()) < 1e-6);
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
  CHECK_THROWS_AS(
      edm::test::TestProcessor(producerConfig(",\n  placeholderPtEtaPhiUserFloats = cms.vstring('a', 'b')")),
      cms::Exception);
}

// ---------------------------------------------------------------------------------------------------------
// truth values under the reco tagger labels, checked through the real NanoAOD column expressions
// (genjet_nano_expressions.txt, as testGenJetExpressions) on the inputs of PatGenJetTruthTestInputs.
namespace {
  struct TaggerColumn {
    std::string table, column, expr;
  };
  std::vector<TaggerColumn> taggerColumns() {
    std::string path = "genjet_nano_expressions.txt";
    for (const char* var : {"CMSSW_BASE", "LOCALTOP"})
      if (const char* base = std::getenv(var)) {
        const std::string p = std::string(base) + "/src/PhysicsTools/PatGenJet/test/genjet_nano_expressions.txt";
        if (std::ifstream(p).good())
          path = p;
      }
    std::ifstream in(path);
    std::vector<TaggerColumn> out;
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#' || line.find("bDiscriminator(") == std::string::npos)
        continue;
      std::vector<std::string> f;
      size_t pos = 0;
      for (int k = 0; k < 4; ++k) {
        const size_t bar = line.find('|', pos);
        f.push_back(line.substr(pos, bar - pos));
        pos = bar + 1;
      }
      out.push_back({f[0], f[2], line.substr(pos)});
    }
    return out;
  }

  // column -> value for one jet; every bDiscriminator label read must be present (not the -1000 of a missing one)
  std::map<std::string, double> columns(pat::GenJet const& j, std::string const& table, unsigned& missing) {
    std::map<std::string, double> r;
    static const std::vector<TaggerColumn> cols = taggerColumns();
    for (auto const& c : cols) {
      if (c.table != table)
        continue;
      r[c.column] = StringObjectFunction<pat::GenJet>(c.expr)(j);
      for (size_t p = c.expr.find("bDiscriminator('"); p != std::string::npos; p = c.expr.find("bDiscriminator('", p + 1)) {
        const size_t b = p + 16;
        if (j.bDiscriminator(c.expr.substr(b, c.expr.find('\'', b) - b)) == -1000.f)
          ++missing;
      }
    }
    return r;
  }

  std::string truthConfig(std::string const& jets, std::string const& flavour, std::string const& type) {
    return std::string(R"_(from FWCore.TestProcessor.TestProcess import *
process = TestProcess()
process.truthInputs = cms.EDProducer("PatGenJetTruthTestInputs")
process.toTest = cms.EDProducer('PATGenJetProducer', jetSource = cms.InputTag('truthInputs:)_") +
           jets + "'), packedGenParticles = cms.InputTag(''), JetFlavourInfoSource = cms.InputTag('truthInputs:" + flavour +
           "'), genParticles = cms.InputTag('truthInputs'), truthJetType = cms.string('" + type +
           "'))\nprocess.moduleToTest(process.toTest, cms.Task(process.truthInputs))\n";
  }

  using Expected = std::vector<std::pair<std::string, double>>;
  void checkColumns(std::map<std::string, double> const& got, Expected const& want, std::string const& what) {
    for (auto const& [col, value] : want) {
      INFO(what << " column " << col);
      REQUIRE(got.count(col) == 1);
      if (std::isnan(value))
        CHECK(std::isnan(got.at(col)));
      else
        CHECK(got.at(col) == Approx(value));
    }
  }
  const double kNaN = std::nan("");
}  // namespace

TEST_CASE("PATGenJetProducer: AK4 truth taggers", "[PATGenJetProducer][truth]") {
  edm::test::TestProcessor tester(truthConfig("jets", "flavour", "AK4"));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 11);  // input order is pt order: index = truth case
  unsigned missing = 0;
  std::vector<std::map<std::string, double>> jet, sub;
  for (auto const& j : out) {
    CHECK(j.getPairDiscri().size() == 46);
    jet.push_back(columns(j, "Jet", missing));
    sub.push_back(columns(j, "SubJet", missing));
  }
  CHECK(missing == 0);
  CHECK(jet[0].size() == 32);
  CHECK(sub[0].size() == 8);
  // b: CvNotB is 0/0 for a one-hot b jet (NaN, documented)
  checkColumns(jet[0],
               {{"btagDeepFlavB", 1},
                {"btagDeepFlavCvB", 0},
                {"btagDeepFlavCvL", -1},
                {"btagPNetB", 1},
                {"btagPNetCvL", -1},
                {"btagPNetCvNotB", kNaN},
                {"btagUParTAK4B", 1},
                {"btagUParTAK4probb", 1},
                {"btagUParTAK4probbb", 0},
                {"btagUParTAK4UDG", 0},
                {"btagUParTAK4CvNotB", kNaN}},
               "b");
  checkColumns(sub[0], {{"btagDeepFlavB", 1}, {"btagUParTAK4B", 1}}, "b subjet");
  checkColumns(jet[1], {{"btagDeepFlavB", 1}, {"btagUParTAK4probbb", 1}, {"btagUParTAK4probb", 0}, {"btagPNetB", 1}}, "bb");
  checkColumns(jet[2],
               {{"btagDeepFlavB", 0},
                {"btagDeepFlavCvL", 1},
                {"btagDeepFlavCvB", 1},
                {"btagPNetB", -1},
                {"btagPNetCvL", 1},
                {"btagPNetCvB", 1},
                {"btagPNetCvNotB", -1},
                {"btagUParTAK4CvL", 1},
                {"btagUParTAK4CvB", 1},
                {"btagUParTAK4CvNotB", -1}},
               "c");
  checkColumns(jet[3],
               {{"btagDeepFlavQG", 0},
                {"btagDeepFlavCvL", 0},
                {"btagPNetQvG", 1},
                {"btagUParTAK4UDG", 1},
                {"btagUParTAK4QvG", 1},
                {"btagUParTAK4SvUDG", -1}},
               "u");
  checkColumns(jet[4], {{"btagUParTAK4UDG", 1}, {"btagUParTAK4QvG", 1}, {"btagUParTAK4SvUDG", -1}}, "d");
  checkColumns(jet[5],
               {{"btagUParTAK4SvUDG", 1}, {"btagUParTAK4SvCB", 1}, {"btagUParTAK4UDG", 0}, {"btagUParTAK4QvG", 1}},
               "s");
  checkColumns(jet[6],
               {{"btagDeepFlavQG", 1}, {"btagPNetQvG", 0}, {"btagUParTAK4UDG", 1}, {"btagUParTAK4QvG", -1}},
               "g");
  checkColumns(jet[7],
               {{"btagPNetTauVJet", 1},
                {"btagUParTAK4TauVJet", 1},
                {"btagUParTAK4Ele", 0},
                {"btagUParTAK4Mu", 0},
                {"btagPNetB", -1},
                {"btagDeepFlavB", 0}},
               "hadronic tau");
  checkColumns(jet[8],
               {{"btagUParTAK4Mu", 1}, {"btagUParTAK4Ele", 0}, {"btagUParTAK4TauVJet", -1}, {"btagPNetTauVJet", -1}},
               "prompt muon");
  checkColumns(jet[9], {{"btagDeepFlavB", 1}, {"btagUParTAK4probb", 0}, {"btagUParTAK4B", 1}}, "b with B-decay muon");
  CHECK(out[9].bDiscriminator("pfDeepFlavourJetTags:problepb") == 1.f);
  CHECK(out[9].bDiscriminator("pfUnifiedParticleTransformerAK4JetTags:problepb") == 1.f);
  CHECK(out[0].bDiscriminator("pfDeepFlavourJetTags:problepb") == 0.f);
  checkColumns(jet[10], {{"btagPNetQvG", 1}, {"PNetRegPtRawCorr", 1}}, "forward u");
  // regressions: "no correction"
  for (auto const& j : {jet[0], jet[6], jet[10]})
    checkColumns(j,
                 {{"PNetRegPtRawCorr", 1},
                  {"PNetRegPtRawCorrNeutrino", 1},
                  {"PNetRegPtRawRes", 0},
                  {"UParTAK4RegPtRawCorr", 1},
                  {"UParTAK4RegPtRawCorrNeutrino", 1},
                  {"UParTAK4RegPtRawRes", -1},
                  {"UParTAK4V1RegPtRawCorr", 1}},
                 "regressions");
  checkColumns(sub[3], {{"UParTAK4RegPtRawCorr", 1}, {"UParTAK4V1RegPtRawCorrNeutrino", 1}}, "subjet regressions");
}

TEST_CASE("PATGenJetProducer: AK8 truth taggers", "[PATGenJetProducer][truth]") {
  edm::test::TestProcessor tester(truthConfig("fatjets", "fatflavour", "AK8"));
  auto event = tester.test();
  auto const& out = *event.get<std::vector<pat::GenJet>>();
  REQUIRE(out.size() == 7);
  unsigned missing = 0;
  std::vector<std::map<std::string, double>> fat;
  for (auto const& j : out) {
    CHECK(j.getPairDiscri().size() == 54);
    fat.push_back(columns(j, "FatJet", missing));
  }
  CHECK(missing == 0);
  CHECK(fat[0].size() == 48);
  checkColumns(fat[0],
               {{"globalParT3_TopbWqq", 1},
                {"globalParT3_QCD", 0},
                {"globalParT3_withMassTopvsQCD", 1},
                {"globalParT3_withMassWvsQCD", 0},
                {"globalParT3_WvsQCD", -1},
                {"particleNetWithMass_TvsQCD", 1},
                {"particleNetWithMass_QCD", 0},
                {"particleNet_QCD1HF", 1},
                {"particleNetLegacy_QCD", 1}},
               "hadronic top");
  checkColumns(fat[1],
               {{"globalParT3_Xqq", 1},
                {"globalParT3_WvsQCD", 1},
                {"globalParT3_withMassWvsQCD", 1},
                {"globalParT3_withMassTopvsQCD", 0},
                {"particleNetWithMass_WvsQCD", 1},
                {"particleNet_WVsQCD", 1},
                {"particleNet_XqqVsQCD", 1},
                {"particleNetLegacy_Xqq", 1},
                {"particleNet_QCD", 0}},
               "W -> qq");
  checkColumns(fat[2],
               {{"globalParT3_Xbb", 1},
                {"globalParT3_QCD", 0},
                {"particleNetWithMass_HbbvsQCD", 1},
                {"particleNet_XbbVsQCD", 1},
                {"particleNetLegacy_Xbb", 1},
                {"particleNetLegacy_QCD", 0}},
               "H -> bb");
  checkColumns(fat[3],
               {{"globalParT3_QCD", 1},
                {"globalParT3_WvsQCD", 0},
                {"particleNet_QCD0HF", 1},
                {"particleNetWithMass_QCD", 1},
                {"particleNetLegacy_QCD", 1}},
               "QCD 0 HF");
  checkColumns(fat[4], {{"particleNet_QCD1HF", 1}, {"particleNet_QCD0HF", 0}, {"globalParT3_QCD", 1}}, "QCD 1 HF");
  checkColumns(fat[5], {{"particleNet_QCD2HF", 1}, {"particleNet_QCD1HF", 0}}, "QCD 2 HF");
  checkColumns(fat[6],
               {{"globalParT3_TopbWq", 1}, {"globalParT3_TopbWqq", 0}, {"particleNetWithMass_TvsQCD", 0}},
               "top with one W quark outside");
  for (size_t i = 0; i < out.size(); ++i)
    checkColumns(fat[i],
                 {{"globalParT3_massCorrX2p", 1},
                  {"globalParT3_massCorrGeneric", 1},
                  {"particleNet_massCorr", 1},
                  {"particleNetLegacy_mass", out[i].mass()}},
                 "regressions");
}

TEST_CASE("PATGenJetProducer: truthTaggers False stores no tagger values", "[PATGenJetProducer][truth]") {
  edm::test::TestProcessor tester(truthConfig("jets", "flavour", "AK4") + "process.toTest.truthTaggers = cms.bool(False)\n");
  auto event = tester.test();
  for (auto const& j : *event.get<std::vector<pat::GenJet>>())
    CHECK(j.getPairDiscri().empty());
}
