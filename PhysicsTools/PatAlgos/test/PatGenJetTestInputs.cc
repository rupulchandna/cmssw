// Test-only EDProducer: hand-made, mutually consistent inputs for the TestProcessor tests
//. Everything is made in one module so that the Ptrs/Refs
// between the products are valid in the same event (getRefBeforePut / OrphanHandle).
//
// Products (instance):
//   ""        std::vector<reco::GenParticle>                      10 particles, pt = kGenPt
//   "packed"  std::vector<pat::PackedGenParticle>                  9 particles
//   ""        edm::Association<std::vector<pat::PackedGenParticle>>  gen k -> packed kGenToPacked[k]
//                                                                  (gen 8 and gen 9 both -> packed 0)
//   "jets"    std::vector<reco::GenJet>   4 jets, pt 30, 80, 50, 12 (NOT pt-sorted unless sortJets)
//   "flavour" reco::JetFlavourInfoMatchingCollection keyed to "jets"
//   "tau1"    edm::ValueMap<float> keyed to "jets": 0.1 * (jet number + 1)
//   "nconst"  edm::ValueMap<int>   keyed to "jets": number of daughters
//   "subjets" std::vector<reco::GenJet>   6 subjets S0..S5, pt 50, 20, 30, 25, 10, 5
//   "groomed" std::vector<reco::BasicJet> 3 groomed jets; daughters G0 = {S2, S0}, G1 = {S1, S3}, G2 = {S4, S5}
//   "ecfN2"   edm::ValueMap<float> keyed to "groomed": 0.11, 0.22, 0.33
// Option unmappedConstituent (untracked bool, default False): a 11th gen particle with a NULL
// association entry (no packed partner) is added as last daughter of the jet "30" (daughters {3, 2, 10}).
// Geometry: jet "80" at (eta, phi) = (0, 1) with G0 at dR ~0.1; jet "50" at (-1, 2) with G1 at dR ~0.5
// and G2 at dR ~0.05 (first-within-distMax must pick G1); jets "30" and "12" have no groomed jet nearby.

#include "DataFormats/Common/interface/Association.h"
#include "DataFormats/Common/interface/OrphanHandle.h"
#include "DataFormats/Common/interface/RefToBaseProd.h"
#include "DataFormats/Common/interface/ValueMap.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/JetMatching/interface/JetFlavourInfoMatching.h"
#include "DataFormats/JetReco/interface/BasicJet.h"
#include "DataFormats/JetReco/interface/GenJet.h"
#include "DataFormats/PatCandidates/interface/PackedGenParticle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <vector>

namespace {
  using LV = reco::Particle::LorentzVector;
  LV polar(double pt, double eta, double phi, double m = 0.) {
    reco::Particle::PolarLorentzVector p(pt, eta, phi, m);
    return LV(p.px(), p.py(), p.pz(), p.energy());
  }
  constexpr double kGenPt[10] = {5., 40., 12., 25., 8., 30., 3., 18., 22., 6.};
  constexpr unsigned kGenToPacked[10] = {8, 7, 6, 5, 4, 3, 2, 1, 0, 0};
  struct JetDef {
    double pt, eta, phi;
    std::vector<unsigned> gens;  // in pt order, as AOD gen jets
    int hadronFlavour, partonFlavour;
  };
  const std::vector<JetDef> kJets = {{30., 1.0, -2.0, {3, 2}, 0, 21},
                                     {80., 0.0, 1.0, {1, 5, 8, 9}, 5, 5},
                                     {50., -1.0, 2.0, {7, 0}, 4, -4},
                                     {12., 2.0, -1.0, {4, 6}, 0, 1}};
  struct SubDef {
    double pt, eta, phi;
    unsigned gen;
  };
  const std::vector<SubDef> kSubjets = {{50., 0.0, 1.05, 1},
                                        {20., -1.0, 2.45, 7},
                                        {30., 0.0, 1.15, 5},
                                        {25., -1.0, 2.55, 0},
                                        {10., -1.0, 2.0, 4},
                                        {5., -1.0, 2.1, 6}};
  const std::vector<std::vector<unsigned>> kGroomed = {{2, 0}, {1, 3}, {4, 5}};
}  // namespace

class PatGenJetTestInputs : public edm::global::EDProducer<> {
public:
  explicit PatGenJetTestInputs(edm::ParameterSet const& p)
      : sortJets_(p.getUntrackedParameter<bool>("sortJets", false)),
        unmapped_(p.getUntrackedParameter<bool>("unmappedConstituent", false)) {
    produces<std::vector<reco::GenParticle>>();
    produces<std::vector<pat::PackedGenParticle>>("packed");
    produces<edm::Association<std::vector<pat::PackedGenParticle>>>();
    produces<std::vector<reco::GenJet>>("jets");
    produces<reco::JetFlavourInfoMatchingCollection>("flavour");
    produces<edm::ValueMap<float>>("tau1");
    produces<edm::ValueMap<int>>("nconst");
    produces<std::vector<reco::GenJet>>("subjets");
    produces<std::vector<reco::BasicJet>>("groomed");
    produces<edm::ValueMap<float>>("ecfN2");
  }

  void produce(edm::StreamID, edm::Event& iEvent, edm::EventSetup const&) const override {
    // ---- gen particles, packed, association
    auto gens = std::make_unique<std::vector<reco::GenParticle>>();
    const unsigned nGen = unmapped_ ? 11 : 10;  // gen 10 (unmapped option) has no packed partner
    for (unsigned k = 0; k < nGen; ++k)
      gens->emplace_back(
          0, polar(k < 10 ? kGenPt[k] : 7., 0.1 * k - 0.5, 0.3 * k - 1.5), reco::Particle::Point(0, 0, 0), 211, 1, true);
    auto packed = std::make_unique<std::vector<pat::PackedGenParticle>>(9);
    for (unsigned k = 0; k < 9; ++k)  // gen 9 shares packed 0 with gen 8
      (*packed)[kGenToPacked[k]] = pat::PackedGenParticle((*gens)[k], reco::GenParticleRef());
    auto ohGen = iEvent.put(std::move(gens));
    auto ohPacked = iEvent.put(std::move(packed), "packed");
    auto assoc = std::make_unique<edm::Association<std::vector<pat::PackedGenParticle>>>(ohPacked);
    {
      edm::Association<std::vector<pat::PackedGenParticle>>::Filler f(*assoc);
      std::vector<int> idx(kGenToPacked, kGenToPacked + 10);
      if (unmapped_)
        idx.push_back(-1);  // null reference: no packed partner
      f.insert(ohGen, idx.begin(), idx.end());
      f.fill();
    }
    iEvent.put(std::move(assoc));

    // ---- jets
    std::vector<unsigned> order(kJets.size());
    std::iota(order.begin(), order.end(), 0);
    if (sortJets_)
      std::sort(order.begin(), order.end(), [](unsigned a, unsigned b) { return kJets[a].pt > kJets[b].pt; });
    auto jets = std::make_unique<std::vector<reco::GenJet>>();
    for (unsigned j : order) {
      auto const& d = kJets[j];
      reco::GenJet::Specific s;
      s.m_EmEnergy = d.pt / 4;
      s.m_HadEnergy = d.pt / 2;
      s.m_ChargedHadronMultiplicity = d.gens.size();
      reco::GenJet gj(polar(d.pt, d.eta, d.phi, 5.), reco::Particle::Point(0, 0, 0), s);
      gj.setJetArea(0.5);
      for (unsigned g : d.gens)
        gj.addDaughter(reco::CandidatePtr(ohGen, g));
      if (unmapped_ && j == 0)
        gj.addDaughter(reco::CandidatePtr(ohGen, 10));
      jets->push_back(gj);
    }
    auto ohJets = iEvent.put(std::move(jets), "jets");

    auto flavour = std::make_unique<reco::JetFlavourInfoMatchingCollection>(edm::RefToBaseProd<reco::Jet>(ohJets));
    std::vector<float> tau1;
    std::vector<int> nconst;
    for (unsigned i = 0; i < order.size(); ++i) {
      auto const& d = kJets[order[i]];
      reco::GenParticleRefVector bHadrons;
      if (d.hadronFlavour == 5)
        bHadrons.push_back(reco::GenParticleRef(ohGen, 1));
      flavour->setValue(i,
                        reco::JetFlavourInfo(bHadrons,
                                             reco::GenParticleRefVector(),
                                             reco::GenParticleRefVector(),
                                             reco::GenParticleRefVector(),
                                             d.hadronFlavour,
                                             d.partonFlavour));
      tau1.push_back(0.1f * (order[i] + 1));
      nconst.push_back(d.gens.size());
    }
    iEvent.put(std::move(flavour), "flavour");
    putValueMap(iEvent, ohJets, tau1, "tau1");
    putValueMap(iEvent, ohJets, nconst, "nconst");

    // ---- subjets and groomed jets
    auto subjets = std::make_unique<std::vector<reco::GenJet>>();
    for (auto const& s : kSubjets) {
      reco::GenJet sj(polar(s.pt, s.eta, s.phi, 1.), reco::Particle::Point(0, 0, 0), reco::GenJet::Specific());
      sj.addDaughter(reco::CandidatePtr(ohGen, s.gen));
      subjets->push_back(sj);
    }
    auto ohSub = iEvent.put(std::move(subjets), "subjets");
    auto groomed = std::make_unique<std::vector<reco::BasicJet>>();
    for (auto const& g : kGroomed) {
      LV sum;
      reco::Jet::Constituents dau;
      for (unsigned s : g) {
        sum += (*ohSub)[s].p4();
        dau.push_back(reco::CandidatePtr(ohSub, s));
      }
      groomed->emplace_back(sum, reco::Particle::Point(0, 0, 0), dau);
    }
    auto ohGroomed = iEvent.put(std::move(groomed), "groomed");
    putValueMap(iEvent, ohGroomed, std::vector<float>{0.11f, 0.22f, 0.33f}, "ecfN2");
  }

private:
  template <typename C, typename T>
  static void putValueMap(edm::Event& iEvent,
                          edm::OrphanHandle<C> const& h,
                          std::vector<T> const& values,
                          std::string const& instance) {
    auto vm = std::make_unique<edm::ValueMap<T>>();
    typename edm::ValueMap<T>::Filler f(*vm);
    f.insert(h, values.begin(), values.end());
    f.fill();
    iEvent.put(std::move(vm), instance);
  }

  const bool sortJets_;
  const bool unmapped_;
};

DEFINE_FWK_MODULE(PatGenJetTestInputs);
