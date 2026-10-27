// Task F3: every expression of the NanoAOD Jet, FatJet and SubJet tables
// (expanded Run3_2026 MC configuration, list in genjet_nano_expressions.txt) evaluates on a pat::GenJet
// with StringObjectFunction<pat::GenJet>, with lazyEval false and true, without throwing, when the
// placeholder user data, the substructure user floats and the SoftDropPuppi subjets are present
// and the self links are set. Selected values are checked against the documented behaviour.

#include "PhysicsTools/PatGenJet/interface/GenJet.h"

#include "CommonTools/Utils/interface/StringObjectFunction.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/Provenance/interface/ProductID.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// DataFormats/Math/interface/Error.h defines a STATIC_CHECK macro that clashes with Catch2
#undef STATIC_CHECK
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

using Catch::Approx;

namespace {
  struct Expr {
    std::string table, module, column, type, expr;
  };

  std::string dataFile() {
    for (const char* var : {"CMSSW_BASE", "LOCALTOP"}) {
      if (const char* base = std::getenv(var)) {
        std::string p = std::string(base) + "/src/PhysicsTools/PatGenJet/test/genjet_nano_expressions.txt";
        if (std::ifstream(p).good())
          return p;
      }
    }
    return "genjet_nano_expressions.txt";
  }

  std::vector<Expr> readExpressions() {
    std::ifstream in(dataFile());
    std::vector<Expr> out;
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#')
        continue;
      Expr e;
      std::string* fields[4] = {&e.table, &e.module, &e.column, &e.type};
      size_t pos = 0;
      for (auto* f : fields) {
        const size_t bar = line.find('|', pos);
        *f = line.substr(pos, bar - pos);
        pos = bar + 1;
      }
      e.expr = line.substr(pos);  // last field, may contain '|'
      out.push_back(e);
    }
    return out;
  }

  const edm::ProductID kConstituentsPID(1, 1);
  const edm::ProductID kAK4PID(1, 2);
  const edm::ProductID kAK8PID(1, 3);
  const edm::ProductID kSubPID(1, 4);

  std::vector<reco::GenParticle> const& constituents() {
    static const std::vector<reco::GenParticle> parts = [] {
      std::vector<reco::GenParticle> v;
      for (double e : {50., 30., 15., 5.})
        v.emplace_back(0, reco::Particle::LorentzVector(0., 0., e, e), reco::Particle::Point(), 22, 1, true);
      return v;
    }();
    return parts;
  }

  pat::GenJet makeJet(double pt, double eta, double phi, double mass) {
    reco::GenJet::Specific s;
    s.m_EmEnergy = 30.f;
    s.m_HadEnergy = 60.f;
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
    reco::Particle::PolarLorentzVector pp4(pt, eta, phi, mass);
    reco::GenJet rj(reco::Particle::LorentzVector(pp4.px(), pp4.py(), pp4.pz(), pp4.energy()),
                    reco::Particle::Point(0, 0, 0),
                    s);
    auto const& parts = constituents();
    for (size_t i = 0; i < parts.size(); ++i)
      rj.addDaughter(reco::CandidatePtr(kConstituentsPID, &parts[i], i));
    pat::GenJet j(rj);
    j.setPartonFlavour(5);
    j.setHadronFlavour(5);
    return j;
  }

  template <typename C>
  void setSelfLinks(C& coll, const edm::ProductID& pid) {
    for (size_t i = 0; i < coll.size(); ++i) {
      pat::GenJetRef self(pid, &coll[i], i);
      coll[i].setGenJetRef(pat::GenJetFwdRef(self, self));
    }
  }

  // The three "events": AK4 jets, AK8 jets with SoftDropPuppi subjets, and the subjets.
  struct Inputs {
    std::vector<pat::GenJet> ak4, ak8, sub;
    Inputs() {
      ak4 = {makeJet(60., 1.0, 0.5, 8.), makeJet(25., -2.0, 2.0, 4.)};
      sub = {makeJet(180., 0.30, 1.00, 15.), makeJet(90., 0.40, 1.30, 10.)};
      ak8 = {makeJet(300., 0.35, 1.10, 90.), makeJet(150., -1.5, -2.0, 40.)};
      //  (value 0) and  (muon subtraction = the jet's own pt, eta, phi)
      for (auto& j : ak4) {
        j.addUserFloat("pileupJetIdPuppi:fullDiscriminant", 0.f);
        j.addUserFloat("hfsigmaEtaEta", 0.f);
        j.addUserFloat("hfsigmaPhiPhi", 0.f);
        j.addUserInt("hfcentralEtaStripSize", 0);
        j.addUserInt("hfadjacentEtaStripsSize", 0);
        j.addUserFloat("muonSubtrRawPt", j.pt());
        j.addUserFloat("muonSubtrRawEta", j.eta());
        j.addUserFloat("muonSubtrRawPhi", j.phi());
      }
      // substructure user floats under the reco labels (D2 3)
      for (auto& j : ak8) {
        for (int n = 1; n <= 4; ++n)
          j.addUserFloat("NjettinessAK8Puppi:tau" + std::to_string(n), 0.1f * n);
        j.addUserFloat("nb1AK8PuppiSoftDrop:ecfN2", 0.2f);
        j.addUserFloat("nb1AK8PuppiSoftDrop:ecfN3", 1.5f);
      }
      for (auto& j : sub) {
        for (int n = 1; n <= 4; ++n)
          j.addUserFloat("NjettinessAK8Subjets:tau" + std::to_string(n), 0.05f * n);
        j.addUserFloat("nb1AK8PuppiSoftDropSubjets:ecfN2", 0.3f);
        j.addUserFloat("nb1AK8PuppiSoftDropSubjets:ecfN3", 2.0f);
      }
      // subjets of the first fat jet; the second fat jet has none
      ak8[0].addSubjets({edm::Ptr<pat::GenJet>(kSubPID, &sub[0], 0), edm::Ptr<pat::GenJet>(kSubPID, &sub[1], 1)},
                        "SoftDropPuppi");
      setSelfLinks(ak4, kAK4PID);
      setSelfLinks(ak8, kAK8PID);
      setSelfLinks(sub, kSubPID);
    }
    std::vector<pat::GenJet> const& forTable(const std::string& t) const {
      return t == "Jet" ? ak4 : (t == "FatJet" ? ak8 : sub);
    }
  };

  bool isPureBDiscriminator(const std::string& e) {
    return e.rfind("bDiscriminator('", 0) == 0 && e.back() == ')' && e.find(')') == e.size() - 1;
  }
}  // namespace

TEST_CASE("expression list is complete", "[expressions]") {
  const auto exprs = readExpressions();
  std::map<std::string, int> perTable;
  for (auto const& e : exprs)
    ++perTable[e.table];
  // 170 columns = 167 string expressions + 3 FatJet ExtVar (ValueMap) columns (D2 section 0)
  CHECK(exprs.size() == 167);
  CHECK(perTable["Jet"] == 68);
  CHECK(perTable["FatJet"] == 75);
  CHECK(perTable["SubJet"] == 24);
}

TEST_CASE("all NanoAOD jet expressions evaluate on pat::GenJet", "[expressions]") {
  const auto exprs = readExpressions();
  REQUIRE(exprs.size() == 167);
  const Inputs in;

  for (bool lazy : {false, true}) {
    DYNAMIC_SECTION((lazy ? "lazyEval=True" : "lazyEval=False")) {
      // values[table][column][row]
      std::map<std::string, std::map<std::string, std::vector<double>>> values;
      int failures = 0;
      for (auto const& e : exprs) {
        auto const& jets = in.forTable(e.table);
        for (size_t row = 0; row < jets.size(); ++row) {
          try {
            StringObjectFunction<pat::GenJet> f(e.expr, lazy);
            values[e.table][e.column].push_back(f(jets[row]));
          } catch (cms::Exception const& ex) {
            ++failures;
            UNSCOPED_INFO(e.table << "_" << e.column << " row " << row << ": " << ex.category() << " "
                                  << ex.explainSelf());
          } catch (std::exception const& ex) {
            ++failures;
            UNSCOPED_INFO(e.table << "_" << e.column << " row " << row << ": std::exception " << ex.what());
          }
        }
      }
      REQUIRE(failures == 0);

      auto const& jet = values["Jet"];
      auto const& fat = values["FatJet"];
      auto const& sub = values["SubJet"];
      // JEC factor 1: rawFactor = 0
      for (auto const* t : {&jet, &fat, &sub})
        for (double v : t->at("rawFactor"))
          CHECK(v == 0.);
      // pure b-tag columns: -1000
      for (auto const& e : exprs)
        if (isPureBDiscriminator(e.expr))
          for (double v : values[e.table][e.column])
            CHECK(v == -1000.);
      // self links: own row; cuts of the expressions (pt > 10 / pt > 100) respected
      CHECK(jet.at("genJetIdx") == std::vector<double>{0., 1.});
      CHECK(fat.at("genJetAK8Idx") == std::vector<double>{0., 1.});
      CHECK(sub.at("subGenJetAK8Idx") == std::vector<double>{0., 1.});
      // muon-subtraction columns read 0
      for (double v : jet.at("muonSubtrFactor"))
        CHECK(v == Approx(0.).margin(1e-6));
      for (double v : jet.at("muonSubtrDeltaEta"))
        CHECK(v == Approx(0.).margin(1e-6));
      for (double v : jet.at("muonSubtrDeltaPhi"))
        CHECK(v == Approx(0.).margin(1e-6));
      // placeholders 0
      CHECK(jet.at("puIdDisc") == std::vector<double>{0., 0.});
      // PF-named fractions real on AK4 (Jet table has no isPFJet guard)
      CHECK(jet.at("chHEF")[0] == Approx(in.ak4[0].chargedHadronEnergyFraction()));
      CHECK(jet.at("chHEF")[0] > 0.);
      CHECK(jet.at("chMultiplicity")[0] == 9.);
      CHECK(jet.at("neMultiplicity")[0] == 7.);
      CHECK(jet.at("hfHEF")[0] == 0.);
      CHECK(jet.at("partonFlavour")[0] == 5.);
      CHECK(jet.at("nConstituents")[0] == 4.);
      // FatJet PF columns are guarded with isPFJet(), false for a gen jet: -1 as for pat::Jet
      CHECK(fat.at("chHEF")[0] == -1.);
      // subjets and groomed mass (2.6)
      const double sdMass = (in.sub[0].p4() + in.sub[1].p4()).mass();
      CHECK(fat.at("msoftdrop")[0] == Approx(sdMass));
      CHECK(fat.at("msoftdrop")[1] == -1.);  // no subjets: 
      CHECK(fat.at("subJetIdx1") == std::vector<double>{0., -1.});
      CHECK(fat.at("subJetIdx2") == std::vector<double>{1., -1.});
      CHECK(fat.at("tau2")[0] == Approx(0.2));
      CHECK(fat.at("n2b1")[0] == Approx(0.2));
      CHECK(sub.at("tau1")[1] == Approx(0.05));
      CHECK(sub.at("n3b1")[0] == Approx(2.0));
      CHECK(sub.at("hadronFlavour")[0] == 5.);
      CHECK(sub.at("nBHadrons")[0] == 0.);
      // p4 columns are the gen-jet kinematics
      CHECK(jet.at("pt")[0] == Approx(60.));
      CHECK(fat.at("pt")[0] == Approx(300.));
    }
  }
}
