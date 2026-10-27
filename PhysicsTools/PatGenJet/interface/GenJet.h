// -*- C++ -*-
//
// Package:     PhysicsTools/PatGenJet
// Class:       pat::GenJet (pat::io_v1::GenJet)
//
// Analysis-level generator jet. Mirrors the public API of pat::Jet
// (DataFormats/PatCandidates/interface/Jet.h, CMSSW_20_1_0_pre3) on top of reco::GenJet,
// so code and string expressions written for pat::Jet work on generator jets.
// Behaviour on missing data:
//   - where pat::Jet returns a value for missing data, the same value (e.g. bDiscriminator -1000)
//   - where pat::Jet throws: JEC factor 1 at every level, energies 0, groomedMass -1,
//     other numbers 0, lists empty, setters no-op (generic PAT lookups keep throwing)
//   - PF-named energies/fractions/multiplicities that reco::GenJet::Specific fills are real
//   - genJet() returns the jet itself; genJetFwdRef() is a self link set by the module that writes
//     the collection
//   - no scouting constructor
// Comments "copy" / "neutral" / "mapped" say where a body comes from.

#ifndef PhysicsTools_PatGenJet_GenJet_h
#define PhysicsTools_PatGenJet_GenJet_h

// pat::Jet header: reused for the shared typedefs (pat::CaloSpecific, pat::PFSpecific,
// pat::TagInfoFwdPtrCollection, ...) and the b-tag tag-info types named by the tagInfo* getters.
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/PatCandidates/interface/PATObject.h"
#include "DataFormats/PatCandidates/interface/JetCorrFactors.h"
#include "DataFormats/JetReco/interface/GenJet.h"
#include "DataFormats/JetReco/interface/GenJetCollection.h"
#include "DataFormats/JetMatching/interface/JetFlavourInfo.h"
#include "DataFormats/JetReco/interface/JetID.h"
#include "DataFormats/Common/interface/Ptr.h"
#include "DataFormats/Common/interface/Ref.h"
#include "DataFormats/Common/interface/RefVector.h"
#include "DataFormats/Common/interface/FwdRef.h"
#include "DataFormats/Common/interface/AtomicPtrCache.h"

#include <algorithm>
#include <typeinfo>
#include <numeric>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace pat {
  namespace io_v1 {
    class GenJet;
  }
  using GenJet = io_v1::GenJet;
  typedef std::vector<GenJet> GenJetCollection;
  typedef edm::Ref<GenJetCollection> GenJetRef;
  typedef edm::RefVector<GenJetCollection> GenJetRefVector;
  typedef edm::FwdRef<GenJetCollection> GenJetFwdRef;
  /// counterpart of pat::JetPtrCollection (Jet.h:83) for generator subjets
  typedef std::vector<edm::Ptr<pat::GenJet> > GenJetPtrCollection;
}  // namespace pat

namespace reco {
  /// pipe operator, counterpart of reco::operator<<(std::ostream&, const pat::Jet&) (Jet.h:67)
  std::ostream& operator<<(std::ostream& out, const pat::GenJet& obj);
}  // namespace reco

namespace pat {

  namespace io_v1 {
    class GenJet : public PATObject<reco::GenJet> {

    public:
      // ---------------------------------------------------------------- constructors
      GenJet();
      GenJet(const reco::GenJet& aJet);  // implicit, as pat::Jet (review S5)
      GenJet(const edm::RefToBase<reco::GenJet>& aJetRef);  // implicit, as pat::Jet (review S5)
      GenJet(const edm::Ptr<reco::GenJet>& aJetRef);  // implicit, as pat::Jet (review S5)
      GenJet(const edm::RefToBase<pat::GenJet>& aJetRef);  // implicit, as pat::Jet (review S5)
      GenJet(const edm::Ptr<pat::GenJet>& aJetRef);  // implicit, as pat::Jet (review S5)
      // no Run3ScoutingPFJet constructor
      ~GenJet() override;
      GenJet* clone() const override { return new GenJet(*this); }

      // ---------------------------------------------------------------- MC matching
      const reco::GenParticle* genParton() const { return genParticle(); }
      /// the jet itself
      const reco::GenJet* genJet() const { return this; }
      int partonFlavour() const { return jetFlavourInfo_.getPartonFlavour(); }
      int hadronFlavour() const { return jetFlavourInfo_.getHadronFlavour(); }
      const reco::JetFlavourInfo& jetFlavourInfo() const { return jetFlavourInfo_; }

      // ---------------------------------------------------------------- JEC
      // A generator jet is its own uncorrected level: factor 1 at every level, for every set.
      const std::vector<std::string> availableJECSets() const { return {}; }
      /// pat::Jet throws std::out_of_range for set>=0 (Jet.cc:344); here: empty list
      const std::vector<std::string> availableJECLevels(const int& /*set*/ = 0) const { return {}; }
      const std::vector<std::string> availableJECLevels(const std::string& /*set*/) const { return {}; }
      bool jecSetsAvailable() const { return false; }
      bool jecSetAvailable(const std::string& /*set*/) const { return false; }
      bool jecSetAvailable(const unsigned int& /*set*/) const { return false; }
      /// same missing-data value as pat::Jet (Jet.h:149, Jet.h:153)
      std::string currentJECSet() const { return std::string("ERROR"); }
      std::string currentJECLevel() const { return std::string("ERROR"); }
      /// pat::Jet returns an uninitialised member (Jet.h:641); here a defined NONE
      JetCorrFactors::Flavor currentJECFlavor() const { return JetCorrFactors::NONE; }
      float jecFactor(const std::string& /*level*/,
                      const std::string& /*flavor*/ = "none",
                      const std::string& /*set*/ = "") const {
        return 1.f;
      }
      float jecFactor(const unsigned int& /*level*/,
                      const JetCorrFactors::Flavor& /*flavor*/ = JetCorrFactors::NONE,
                      const unsigned int& /*set*/ = 0) const {
        return 1.f;
      }
      /// returns a pat::GenJet copy (signature adapted from pat::Jet, )
      GenJet correctedJet(const std::string& level, const std::string& flavor = "none", const std::string& set = "") const;
      GenJet correctedJet(const unsigned int& level,
                          const JetCorrFactors::Flavor& flavor = JetCorrFactors::NONE,
                          const unsigned int& set = 0) const;
      const LorentzVector correctedP4(const std::string& /*level*/,
                                      const std::string& /*flavor*/ = "none",
                                      const std::string& /*set*/ = "") const {
        return p4();
      }
      const LorentzVector correctedP4(const unsigned int& /*level*/,
                                      const JetCorrFactors::Flavor& /*flavor*/ = JetCorrFactors::NONE,
                                      const unsigned int& /*set*/ = 0) const {
        return p4();
      }
      /// no JEC: only rescales the four-momentum, as pat::Jet does without JEC (Jet.cc:313)
      void scaleEnergy(double fScale) override { scaleEnergy(fScale, "Unscaled"); }
      void scaleEnergy(double fScale, const std::string& /*level*/) { setP4(p4() * fScale); }

      // ---------------------------------------------------------------- b tagging (copy of pat::Jet)
      float bDiscriminator(const std::string& theLabel) const;  // -1000 if absent (Jet.cc:413)
      const std::vector<std::pair<std::string, float> >& getPairDiscri() const { return pairDiscriVector_; }
      std::vector<std::string> const& tagInfoLabels() const { return tagInfoLabels_; }
      bool hasTagInfo(const std::string label) const { return tagInfo(label) != nullptr; }
      const reco::BaseTagInfo* tagInfo(const std::string& label) const;
      const reco::CandIPTagInfo* tagInfoCandIP(const std::string& label = "") const;
      const reco::TrackIPTagInfo* tagInfoTrackIP(const std::string& label = "") const;
      const reco::CandSoftLeptonTagInfo* tagInfoCandSoftLepton(const std::string& label = "") const;
      const reco::SoftLeptonTagInfo* tagInfoSoftLepton(const std::string& label = "") const;
      const reco::CandSecondaryVertexTagInfo* tagInfoCandSecondaryVertex(const std::string& label = "") const;
      const reco::SecondaryVertexTagInfo* tagInfoSecondaryVertex(const std::string& label = "") const;
      const reco::BoostedDoubleSVTagInfo* tagInfoBoostedDoubleSV(const std::string& label = "") const;
      const reco::PixelClusterTagInfo* tagInfoPixelCluster(const std::string& label = "") const;
      void addBDiscriminatorPair(const std::pair<std::string, float>& thePair) { pairDiscriVector_.push_back(thePair); }
      void addTagInfo(const std::string& label, const TagInfoFwdPtrCollection::value_type& info);

      // ---------------------------------------------------------------- tracks (copy; never filled)
      float jetCharge() const { return jetCharge_; }
      const reco::TrackRefVector& associatedTracks() const { return associatedTracks_; }
      void setJetCharge(float jetCharge) { jetCharge_ = jetCharge; }
      void setAssociatedTracks(const reco::TrackRefVector& tracks) { associatedTracks_ = tracks; }

      // ---------------------------------------------------------------- embedding setters (copy)
      void setCaloTowers(const CaloTowerFwdPtrCollection& caloTowers);
      void setPFCandidates(const PFCandidateFwdPtrCollection& pfCandidates);
      void setGenParton(const reco::GenParticleRef& gp, bool embed = false) { setGenParticleRef(gp, embed); }
      /// type adapted: the link is to a pat::GenJet collection
      void setGenJetRef(const GenJetFwdRef& gj) { genJetFwdRef_ = gj; }
      void setPartonFlavour(int partonFl) { jetFlavourInfo_.setPartonFlavour(partonFl); }
      void setHadronFlavour(int hadronFl) { jetFlavourInfo_.setHadronFlavour(hadronFl); }
      void setJetFlavourInfo(const reco::JetFlavourInfo& jetFlavourInfo) { jetFlavourInfo_ = jetFlavourInfo; }
      void setJetID(reco::JetID const& id) { jetID_ = id; }

      // ---------------------------------------------------------------- jet type
      bool isCaloJet() const { return false; }
      bool isJPTJet() const { return false; }
      bool isPFJet() const { return false; }
      bool isBasicJet() const { return true; }
      bool hasPFSpecific() const { return false; }
      /// pat::Jet throws "Type Mismatch" (Jet.h:292/298/306); here a static all-zero struct
      const CaloSpecific& caloSpecific() const;
      const JPTSpecific& jptSpecific() const;
      const PFSpecific& pfSpecific() const;
      /// pat::Jet throws (Jet.h:312/318/324); here no-op
      void setCaloSpecific(const CaloSpecific&) {}
      void setJPTSpecific(const JPTSpecific&) {}
      void setPFSpecific(const PFSpecific&) {}

      // ---------------------------------------------------------------- calo-like
      float maxEInEmTowers() const { return 0.f; }  // energies 0
      float maxEInHadTowers() const { return 0.f; }
      /// mapped: calo-like split of reco::GenJet::Specific (GenJet.h:86, GenJet.h:88)
      float energyFractionHadronic() const { return energy() > 0 ? hadEnergy() / energy() : 0.f; }
      float emEnergyFraction() const { return energy() > 0 ? emEnergy() / energy() : 0.f; }
      float hadEnergyInHB() const { return 0.f; }
      float hadEnergyInHO() const { return 0.f; }
      float hadEnergyInHE() const { return 0.f; }
      float hadEnergyInHF() const { return 0.f; }
      float emEnergyInEB() const { return 0.f; }
      float emEnergyInEE() const { return 0.f; }
      float emEnergyInHF() const { return 0.f; }
      float towersArea() const { return 0.f; }  //
      int n90() const { return nCarrying(0.9); }
      int n60() const { return nCarrying(0.6); }
      /// embedded tower if set and in range, else a null Ptr (pat::Jet throws for non-tower daughters)
      CaloTowerPtr getCaloConstituent(unsigned fIndex) const;
      std::vector<CaloTowerPtr> const& getCaloConstituents() const;  // embedded towers if set, else empty

      // ---------------------------------------------------------------- JPT
      const reco::TrackRefVector& pionsInVertexInCalo() const { return jptSpecific().pionsInVertexInCalo; }
      const reco::TrackRefVector& pionsInVertexOutCalo() const { return jptSpecific().pionsInVertexOutCalo; }
      const reco::TrackRefVector& pionsOutVertexInCalo() const { return jptSpecific().pionsOutVertexInCalo; }
      const reco::TrackRefVector& muonsInVertexInCalo() const { return jptSpecific().muonsInVertexInCalo; }
      const reco::TrackRefVector& muonsInVertexOutCalo() const { return jptSpecific().muonsInVertexOutCalo; }
      const reco::TrackRefVector& muonsOutVertexInCalo() const { return jptSpecific().muonsOutVertexInCalo; }
      const reco::TrackRefVector& elecsInVertexInCalo() const { return jptSpecific().elecsInVertexInCalo; }
      const reco::TrackRefVector& elecsInVertexOutCalo() const { return jptSpecific().elecsInVertexOutCalo; }
      const reco::TrackRefVector& elecsOutVertexInCalo() const { return jptSpecific().elecsOutVertexInCalo; }
      float elecMultiplicity() const { return 0.f; }

      // ---------------------------------------------------------------- PF-named
      // NOT redeclared (inherited from reco::GenJet with identical signature, GenJet.h:95-104):
      //   chargedHadronEnergy, neutralHadronEnergy, chargedEmEnergy, neutralEmEnergy, muonEnergy,
      //   chargedHadronMultiplicity, neutralHadronMultiplicity, muonMultiplicity
      /// mapped: m_ChargedHadronMultiplicity + m_ChargedEmMultiplicity + m_MuonMultiplicity
      int chargedMultiplicity() const {
        return chargedHadronMultiplicity() + chargedEmMultiplicity() + muonMultiplicity();
      }
      /// copy of pat::Jet bodies (Jet.h:409-423); jecSetsAvailable() is false, so the denominator is energy()
      float chargedHadronEnergyFraction() const { return chargedHadronEnergy() / energy(); }
      float neutralHadronEnergyFraction() const { return neutralHadronEnergy() / energy(); }
      float chargedEmEnergyFraction() const { return chargedEmEnergy() / energy(); }
      float neutralEmEnergyFraction() const { return neutralEmEnergy() / energy(); }
      /// mapped: photons = m_NeutralEmEnergy, electrons = m_ChargedEmEnergy
      float photonEnergy() const { return neutralEmEnergy(); }
      float photonEnergyFraction() const { return photonEnergy() / energy(); }
      float electronEnergy() const { return chargedEmEnergy(); }
      float electronEnergyFraction() const { return electronEnergy() / energy(); }
      float muonEnergyFraction() const { return muonEnergy() / energy(); }
      /// HF and HO energies 0, so their fractions are 0
      float HFHadronEnergy() const { return 0.f; }
      float HFHadronEnergyFraction() const { return 0.f; }
      float HFEMEnergy() const { return 0.f; }
      float HFEMEnergyFraction() const { return 0.f; }
      /// mapped: photon = neutral EM, electron = charged EM multiplicity (GenJet.h:102-103)
      int photonMultiplicity() const { return neutralEmMultiplicity(); }
      int electronMultiplicity() const { return chargedEmMultiplicity(); }
      int HFHadronMultiplicity() const { return 0; }  //
      int HFEMMultiplicity() const { return 0; }
      /// mapped: m_MuonEnergy
      float chargedMuEnergy() const { return muonEnergy(); }
      float chargedMuEnergyFraction() const { return chargedMuEnergy() / energy(); }
      /// mapped: m_NeutralHadronMultiplicity + m_NeutralEmMultiplicity
      int neutralMultiplicity() const { return neutralHadronMultiplicity() + neutralEmMultiplicity(); }
      float hoEnergy() const { return 0.f; }
      float hoEnergyFraction() const { return 0.f; }
      /// embedded candidate if set and in range, else a null Ptr (pat::Jet throws for non-PF daughters)
      reco::PFCandidatePtr getPFConstituent(unsigned fIndex) const;
      std::vector<reco::PFCandidatePtr> const& getPFConstituents() const;  // embedded if set, else empty

      // ---------------------------------------------------------------- constituents (copy, subjet-aware)
      const reco::Candidate* daughter(size_t i) const override;
      reco::CandidatePtr daughterPtr(size_t i) const override;
      const reco::CompositePtrCandidate::daughters& daughterPtrVector() const override;
      using reco::LeafCandidate::daughter;
      size_t numberOfDaughters() const override;
      void clearDaughters() override {
        PATObject<reco::GenJet>::clearDaughters();
        daughtersTemp_.reset();
      }

      // ---------------------------------------------------------------- jet ID, forward pointers
      reco::JetID const& jetID() const { return jetID_; }
      CaloTowerFwdPtrVector const& caloTowersFwdPtr() const { return caloTowersFwdPtr_; }
      reco::PFCandidateFwdPtrVector const& pfCandidatesFwdPtr() const { return pfCandidatesFwdPtr_; }
      /// forward and back ref both point to this jet's own entry in its collection.
      /// Return type adapted from edm::FwdRef<reco::GenJetCollection> (Jet.h:520).
      GenJetFwdRef const& genJetFwdRef() const { return genJetFwdRef_; }
      TagInfoFwdPtrCollection const& tagInfosFwdPtr() const { return tagInfosFwdPtr_; }
      /// as pat::Jet inside the range; out of range: no-op instead of OutOfRange
      void updateFwdCaloTowerFwdPtr(unsigned int index, const edm::Ptr<CaloTower>& updateFwd);
      void updateFwdPFCandidateFwdPtr(unsigned int index, const edm::Ptr<reco::PFCandidate>& updateFwd);
      void updateFwdTagInfoFwdPtr(unsigned int index, const edm::Ptr<reco::BaseTagInfo>& updateFwd);
      void updateFwdGenJetFwdRef(GenJetRef updateRef) {
        genJetFwdRef_ = GenJetFwdRef(updateRef, genJetFwdRef_.backRef());
      }

      friend std::ostream& reco::operator<<(std::ostream& out, const pat::GenJet& obj);

      // ---------------------------------------------------------------- subjets
      /// missing index or label: static empty collection, not OutOfRange/SubjetsNotFound
      pat::GenJetPtrCollection const& subjets(unsigned int index = 0) const;
      pat::GenJetPtrCollection const& subjets(std::string const& label) const;
      void addSubjets(pat::GenJetPtrCollection const& pieces, std::string const& label = "");
      bool hasSubjets(std::string const& label) const {
        return std::find(subjetLabels_.begin(), subjetLabels_.end(), label) != subjetLabels_.end();
      }
      unsigned int nSubjetCollections() const { return subjetCollections_.size(); }
      std::vector<std::string> const& subjetCollectionNames() const { return subjetLabels_; }
      /// guard evaluated first; -1 if the collection is missing or empty
      double groomedMass(unsigned int index = 0) const {
        return index < nSubjetCollections() ? sumMass(subjetCollections_[index]) : -1.0;
      }
      double groomedMass(std::string const& label) const {
        return hasSubjets(label) ? sumMass(subjets(label)) : -1.0;
      }

      // ---------------------------------------------------------------- inherited, listed for the reader
      // PATObject<reco::GenJet>: user data, trigger matches, efficiencies, overlaps, resolutions,
      //   genParticle refs (userFloat/userInt with a missing key, efficiency, getKinResolution and
      //   resol* keep throwing:  exception).
      // reco::GenJet: emEnergy, hadEnergy, invisibleEnergy, auxiliaryEnergy, chargedEmMultiplicity,
      //   neutralEmMultiplicity, detectorEta, getGenConstituent(s), getSpecific, setSpecific.
      //   Note: PATObject::genParticle(size_t) hides the static reco::GenJet::genParticle(const Candidate*);
      //   call it as reco::GenJet::genParticle(c).

    protected:
      // ---- embedding (kept so that the mirrored setters/getters behave as in pat::Jet)
      edm::AtomicPtrCache<std::vector<CaloTowerPtr> > caloTowersTemp_;              // transient
      CaloTowerFwdPtrVector caloTowersFwdPtr_;
      edm::AtomicPtrCache<std::vector<reco::PFCandidatePtr> > pfCandidatesTemp_;  // transient
      reco::PFCandidateFwdPtrVector pfCandidatesFwdPtr_;

      // ---- substructure
      std::vector<pat::GenJetPtrCollection> subjetCollections_;
      std::vector<std::string> subjetLabels_;
      edm::AtomicPtrCache<std::vector<reco::CandidatePtr> > daughtersTemp_;  // transient

      // ---- MC
      GenJetFwdRef genJetFwdRef_;  // self link, set by the module that puts the collection
      reco::JetFlavourInfo jetFlavourInfo_;

      // ---- b tagging
      std::vector<std::pair<std::string, float> > pairDiscriVector_;
      std::vector<std::string> tagInfoLabels_;
      TagInfoFwdPtrCollection tagInfosFwdPtr_;

      // ---- tracks
      float jetCharge_;
      reco::TrackRefVector associatedTracks_;

      // ---- id
      reco::JetID jetID_;

      // Not stored (pat::Jet members with no generator meaning): jec_, currentJECSet_, currentJECLevel_,
      // currentJECFlavor_, specificCalo_, specificJPT_, specificPF_, genJet_, genJetRef_, tagInfos_,
      // caloTowers_, pfCandidates_, embeddedCaloTowers_, embeddedPFCandidates_.

    private:
      /// copy of Jet.h:670 without the legacy OwnVector branch
      template <typename T>
      const T* tagInfoByType() const {
        for (size_t i = 0, n = tagInfosFwdPtr_.size(); i < n; ++i) {
          reco::BaseTagInfo const* baseTagInfo = tagInfosFwdPtr_[i].get();
          if (baseTagInfo != nullptr && typeid(*baseTagInfo) == typeid(T)) {
            return static_cast<const T*>(baseTagInfo);
          }
        }
        return nullptr;
      }
      template <typename T>
      const T* tagInfoByTypeOrLabel(const std::string& label = "") const {
        return (label.empty() ? tagInfoByType<T>() : dynamic_cast<const T*>(tagInfo(label)));
      }
      static double sumMass(pat::GenJetPtrCollection const& sub) {
        return sub.empty() ? -1.0
                           : std::accumulate(sub.begin(),
                                             sub.end(),
                                             reco::Candidate::LorentzVector(),
                                             [](reco::Candidate::LorentzVector const& a,
                                                edm::Ptr<pat::GenJet> const& b) { return a + b->p4(); })
                                 .mass();
      }
      void cacheCaloTowers() const;     // embedded FwdPtrs only; no throw
      void cachePFCandidates() const;   // embedded FwdPtrs only; no throw
      void cacheDaughters() const;      // copy of Jet.cc:616
      static pat::GenJetPtrCollection const& emptySubjets();
    };
  }  // namespace io_v1
}  // namespace pat

#endif
