//
// Package:     PhysicsTools/PatGenJet
// Class:       pat::GenJet (pat::io_v1::GenJet)
//
// Out-of-line members. Bodies marked "copy" follow DataFormats/PatCandidates/src/Jet.cc
// (CMSSW_20_1_0_pre3); "neutral" bodies return 0, empty lists or no-ops where pat::Jet would throw.
//
#include "PhysicsTools/PatGenJet/interface/GenJet.h"

#include <iomanip>

using namespace pat;

// ------------------------------------------------------------------ constructors

/// default constructor
GenJet::GenJet() : PATObject<reco::GenJet>(reco::GenJet()), jetCharge_(0.) {}

/// constructor from a reco::GenJet: keeps p4, vertex, area, constituents and reco::GenJet::Specific
GenJet::GenJet(const reco::GenJet& aJet) : PATObject<reco::GenJet>(aJet), jetCharge_(0.) {}

/// constructor from ref to reco::GenJet
GenJet::GenJet(const edm::RefToBase<reco::GenJet>& aJetRef) : PATObject<reco::GenJet>(aJetRef), jetCharge_(0.) {}

/// constructor from ptr to reco::GenJet
GenJet::GenJet(const edm::Ptr<reco::GenJet>& aJetRef) : PATObject<reco::GenJet>(aJetRef), jetCharge_(0.) {}

/// constructor from ref to pat::GenJet (copy of Jet.cc, pat::Jet(RefToBase<pat::Jet>))
GenJet::GenJet(const edm::RefToBase<pat::GenJet>& aJetRef) : GenJet(*aJetRef) {
  refToOrig_ = edm::Ptr<reco::Candidate>(aJetRef.id(), aJetRef.get(), aJetRef.key());
}

/// constructor from ptr to pat::GenJet (copy of Jet.cc, pat::Jet(Ptr<pat::Jet>))
GenJet::GenJet(const edm::Ptr<pat::GenJet>& aJetRef) : GenJet(*aJetRef) { refToOrig_ = aJetRef; }

GenJet::~GenJet() {}

std::ostream& reco::operator<<(std::ostream& out, const pat::GenJet& obj) {
  if (!out)
    return out;

  out << "\tpat::GenJet: ";
  out << std::setiosflags(std::ios::right);
  out << std::setiosflags(std::ios::fixed);
  out << std::setprecision(3);
  out << " E/pT/eta/phi " << obj.energy() << "/" << obj.pt() << "/" << obj.eta() << "/" << obj.phi();
  return out;
}

// ------------------------------------------------------------------ JEC: a copy of the jet

GenJet GenJet::correctedJet(const std::string& /*level*/,
                            const std::string& /*flavor*/,
                            const std::string& /*set*/) const {
  return *this;
}

GenJet GenJet::correctedJet(const unsigned int& /*level*/,
                            const JetCorrFactors::Flavor& /*flavor*/,
                            const unsigned int& /*set*/) const {
  return *this;
}

// ------------------------------------------------------------------ b tagging (copy)

/// get b discriminant from label name; -1000 if absent (Jet.cc:413)
float GenJet::bDiscriminator(const std::string& aLabel) const {
  float discriminator = -1000.;
  for (int i = (int(pairDiscriVector_.size()) - 1); i >= 0; i--) {
    if (pairDiscriVector_[i].first == aLabel) {
      discriminator = pairDiscriVector_[i].second;
      break;
    }
  }
  return discriminator;
}

/// copy of Jet.cc without the legacy OwnVector fallback
const reco::BaseTagInfo* GenJet::tagInfo(const std::string& label) const {
  for (int i = (int(tagInfoLabels_.size()) - 1); i >= 0; i--) {
    if (tagInfoLabels_[i] == label) {
      if (static_cast<size_t>(i) < tagInfosFwdPtr_.size())
        return tagInfosFwdPtr_[i].get();
      return nullptr;
    }
  }
  return nullptr;
}

const reco::CandIPTagInfo* GenJet::tagInfoCandIP(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::CandIPTagInfo>(label);
}

const reco::TrackIPTagInfo* GenJet::tagInfoTrackIP(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::TrackIPTagInfo>(label);
}

const reco::CandSoftLeptonTagInfo* GenJet::tagInfoCandSoftLepton(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::CandSoftLeptonTagInfo>(label);
}

const reco::SoftLeptonTagInfo* GenJet::tagInfoSoftLepton(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::SoftLeptonTagInfo>(label);
}

const reco::CandSecondaryVertexTagInfo* GenJet::tagInfoCandSecondaryVertex(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::CandSecondaryVertexTagInfo>(label);
}

const reco::SecondaryVertexTagInfo* GenJet::tagInfoSecondaryVertex(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::SecondaryVertexTagInfo>(label);
}

const reco::BoostedDoubleSVTagInfo* GenJet::tagInfoBoostedDoubleSV(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::BoostedDoubleSVTagInfo>(label);
}

const reco::PixelClusterTagInfo* GenJet::tagInfoPixelCluster(const std::string& label) const {
  return tagInfoByTypeOrLabel<reco::PixelClusterTagInfo>(label);
}

void GenJet::addTagInfo(const std::string& label, const TagInfoFwdPtrCollection::value_type& info) {
  std::string::size_type idx = label.find("TagInfos");
  if (idx == std::string::npos) {
    tagInfoLabels_.push_back(label);
  } else {
    tagInfoLabels_.push_back(label.substr(0, idx));
  }
  tagInfosFwdPtr_.push_back(info);
}

// ------------------------------------------------------------------ embedded constituents (copy, no throw)

void GenJet::setCaloTowers(const CaloTowerFwdPtrCollection& caloTowers) {
  caloTowersFwdPtr_.reserve(caloTowersFwdPtr_.size() + caloTowers.size());
  for (auto const& tower : caloTowers) {
    caloTowersFwdPtr_.push_back(tower);
  }
  caloTowersTemp_.reset();
}

void GenJet::setPFCandidates(const PFCandidateFwdPtrCollection& pfCandidates) {
  pfCandidatesFwdPtr_.reserve(pfCandidatesFwdPtr_.size() + pfCandidates.size());
  for (auto const& cand : pfCandidates) {
    pfCandidatesFwdPtr_.push_back(cand);
  }
  pfCandidatesTemp_.reset();
}

/// embedded tower if one was set, otherwise a null Ptr (pat::Jet throws "Invalid Constituent")
CaloTowerPtr GenJet::getCaloConstituent(unsigned fIndex) const {
  return (fIndex < caloTowersFwdPtr_.size() ? caloTowersFwdPtr_[fIndex].ptr() : CaloTowerPtr());
}

std::vector<CaloTowerPtr> const& GenJet::getCaloConstituents() const {
  if (!caloTowersTemp_.isSet())
    cacheCaloTowers();
  return *caloTowersTemp_;
}

/// embedded candidate if one was set, otherwise a null Ptr (pat::Jet throws "Invalid Constituent")
reco::PFCandidatePtr GenJet::getPFConstituent(unsigned fIndex) const {
  return (fIndex < pfCandidatesFwdPtr_.size() ? pfCandidatesFwdPtr_[fIndex].ptr() : reco::PFCandidatePtr());
}

std::vector<reco::PFCandidatePtr> const& GenJet::getPFConstituents() const {
  if (!pfCandidatesTemp_.isSet())
    cachePFCandidates();
  return *pfCandidatesTemp_;
}

void GenJet::cacheCaloTowers() const {
  auto caloTowersTemp = std::make_unique<std::vector<CaloTowerPtr>>();
  caloTowersTemp->reserve(caloTowersFwdPtr_.size());
  for (auto const& tower : caloTowersFwdPtr_)
    caloTowersTemp->emplace_back(tower.ptr());
  caloTowersTemp_.set(std::move(caloTowersTemp));
}

void GenJet::cachePFCandidates() const {
  auto pfCandidatesTemp = std::make_unique<std::vector<reco::PFCandidatePtr>>();
  pfCandidatesTemp->reserve(pfCandidatesFwdPtr_.size());
  for (auto const& cand : pfCandidatesFwdPtr_)
    pfCandidatesTemp->emplace_back(cand.ptr());
  pfCandidatesTemp_.set(std::move(pfCandidatesTemp));
}

// ------------------------------------------------------------------ static all-zero specifics

const CaloSpecific& GenJet::caloSpecific() const {
  static const CaloSpecific empty;
  return empty;
}

const JPTSpecific& GenJet::jptSpecific() const {
  static const JPTSpecific empty;
  return empty;
}

const PFSpecific& GenJet::pfSpecific() const {
  static const PFSpecific empty;
  return empty;
}

// ------------------------------------------------------------------ forward-pointer updates

void GenJet::updateFwdCaloTowerFwdPtr(unsigned int index, const edm::Ptr<CaloTower>& updateFwd) {
  if (index < caloTowersFwdPtr_.size()) {
    caloTowersFwdPtr_[index] = CaloTowerFwdPtrCollection::value_type(updateFwd, caloTowersFwdPtr_[index].backPtr());
    caloTowersTemp_.reset();
  }
}

void GenJet::updateFwdPFCandidateFwdPtr(unsigned int index, const edm::Ptr<reco::PFCandidate>& updateFwd) {
  if (index < pfCandidatesFwdPtr_.size()) {
    pfCandidatesFwdPtr_[index] =
        PFCandidateFwdPtrCollection::value_type(updateFwd, pfCandidatesFwdPtr_[index].backPtr());
    pfCandidatesTemp_.reset();
  }
}

void GenJet::updateFwdTagInfoFwdPtr(unsigned int index, const edm::Ptr<reco::BaseTagInfo>& updateFwd) {
  if (index < tagInfosFwdPtr_.size()) {
    tagInfosFwdPtr_[index] = TagInfoFwdPtrCollection::value_type(updateFwd, tagInfosFwdPtr_[index].backPtr());
  }
}

// ------------------------------------------------------------------ daughters (copy, subjet-aware)

const reco::Candidate* GenJet::daughter(size_t i) const {
  if (!subjetCollections_.empty()) {
    if (!daughtersTemp_.isSet())
      cacheDaughters();
    return daughtersTemp_->at(i).get();
  }
  return reco::GenJet::daughter(i);
}

reco::CandidatePtr GenJet::daughterPtr(size_t i) const {
  if (!subjetCollections_.empty()) {
    if (!daughtersTemp_.isSet())
      cacheDaughters();
    return daughtersTemp_->at(i);
  }
  return reco::GenJet::daughterPtr(i);
}

const reco::CompositePtrCandidate::daughters& GenJet::daughterPtrVector() const {
  if (!subjetCollections_.empty()) {
    if (!daughtersTemp_.isSet())
      cacheDaughters();
    return *daughtersTemp_;
  }
  return reco::GenJet::daughterPtrVector();
}

size_t GenJet::numberOfDaughters() const {
  if (!subjetCollections_.empty()) {
    if (!daughtersTemp_.isSet())
      cacheDaughters();
    return daughtersTemp_->size();
  }
  return reco::GenJet::numberOfDaughters();
}

/// copy of Jet.cc:616: subjet daughters are replaced by their own constituents
void GenJet::cacheDaughters() const {
  auto daughtersTemp = std::make_unique<std::vector<reco::CandidatePtr>>();
  const std::vector<reco::CandidatePtr>& jdaus = reco::GenJet::daughterPtrVector();
  for (const reco::CandidatePtr& dau : jdaus) {
    if (dau->isJet()) {
      const reco::Jet* subjet = dynamic_cast<const reco::Jet*>(&*dau);
      if (subjet) {
        const std::vector<reco::CandidatePtr>& sjdaus = subjet->daughterPtrVector();
        daughtersTemp->insert(daughtersTemp->end(), sjdaus.begin(), sjdaus.end());
      }
    } else
      daughtersTemp->push_back(dau);
  }
  daughtersTemp_.set(std::move(daughtersTemp));
}

// ------------------------------------------------------------------ subjets

pat::GenJetPtrCollection const& GenJet::emptySubjets() {
  static const pat::GenJetPtrCollection empty;
  return empty;
}

pat::GenJetPtrCollection const& GenJet::subjets(unsigned int index) const {
  if (index < subjetCollections_.size())
    return subjetCollections_[index];
  return emptySubjets();
}

pat::GenJetPtrCollection const& GenJet::subjets(std::string const& label) const {
  auto found = std::find(subjetLabels_.begin(), subjetLabels_.end(), label);
  if (found != subjetLabels_.end())
    return subjetCollections_[std::distance(subjetLabels_.begin(), found)];
  return emptySubjets();
}

void GenJet::addSubjets(pat::GenJetPtrCollection const& pieces, std::string const& label) {
  subjetCollections_.push_back(pieces);
  subjetLabels_.push_back(label);
}
