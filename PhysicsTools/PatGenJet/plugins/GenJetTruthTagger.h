#ifndef PhysicsTools_PatGenJet_GenJetTruthTagger_h
#define PhysicsTools_PatGenJet_GenJetTruthTagger_h
// truth values under the reco tagger output names (bDiscriminator labels), so that every
// NanoAOD tagger column of Jet/FatJet/SubJet fills from the unchanged expressions. One-hot class probabilities,
// discriminators as sum(numerator)/sum(denominator) of the one-hot classes (0/0 -> 0), regressions "no correction".
// Rules: README.md, section on truth taggers. Only the labels the three tables read are stored.

#include "PhysicsTools/PatGenJet/interface/GenJet.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"

#include <string>
#include <vector>

namespace pat {

  class GenJetTruthTagger {
  public:
    enum class JetType { AK4, AK8 };

    // per-event generator objects, built once from prunedGenParticles (or any GenParticleCollection with mother links)
    struct Lepton {
      int pdg;  // 11 or 13 (abs)
      double eta, phi;
    };
    struct HadTau {
      double eta, phi;  // visible direction
      int charge, prongs, pi0s;
    };
    struct Leaf {
      int pdg;  // abs pdgId: quark 1-5, gluon 21, e 11, mu 13, hadronic tau 15, leptonic tau 1511 / 1513
      double eta, phi;
      int parent;  // abs pdgId of the boson it comes from (24, 23, 25) or 6 for the b of a top
    };
    struct Resonance {
      int pdg;  // 6, 23, 24, 25 (abs)
      double eta, phi;
      std::vector<Leaf> leaves;  // decay products without neutrinos
    };
    struct EventTruth {
      std::vector<Lepton> promptLeptons, bDecayLeptons;
      std::vector<HadTau> hadTaus;
      std::vector<Resonance> resonances;
    };

    static EventTruth eventTruth(const reco::GenParticleCollection* genParticles);

    // AK4 truth category and the AK8 class of each tagger family (exposed for tests and plots)
    struct AK4Truth {
      std::string flavour;  // b, bb, lepb, c, s, u, d, g
      std::string lepton;   // "", ele, mu, or the tau class name "taup1h0p" ...
    };
    static AK4Truth ak4Truth(const pat::GenJet& jet, const EventTruth& ev, double coneSize);

    struct AK8Truth {
      std::string glopart, pnet, pnetMD, pnetMiniAOD;  // class names without "prob"
      bool top = false, wHad = false, zHad = false;
    };
    static AK8Truth ak8Truth(const pat::GenJet& jet, const EventTruth& ev, double coneSize);

    // all values (label -> value) for a jet; only the labels the NanoAOD tables read
    static std::vector<std::pair<std::string, float>> values(const pat::GenJet& jet, const EventTruth& ev, JetType type);

    static const std::vector<std::string>& labels(JetType type);
  };

}  // namespace pat

#endif
