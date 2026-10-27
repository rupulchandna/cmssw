#ifndef PhysicsTools_PatGenJet_plugins_PATGenJetPluginHelpers_h
#define PhysicsTools_PatGenJet_plugins_PATGenJetPluginHelpers_h
//
// Small helpers shared by the PatGenJet plugins (header-only so that the unit tests can include them).
//
#include "PhysicsTools/PatGenJet/interface/GenJet.h"
#include "DataFormats/Common/interface/RefProd.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include <string>
#include <vector>

namespace pat::genjethelpers {

  /// forward and back ref of entry i point to entry i of the collection
  /// that is about to be put (RefProd from Event::getRefBeforePut). Call after the final sort.
  inline void setSelfLinks(pat::GenJetCollection& jets, edm::RefProd<pat::GenJetCollection> const& prod) {
    for (size_t i = 0, n = jets.size(); i < n; ++i) {
      pat::GenJetRef self(prod, i);
      jets[i].setGenJetRef(pat::GenJetFwdRef(self, self));
    }
  }

  /// MC switches of PATGenJetProducer. On real data they are switched off for that event,
  /// as PATJetProducer.cc:240 does (there by overwriting the members; here per event).
  struct MCSwitches {
    bool getJetMCFlavour = false;
    bool useLegacyJetMCFlavour = false;
    bool addJetFlavourInfo = false;
    bool addGenPartonMatch = false;
    MCSwitches forEvent(bool isRealData) const { return isRealData ? MCSwitches() : *this; }
  };

  /// user-data label of an InputTag after stripping postfixes, as PATUserDataMerger.h:110-117
  inline std::string strippedLabel(edm::InputTag tag, std::vector<std::string> const& postfixes) {
    for (std::string const& stripme : postfixes) {
      auto match = tag.label().rfind(stripme);
      if (match != std::string::npos && match == (tag.label().length() - stripme.length())) {
        tag = edm::InputTag(tag.label().substr(0, match), tag.instance(), tag.process());
      }
    }
    return tag.encode();
  }

}  // namespace pat::genjethelpers

#endif
