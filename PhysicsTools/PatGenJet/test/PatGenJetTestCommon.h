#ifndef PhysicsTools_PatGenJet_test_PatGenJetTestCommon_h
#define PhysicsTools_PatGenJet_test_PatGenJetTestCommon_h
// Shared pieces of the P4 TestProcessor tests.

#include "PhysicsTools/PatGenJet/interface/GenJet.h"

#include <string>
#include <vector>

namespace patgenjettest {

  // The input module (test/PatGenJetTestInputs.cc). Its label ends in "ForPatGenJets", so that
  // labelPostfixesToStrip can be tested.
  inline std::string header(bool sortJets = false, bool unmappedConstituent = false) {
    return std::string(R"_(from FWCore.TestProcessor.TestProcess import *
process = TestProcess()
process.genInputsForPatGenJets = cms.EDProducer("PatGenJetTestInputs", sortJets = cms.untracked.bool()_") +
           (sortJets ? "True" : "False") + "), unmappedConstituent = cms.untracked.bool(" +
           (unmappedConstituent ? "True" : "False") + "))\n";
  }

  // userData block with every PSet that PATUserDataHelper::fillDescription requires
  inline std::string userData(std::string const& floats, std::string const& ints, std::string const& strip) {
    return "cms.PSet(userFloats = cms.PSet(src = cms.VInputTag(" + floats +
           "), labelPostfixesToStrip = cms.vstring(" + strip + ")),\n" + "  userInts = cms.PSet(src = cms.VInputTag(" +
           ints + "), labelPostfixesToStrip = cms.vstring(" + strip + ")),\n" +
           "  userClasses = cms.PSet(src = cms.VInputTag()), userCands = cms.PSet(src = cms.VInputTag()),\n"
           "  userFunctions = cms.vstring(), userFunctionLabels = cms.vstring())";
  }

  inline std::vector<double> pts(std::vector<pat::GenJet> const& v) {
    std::vector<double> r;
    for (auto const& j : v)
      r.push_back(j.pt());
    return r;
  }
  template <typename C>
  inline std::vector<double> ptsOf(C const& v) {
    std::vector<double> r;
    for (auto const& j : v)
      r.push_back(j.pt());
    return r;
  }

  // entry i links to itself in the product the module put (forward and back ref).
  // Returns the number of entries that do NOT; .get() resolves through the event, so the address
  // check proves the product ID is that of the output product.
  inline unsigned badSelfLinks(std::vector<pat::GenJet> const& v) {
    unsigned bad = 0;
    for (size_t i = 0; i < v.size(); ++i) {
      auto const& l = v[i].genJetFwdRef();
      if (l.isNull() || l.backRef().isNull() || l.ref().key() != i || l.backRef().key() != i ||
          l.ref().id() != l.backRef().id() || l.get() != &v[i] || &*l.backRef() != &v[i] || v[i].genJet() != &v[i])
        ++bad;
    }
    return bad;
  }

}  // namespace patgenjettest

#endif
