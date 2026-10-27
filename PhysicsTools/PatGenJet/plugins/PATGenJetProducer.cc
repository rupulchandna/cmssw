// -*- C++ -*-
//
// Package:     PhysicsTools/PatGenJet
// Class:       PATGenJetProducer
//
// reco::GenJet -> pat::GenJet (built from AOD-level generator jets).
// Mirrors PhysicsTools/PatAlgos/plugins/PATJetProducer.cc (CMSSW_20_1_0_pre3):
//   loop over View<reco::GenJet>, build from refAt(idx), fill, add user data, sort by pt,
//   THEN set the self links, then put.
// Differences to PATJetProducer: no tower/PF embedding, no gen-jet match (the self
// link replaces it), no jet ID, no parton-jet match, no tracks, no JEC; new parameters
// packedGenParticles (constituent re-keying as PATGenJetSlimmer), daughterOrder, and the
// placeholder user data of placeholder user data; truth values under the reco tagger labels.
//
#include "PhysicsTools/PatGenJet/interface/GenJet.h"
#include "PhysicsTools/PatGenJet/plugins/PATGenJetPluginHelpers.h"
#include "PhysicsTools/PatGenJet/plugins/GenJetTruthTagger.h"

#include "CommonTools/Utils/interface/PtComparator.h"
#include "DataFormats/BTauReco/interface/BaseTagInfo.h"
#include "DataFormats/Common/interface/Association.h"
#include "DataFormats/Common/interface/OwnVector.h"
#include "DataFormats/Common/interface/RefToPtr.h"
#include "DataFormats/Common/interface/View.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/HepMCCandidate/interface/GenParticleFwd.h"
#include "DataFormats/JetMatching/interface/JetFlavourInfoMatching.h"
#include "DataFormats/JetMatching/interface/JetFlavourMatching.h"
#include "DataFormats/JetReco/interface/JetFloatAssociation.h"
#include "DataFormats/PatCandidates/interface/PackedGenParticle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ParameterSet/interface/allowedValues.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/transform.h"
#include "PhysicsTools/PatAlgos/interface/EfficiencyLoader.h"
#include "PhysicsTools/PatAlgos/interface/KinResolutionsLoader.h"
#include "PhysicsTools/PatAlgos/interface/PATUserDataHelper.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace pat {

  class PATGenJetProducer : public edm::stream::EDProducer<> {
  public:
    explicit PATGenJetProducer(const edm::ParameterSet& iConfig);
    ~PATGenJetProducer() override = default;

    void produce(edm::Event& iEvent, const edm::EventSetup& iSetup) override;

    static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

  private:
    enum class DaughterOrder { pt, key };

    void rekeyDaughters(pat::GenJet& jet,
                        const edm::Association<std::vector<pat::PackedGenParticle>>* gp2pgp) const;
    void addPlaceholders(pat::GenJet& jet) const;

    // inputs
    const edm::EDGetTokenT<edm::View<reco::GenJet>> jetsToken_;
    const bool rekey_;
    edm::EDGetTokenT<edm::Association<std::vector<pat::PackedGenParticle>>> gp2pgpToken_;
    DaughterOrder daughterOrder_;

    // MC (switched off per event on real data)
    pat::genjethelpers::MCSwitches mc_;
    bool embedGenPartonMatch_;
    edm::EDGetTokenT<edm::Association<reco::GenParticleCollection>> genPartonToken_;
    edm::EDGetTokenT<reco::JetFlavourMatchingCollection> jetPartonMapToken_;
    edm::EDGetTokenT<reco::JetFlavourInfoMatchingCollection> jetFlavourInfoToken_;

    // jet charge
    bool addJetCharge_;
    edm::EDGetTokenT<reco::JetFloatAssociation::Container> jetChargeToken_;

    // b tagging
    bool addBTagInfo_;
    bool addDiscriminators_;
    std::vector<edm::EDGetTokenT<reco::JetFloatAssociation::Container>> discriminatorTokens_;
    std::vector<std::string> discriminatorLabels_;
    bool addTagInfos_;
    std::vector<edm::EDGetTokenT<edm::View<reco::BaseTagInfo>>> tagInfoTokens_;
    std::vector<std::string> tagInfoLabels_;

    // generic PAT
    bool addEfficiencies_;
    pat::helper::EfficiencyLoader efficiencyLoader_;
    bool addResolutions_;
    pat::helper::KinResolutionsLoader resolutionLoader_;
    const bool useUserData_;
    pat::PATUserDataHelper<pat::GenJet> userDataHelper_;

    // placeholder user data
    const std::vector<std::string> placeholderUserFloats_;
    const std::vector<std::string> placeholderUserInts_;
    const std::vector<std::string> placeholderPtEtaPhiUserFloats_;

    // truth taggers
    const bool truthTaggers_;
    pat::GenJetTruthTagger::JetType truthJetType_;
    edm::EDGetTokenT<reco::GenParticleCollection> genParticlesToken_;

    GreaterByPt<pat::GenJet> pTComparator_;

    // review M1: constituents without a packed partner seen so far (warnings are limited per stream)
    static constexpr unsigned kMaxUnmappedWarnings = 10;
    mutable unsigned nUnmapped_ = 0;
  };

}  // namespace pat

using namespace pat;

PATGenJetProducer::PATGenJetProducer(const edm::ParameterSet& iConfig)
    : jetsToken_(consumes<edm::View<reco::GenJet>>(iConfig.getParameter<edm::InputTag>("jetSource"))),
      rekey_(!iConfig.getParameter<edm::InputTag>("packedGenParticles").label().empty()),
      useUserData_(iConfig.exists("userData")),
      placeholderUserFloats_(iConfig.getParameter<std::vector<std::string>>("placeholderUserFloats")),
      placeholderUserInts_(iConfig.getParameter<std::vector<std::string>>("placeholderUserInts")),
      placeholderPtEtaPhiUserFloats_(iConfig.getParameter<std::vector<std::string>>("placeholderPtEtaPhiUserFloats")),
      truthTaggers_(iConfig.getParameter<bool>("truthTaggers")),
      truthJetType_(iConfig.getParameter<std::string>("truthJetType") == "AK8" ? pat::GenJetTruthTagger::JetType::AK8
                                                                               : pat::GenJetTruthTagger::JetType::AK4) {
  // ---- generator particles for taus, leptons and resonances (empty label = not used)
  const auto genParticlesTag = iConfig.getParameter<edm::InputTag>("genParticles");
  if (truthTaggers_ && !genParticlesTag.label().empty())
    genParticlesToken_ = consumes<reco::GenParticleCollection>(genParticlesTag);
  // ---- constituents
  if (rekey_)
    gp2pgpToken_ = consumes<edm::Association<std::vector<pat::PackedGenParticle>>>(
        iConfig.getParameter<edm::InputTag>("packedGenParticles"));
  const std::string order = iConfig.getParameter<std::string>("daughterOrder");
  if (order == "pt")
    daughterOrder_ = DaughterOrder::pt;
  else if (order == "key")
    daughterOrder_ = DaughterOrder::key;
  else
    throw edm::Exception(edm::errors::Configuration) << "PATGenJetProducer: daughterOrder must be \"pt\" or \"key\", got \""
                                                     << order << "\"";

  // ---- MC flavour and parton match (as PATJetProducer.cc:128-148)
  mc_.getJetMCFlavour = iConfig.getParameter<bool>("getJetMCFlavour");
  // flavour stays at its default when no flavour input is configured (empty source label)
  const auto flavourSource = iConfig.getParameter<edm::InputTag>(
      iConfig.getParameter<bool>("useLegacyJetMCFlavour") ? "JetPartonMapSource" : "JetFlavourInfoSource");
  if (flavourSource.label().empty()) {
    // review M4: do not switch off silently
    if (mc_.getJetMCFlavour)
      edm::LogWarning("PATGenJetProducer")
          << "getJetMCFlavour is True but "
          << (iConfig.getParameter<bool>("useLegacyJetMCFlavour") ? "JetPartonMapSource" : "JetFlavourInfoSource")
          << " is empty: the flavour of the jets is not filled (hadronFlavour = partonFlavour = 0)";
    mc_.getJetMCFlavour = false;
  }
  mc_.useLegacyJetMCFlavour = iConfig.getParameter<bool>("useLegacyJetMCFlavour");
  mc_.addJetFlavourInfo = (mc_.useLegacyJetMCFlavour ? false : iConfig.getParameter<bool>("addJetFlavourInfo"));
  if (mc_.getJetMCFlavour && mc_.useLegacyJetMCFlavour)
    jetPartonMapToken_ =
        consumes<reco::JetFlavourMatchingCollection>(iConfig.getParameter<edm::InputTag>("JetPartonMapSource"));
  else if (mc_.getJetMCFlavour && !mc_.useLegacyJetMCFlavour)
    jetFlavourInfoToken_ =
        consumes<reco::JetFlavourInfoMatchingCollection>(iConfig.getParameter<edm::InputTag>("JetFlavourInfoSource"));
  mc_.addGenPartonMatch = iConfig.getParameter<bool>("addGenPartonMatch");
  embedGenPartonMatch_ = iConfig.getParameter<bool>("embedGenPartonMatch");
  if (mc_.addGenPartonMatch)
    genPartonToken_ =
        consumes<edm::Association<reco::GenParticleCollection>>(iConfig.getParameter<edm::InputTag>("genPartonMatch"));

  // ---- jet charge
  addJetCharge_ = iConfig.getParameter<bool>("addJetCharge");
  if (addJetCharge_)
    jetChargeToken_ =
        consumes<reco::JetFloatAssociation::Container>(iConfig.getParameter<edm::InputTag>("jetChargeSource"));

  // ---- b tagging (as PATJetProducer.cc:155-215)
  addBTagInfo_ = iConfig.getParameter<bool>("addBTagInfo");
  addDiscriminators_ = iConfig.getParameter<bool>("addDiscriminators");
  const auto discriminatorTags = iConfig.getParameter<std::vector<edm::InputTag>>("discriminatorSources");
  discriminatorTokens_ = edm::vector_transform(discriminatorTags, [this](edm::InputTag const& tag) {
    return mayConsume<reco::JetFloatAssociation::Container>(tag);
  });
  addTagInfos_ = iConfig.getParameter<bool>("addTagInfos");
  const auto tagInfoTags = iConfig.getParameter<std::vector<edm::InputTag>>("tagInfoSources");
  tagInfoTokens_ = edm::vector_transform(
      tagInfoTags, [this](edm::InputTag const& tag) { return mayConsume<edm::View<reco::BaseTagInfo>>(tag); });
  if (discriminatorTags.empty()) {
    addDiscriminators_ = false;
  } else {
    for (auto const& tag : discriminatorTags) {
      std::string label = tag.label();
      std::string::size_type pos = label.find("JetTags");
      if ((pos != std::string::npos) && (pos != label.length() - 7))
        label.erase(pos + 7);  // trim a tail after "JetTags"
      if (!tag.instance().empty())
        label = (label + std::string(":") + tag.instance());
      discriminatorLabels_.push_back(label);
    }
  }
  if (tagInfoTags.empty()) {
    addTagInfos_ = false;
  } else {
    for (auto const& tag : tagInfoTags) {
      std::string label = tag.label();
      std::string::size_type pos = label.find("TagInfos");
      if ((pos != std::string::npos) && (pos != label.length() - 8))
        label.erase(pos + 8);  // trim a tail after "TagInfos"
      tagInfoLabels_.push_back(label);
    }
  }
  if (!addBTagInfo_) {
    addDiscriminators_ = false;
    addTagInfos_ = false;
  }

  // ---- generic PAT
  addEfficiencies_ = iConfig.getParameter<bool>("addEfficiencies");
  if (addEfficiencies_)
    efficiencyLoader_ =
        pat::helper::EfficiencyLoader(iConfig.getParameter<edm::ParameterSet>("efficiencies"), consumesCollector());
  addResolutions_ = iConfig.getParameter<bool>("addResolutions");
  if (addResolutions_)
    resolutionLoader_ =
        pat::helper::KinResolutionsLoader(iConfig.getParameter<edm::ParameterSet>("resolutions"), consumesCollector());
  if (useUserData_)
    userDataHelper_ =
        PATUserDataHelper<pat::GenJet>(iConfig.getParameter<edm::ParameterSet>("userData"), consumesCollector());

  if (!placeholderPtEtaPhiUserFloats_.empty() && placeholderPtEtaPhiUserFloats_.size() != 3)
    throw edm::Exception(edm::errors::Configuration)
        << "PATGenJetProducer: placeholderPtEtaPhiUserFloats needs 0 or 3 labels (pt, eta, phi), got "
        << placeholderPtEtaPhiUserFloats_.size();

  produces<std::vector<pat::GenJet>>();
  produces<edm::OwnVector<reco::BaseTagInfo>>("tagInfos");
}

/// constituents mapped through the genParticles -> packedGenParticles association
/// (PATGenJetSlimmer.cc:111). "pt": input order kept (AOD gen jets are pt-ordered), later duplicates
/// dropped; "key": packed-key order, as PATGenJetSlimmer (map iteration, PATGenJetSlimmer.cc:114).
/// Without re-keying the original daughters are kept, ordered the same way.
void PATGenJetProducer::rekeyDaughters(pat::GenJet& jet,
                                       const edm::Association<std::vector<pat::PackedGenParticle>>* gp2pgp) const {
  const reco::CompositePtrCandidate::daughters old = jet.daughterPtrVector();
  std::vector<reco::CandidatePtr> mapped;
  mapped.reserve(old.size());
  for (auto const& dau : old) {
    if (gp2pgp) {
      auto const ref = (*gp2pgp)[dau];
      if (ref.isNonnull()) {
        mapped.push_back(edm::refToPtr(ref));
      } else {
        // a null mapping is dropped (PATGenJetSlimmer would add a null Ptr); nConstituents can then differ from
        // slimmedGenJets (review M1). Reported for the first few cases of each stream, to keep the log small.
        if (nUnmapped_++ < kMaxUnmappedWarnings)
          edm::LogWarning("PATGenJetProducer")
              << "constituent (product " << dau.id() << ", key " << dau.key() << ") of a jet with pt " << jet.pt()
              << " has no packed partner in the packedGenParticles association: dropped from the daughters"
              << (nUnmapped_ == kMaxUnmappedWarnings ? " (further occurrences of this warning are not printed)" : "");
      }
    } else {
      mapped.push_back(dau);
    }
  }
  std::vector<reco::CandidatePtr> ordered;
  ordered.reserve(mapped.size());
  if (daughterOrder_ == DaughterOrder::key) {
    std::map<unsigned int, reco::CandidatePtr> byKey;
    for (auto const& p : mapped)
      byKey.emplace(p.key(), p);  // first wins; same key = same packed particle
    for (auto const& kv : byKey)
      ordered.push_back(kv.second);
  } else {
    std::set<std::pair<edm::ProductID, unsigned int>> seen;
    for (auto const& p : mapped)
      if (seen.emplace(p.id(), p.key()).second)
        ordered.push_back(p);
  }
  jet.clearDaughters();
  for (auto const& p : ordered)
    jet.addDaughter(p);
}

/// labels that NanoAOD reads but no generator module provides.
/// Written only when not already present (e.g. from userData).
void PATGenJetProducer::addPlaceholders(pat::GenJet& jet) const {
  for (auto const& label : placeholderUserFloats_)
    if (!jet.hasUserFloat(label))
      jet.addUserFloat(label, 0.f);
  for (auto const& label : placeholderUserInts_)
    if (!jet.hasUserInt(label))
      jet.addUserInt(label, 0);
  if (!placeholderPtEtaPhiUserFloats_.empty()) {
    const float values[3] = {float(jet.pt()), float(jet.eta()), float(jet.phi())};
    for (size_t i = 0; i < 3; ++i)
      if (!jet.hasUserFloat(placeholderPtEtaPhiUserFloats_[i]))
        jet.addUserFloat(placeholderPtEtaPhiUserFloats_[i], values[i]);
  }
}

void PATGenJetProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  // PATJetProducer.cc:240: MC inputs off on real data
  const pat::genjethelpers::MCSwitches mc = mc_.forEvent(iEvent.isRealData());

  edm::Handle<edm::View<reco::GenJet>> jets;
  iEvent.getByToken(jetsToken_, jets);

  if (efficiencyLoader_.enabled())
    efficiencyLoader_.newEvent(iEvent);
  if (resolutionLoader_.enabled())
    resolutionLoader_.newEvent(iEvent, iSetup);

  const edm::Association<std::vector<pat::PackedGenParticle>>* gp2pgp = nullptr;
  if (rekey_)
    gp2pgp = &iEvent.get(gp2pgpToken_);

  edm::Handle<reco::JetFlavourMatchingCollection> jetFlavMatch;
  edm::Handle<reco::JetFlavourInfoMatchingCollection> jetFlavInfoMatch;
  if (mc.getJetMCFlavour && mc.useLegacyJetMCFlavour)
    iEvent.getByToken(jetPartonMapToken_, jetFlavMatch);
  else if (mc.getJetMCFlavour && !mc.useLegacyJetMCFlavour)
    iEvent.getByToken(jetFlavourInfoToken_, jetFlavInfoMatch);

  edm::Handle<edm::Association<reco::GenParticleCollection>> partonMatch;
  if (mc.addGenPartonMatch)
    iEvent.getByToken(genPartonToken_, partonMatch);

  std::vector<edm::Handle<reco::JetFloatAssociation::Container>> jetDiscriminators;
  if (addBTagInfo_ && addDiscriminators_) {
    jetDiscriminators.resize(discriminatorTokens_.size());
    for (size_t i = 0; i < discriminatorTokens_.size(); ++i)
      iEvent.getByToken(discriminatorTokens_[i], jetDiscriminators[i]);
  }
  std::vector<edm::Handle<edm::View<reco::BaseTagInfo>>> jetTagInfos;
  if (addBTagInfo_ && addTagInfos_) {
    jetTagInfos.resize(tagInfoTokens_.size());
    for (size_t i = 0; i < tagInfoTokens_.size(); ++i)
      iEvent.getByToken(tagInfoTokens_[i], jetTagInfos[i]);
  }

  edm::Handle<reco::JetFloatAssociation::Container> hJetChargeAss;
  if (addJetCharge_)
    iEvent.getByToken(jetChargeToken_, hJetChargeAss);

  // generator taus, leptons and resonances, once per event (none on real data or without genParticles)
  pat::GenJetTruthTagger::EventTruth truth;
  if (truthTaggers_ && !iEvent.isRealData() && !genParticlesToken_.isUninitialized())
    truth = pat::GenJetTruthTagger::eventTruth(&iEvent.get(genParticlesToken_));

  auto patJets = std::make_unique<std::vector<pat::GenJet>>();
  patJets->reserve(jets->size());
  auto tagInfosOut = std::make_unique<edm::OwnVector<reco::BaseTagInfo>>();
  const edm::RefProd<edm::OwnVector<reco::BaseTagInfo>> h_tagInfosOut =
      iEvent.getRefBeforePut<edm::OwnVector<reco::BaseTagInfo>>("tagInfos");
  const edm::RefProd<pat::GenJetCollection> h_jetsOut = iEvent.getRefBeforePut<pat::GenJetCollection>();

  for (size_t idx = 0, n = jets->size(); idx < n; ++idx) {
    const edm::RefToBase<reco::GenJet> genJetRef = jets->refAt(idx);
    // key for the maps keyed to reco::Jet (flavour, JetFloatAssociation): same product ID and key
    const edm::Ptr<reco::Jet> jetPtr(jets->ptrAt(idx));
    pat::GenJet ajet(genJetRef);

    rekeyDaughters(ajet, gp2pgp);

    // flavour, as PATJetProducer.cc:419-430
    if (mc.getJetMCFlavour && mc.useLegacyJetMCFlavour) {
      ajet.setPartonFlavour((*jetFlavMatch)[jetPtr].getFlavour());
    } else if (mc.getJetMCFlavour && !mc.useLegacyJetMCFlavour) {
      const reco::JetFlavourInfo& info = (*jetFlavInfoMatch)[jetPtr];
      if (mc.addJetFlavourInfo) {
        ajet.setJetFlavourInfo(info);
      } else {
        ajet.setPartonFlavour(info.getPartonFlavour());
        ajet.setHadronFlavour(info.getHadronFlavour());
      }
    }
    if (mc.addGenPartonMatch) {
      reco::GenParticleRef parton = (*partonMatch)[genJetRef];
      if (parton.isNonnull() && parton.isAvailable())
        ajet.setGenParton(parton, embedGenPartonMatch_);
    }

    if (efficiencyLoader_.enabled())
      efficiencyLoader_.setEfficiencies(ajet, genJetRef);
    if (resolutionLoader_.enabled())
      resolutionLoader_.setResolutions(ajet);

    if (addBTagInfo_) {
      if (addDiscriminators_) {
        for (size_t k = 0; k < jetDiscriminators.size(); ++k) {
          float value = (*jetDiscriminators[k])[jetPtr];
          ajet.addBDiscriminatorPair(std::make_pair(discriminatorLabels_[k], value));
        }
      }
      if (addTagInfos_) {
        for (size_t k = 0; k < jetTagInfos.size(); ++k) {
          const edm::View<reco::BaseTagInfo>& taginfos = *jetTagInfos[k];
          auto sameJet = [&jetPtr](reco::BaseTagInfo const& ti) {
            return ti.jet().id() == jetPtr.id() && ti.jet().key() == jetPtr.key();
          };
          edm::Ptr<reco::BaseTagInfo> match;
          if ((idx < taginfos.size()) && sameJet(taginfos[idx])) {
            match = taginfos.ptrAt(idx);
          } else {
            for (size_t t = 0; t < taginfos.size(); ++t) {
              if (sameJet(taginfos[t])) {
                match = taginfos.ptrAt(t);
                break;
              }
            }
          }
          if (match.isNonnull()) {
            tagInfosOut->push_back(match->clone());
            edm::Ptr<reco::BaseTagInfo> tagInfoForwardPtr(
                h_tagInfosOut.id(), &tagInfosOut->back(), tagInfosOut->size() - 1);
            ajet.addTagInfo(tagInfoLabels_[k], TagInfoFwdPtrCollection::value_type(tagInfoForwardPtr, match));
          }
        }
      }
    }

    // truth values under the reco tagger labels (after the flavour, which they read)
    if (truthTaggers_ && !iEvent.isRealData())
      for (auto const& v : pat::GenJetTruthTagger::values(ajet, truth, truthJetType_))
        ajet.addBDiscriminatorPair(v);

    if (addJetCharge_)
      ajet.setJetCharge((*hJetChargeAss)[jetPtr]);

    if (useUserData_)
      userDataHelper_.add(ajet, iEvent, iSetup);
    addPlaceholders(ajet);

    patJets->push_back(std::move(ajet));
  }

  std::sort(patJets->begin(), patJets->end(), pTComparator_);
  // after the sort, so that entry i links to itself
  pat::genjethelpers::setSelfLinks(*patJets, h_jetsOut);

  iEvent.put(std::move(patJets));
  iEvent.put(std::move(tagInfosOut), "tagInfos");
}

void PATGenJetProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  // Parameter set of PATJetProducer::fillDescriptions (PATJetProducer.cc:544-614).
  edm::ParameterSetDescription iDesc;
  iDesc.setComment("pat::GenJet producer (PhysicsTools/PatGenJet)");

  iDesc.add<edm::InputTag>("jetSource", edm::InputTag("no default"))->setComment("input View<reco::GenJet>");
  iDesc.add<edm::InputTag>("packedGenParticles", edm::InputTag("packedGenParticles"))
      ->setComment(
          "Association<vector<PackedGenParticle>> keyed to the constituents' collection; empty label = no re-keying");
  iDesc.ifValue(edm::ParameterDescription<std::string>("daughterOrder", "pt", true),
                edm::allowedValues<std::string>("pt", "key"))
      ->setComment("\"pt\": input (pt) order, duplicates dropped; \"key\": packed-key order as PATGenJetSlimmer");

  iDesc.add<bool>("addGenPartonMatch", false)->setComment("add MC matching");
  iDesc.add<bool>("embedGenPartonMatch", false)->setComment("embed MC matched MC information");
  iDesc.add<edm::InputTag>("genPartonMatch", edm::InputTag())->setComment("input with MC match information");

  iDesc.add<bool>("addJetCharge", false);
  iDesc.add<edm::InputTag>("jetChargeSource", edm::InputTag());

  iDesc.add<bool>("addTagInfos", false);
  std::vector<edm::InputTag> emptyVInputTags;
  iDesc.add<std::vector<edm::InputTag>>("tagInfoSources", emptyVInputTags);

  iDesc.add<bool>("addBTagInfo", false);
  iDesc.add<bool>("addDiscriminators", false);
  iDesc.add<std::vector<edm::InputTag>>("discriminatorSources", emptyVInputTags);

  iDesc.add<bool>("getJetMCFlavour", true);
  iDesc.add<bool>("useLegacyJetMCFlavour", false);
  iDesc.add<bool>("addJetFlavourInfo", true);
  iDesc.add<edm::InputTag>("JetPartonMapSource", edm::InputTag());
  iDesc.add<edm::InputTag>("JetFlavourInfoSource", edm::InputTag());

  pat::helper::KinResolutionsLoader::fillDescription(iDesc);

  edm::ParameterSetDescription efficienciesPSet;
  efficienciesPSet.setAllowAnything();  // as PATJetProducer
  iDesc.add("efficiencies", efficienciesPSet);
  iDesc.add<bool>("addEfficiencies", false);

  edm::ParameterSetDescription userDataPSet;
  PATUserDataHelper<pat::GenJet>::fillDescription(userDataPSet);
  iDesc.addOptional("userData", userDataPSet);

  std::vector<std::string> empty;
  iDesc.add<std::vector<std::string>>("placeholderUserFloats", empty)
      ->setComment("user floats written with 0 unless already present");
  iDesc.add<std::vector<std::string>>("placeholderUserInts", empty)
      ->setComment("user ints written with 0 unless already present");
  iDesc.add<std::vector<std::string>>("placeholderPtEtaPhiUserFloats", empty)
      ->setComment(
          "0 or 3 labels (e.g. muonSubtrRawPt, muonSubtrRawEta, muonSubtrRawPhi) written with the jet's "
          "own pt, eta, phi unless already present");

  iDesc.add<bool>("truthTaggers", true)
      ->setComment("truth values under every bDiscriminator label the NanoAOD jet tables read");
  iDesc.ifValue(edm::ParameterDescription<std::string>("truthJetType", "AK4", true),
                edm::allowedValues<std::string>("AK4", "AK8"))
      ->setComment("\"AK4\": AK4 flavour/tau/lepton classes (cone 0.4, also for subjets); \"AK8\": resonance classes (cone 0.8)");
  iDesc.add<edm::InputTag>("genParticles", edm::InputTag())
      ->setComment("GenParticleCollection (prunedGenParticles) for taus, leptons and resonances; empty = not used");

  descriptions.add("patGenJetProducer", iDesc);
}

#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(PATGenJetProducer);
