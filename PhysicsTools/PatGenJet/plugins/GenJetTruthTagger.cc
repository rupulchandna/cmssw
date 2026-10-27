// truth values under the reco tagger output names. Rules: README.md, section on truth taggers.
// Class lists and discriminator definitions as in RecoBTag/ONNXRuntime (CMSSW_20_1_0_pre3):
//   pfDeepFlavour_cff.py, pfParticleNetFromMiniAODAK4_cff.py (+DiscriminatorsJetTags_cfi), pfUnifiedParticleTransformerAK4
//   (UnifiedParticleTransformerAK4ONNXJetTagsProducer.cc flav_names, +DiscriminatorsJetTags_cfi),
//   pfGlobalParticleTransformerAK8_cff.py, pfParticleNet_cff.py (+pfParticleNetDiscriminatorsJetTags_cfi),
//   pfParticleNetFromMiniAODAK8_cff.py (+DiscriminatorsJetTags_cfi).

#include "PhysicsTools/PatGenJet/plugins/GenJetTruthTagger.h"

#include "DataFormats/Math/interface/deltaR.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

using pat::GenJetTruthTagger;

namespace {
  using Map = std::unordered_map<std::string, float>;

  bool isBHadron(int pdg) {
    const int a = std::abs(pdg);
    return (a / 100) % 10 == 5 || (a / 1000) % 10 == 5;
  }
  bool isNeutrino(int a) { return a == 12 || a == 14 || a == 16; }

  const reco::Candidate* lastCopy(const reco::Candidate* p) {
    for (int guard = 0; guard < 100; ++guard) {
      const reco::Candidate* next = nullptr;
      for (size_t i = 0; i < p->numberOfDaughters(); ++i)
        if (p->daughter(i)->pdgId() == p->pdgId())
          next = p->daughter(i);
      if (!next)
        break;
      p = next;
    }
    return p;
  }

  bool fromBHadron(const reco::Candidate* p) {
    for (int guard = 0; guard < 100 && p->numberOfMothers() > 0; ++guard) {
      p = p->mother(0);
      if (isBHadron(p->pdgId()))
        return true;
    }
    return false;
  }

  // abs pdgId of the e/mu a tau decays to, 0 if hadronic
  int tauLepton(const reco::Candidate* tau) {
    for (size_t i = 0; i < tau->numberOfDaughters(); ++i) {
      const int a = std::abs(tau->daughter(i)->pdgId());
      if (a == 11 || a == 13)
        return a;
    }
    return 0;
  }

  void visibleTau(const reco::Candidate* p, reco::Candidate::LorentzVector& vis, int& prongs, int& pi0s) {
    for (size_t i = 0; i < p->numberOfDaughters(); ++i) {
      const reco::Candidate* d = p->daughter(i);
      const int a = std::abs(d->pdgId());
      if (isNeutrino(a))
        continue;
      if (a == 111) {
        ++pi0s;
        vis += d->p4();
      } else if (d->numberOfDaughters() == 0 || d->status() == 1) {
        vis += d->p4();
        if (d->charge() != 0)
          ++prongs;
      } else {
        visibleTau(d, vis, prongs, pi0s);
      }
    }
  }

  void collectLeaves(const reco::Candidate* p, int parent, std::vector<GenJetTruthTagger::Leaf>& out) {
    for (size_t i = 0; i < p->numberOfDaughters(); ++i) {
      const reco::Candidate* d = p->daughter(i);
      const int a = std::abs(d->pdgId());
      if (a == std::abs(p->pdgId())) {
        collectLeaves(lastCopy(d), parent, out);
      } else if (a == 6 || a == 23 || a == 24 || a == 25) {
        collectLeaves(lastCopy(d), a, out);
      } else if (a == 15) {
        const reco::Candidate* t = lastCopy(d);
        const int l = tauLepton(t);
        out.push_back({l ? 1500 + l : 15, t->eta(), t->phi(), parent});
      } else if ((a >= 1 && a <= 5) || a == 21 || a == 11 || a == 13 || a == 22) {
        out.push_back({a, d->eta(), d->phi(), parent});
      }
    }
  }

  const std::vector<std::string> kTaus = {"taup1h0p",
                                          "taup1h1p",
                                          "taup1h2p",
                                          "taup3h0p",
                                          "taup3h1p",
                                          "taum1h0p",
                                          "taum1h1p",
                                          "taum1h2p",
                                          "taum3h0p",
                                          "taum3h1p"};

  std::vector<std::string> plus(std::vector<std::string> a, std::vector<std::string> const& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
  }

  // one-hot: every class of `prefix` 0, the true one 1 (an empty or unknown true class sets nothing to 1)
  void oneHot(Map& m, std::string const& prefix, std::vector<std::string> const& classes, std::string const& truth) {
    for (auto const& c : classes)
      m[prefix + "prob" + c] = (c == truth) ? 1.f : 0.f;
  }

  // discriminator as RecoBTag's BTagProbabilityToDiscriminator: sum(num)/sum(den); 0/0 -> 0
  float ratio(Map const& m, std::string const& prefix, std::vector<std::string> const& num, std::vector<std::string> const& den) {
    float n = 0, d = 0;
    for (auto const& c : num)
      n += m.at(prefix + "prob" + c);
    for (auto const& c : den)
      d += m.at(prefix + "prob" + c);
    return d > 0 ? n / d : 0.f;
  }

  void regressions(Map& m, std::string const& prefix) {
    m[prefix + "ptcorr"] = 1.f;
    m[prefix + "ptnu"] = 1.f;
    m[prefix + "ptreshigh"] = 0.f;
    m[prefix + "ptreslow"] = 0.f;
  }

  std::vector<std::string> prefixed(std::string const& prefix, std::vector<std::string> const& names) {
    std::vector<std::string> r;
    for (auto const& n : names)
      r.push_back(prefix + n);
    return r;
  }

  const std::string kDF = "pfDeepFlavourJetTags:";
  const std::string kPNC = "pfParticleNetFromMiniAODAK4PuppiCentralJetTags:";
  const std::string kPNF = "pfParticleNetFromMiniAODAK4PuppiForwardJetTags:";
  const std::string kPNCD = "pfParticleNetFromMiniAODAK4PuppiCentralDiscriminatorsJetTags:";
  const std::string kPNFD = "pfParticleNetFromMiniAODAK4PuppiForwardDiscriminatorsJetTags:";
  const std::string kUP = "pfUnifiedParticleTransformerAK4JetTags:";
  const std::string kUPD = "pfUnifiedParticleTransformerAK4DiscriminatorsJetTags:";
  const std::string kUP1 = "pfUnifiedParticleTransformerAK4V1JetTags:";
  const std::string kGP = "pfGlobalParticleTransformerAK8JetTags:";
  const std::string kPN = "pfParticleNetJetTags:";
  const std::string kPND = "pfParticleNetDiscriminatorsJetTags:";
  const std::string kMD = "pfMassDecorrelatedParticleNetJetTags:";
  const std::string kP8 = "pfParticleNetFromMiniAODAK8JetTags:";
  const std::string kP8D = "pfParticleNetFromMiniAODAK8DiscriminatorsJetTags:";
  const std::string kMassReg = "pfParticleNetMassRegressionJetTags:mass";

  const std::vector<std::string> kDFClasses = {"b", "bb", "lepb", "c", "uds", "g"};
  const std::vector<std::string> kPNCClasses = plus({"mu", "ele", "b", "c", "uds", "g"}, kTaus);
  const std::vector<std::string> kUPClasses = plus({"b", "bb", "lepb", "c", "s", "u", "d", "g", "ele", "mu"}, kTaus);
  const std::vector<std::string> kGPClasses = {"Xbb", "Xcc", "Xcs", "Xqq", "Xtauhtaue", "Xtauhtaum", "Xtauhtauh",
                                               "XWW4q", "XWW3q", "XWWqqev", "XWWqqmv", "TopbWqq", "TopbWq", "TopbWev",
                                               "TopbWmv", "TopbWtauhv", "QCD"};
  const std::vector<std::string> kPNClasses = {"Tbcq", "Tbqq", "Tbc", "Tbq", "Tbel", "Tbmu", "Tbta",
                                               "Wcq", "Wqq", "Zbb", "Zcc", "Zqq", "Hbb", "Hcc",
                                               "Hqqqq", "QCDbb", "QCDcc", "QCDb", "QCDc", "QCDothers"};
  const std::vector<std::string> kMDClasses = {"Xbb", "Xcc", "Xqq", "QCDbb", "QCDcc", "QCDb", "QCDc", "QCDothers"};
  const std::vector<std::string> kP8Classes = {"Htt", "Htm", "Hte", "Hbb", "Hcc", "Hqq", "Hgg", "QCD2hf", "QCD1hf", "QCD0hf"};

  struct Candidate8 {
    int priority = 0;
    double dR = 1e9;
    GenJetTruthTagger::AK8Truth t;
  };

  // classification of one resonance against the fat jet; priority 0 = no class
  Candidate8 classify(GenJetTruthTagger::Resonance const& r, pat::GenJet const& jet, double R) {
    Candidate8 c;
    auto inside = [&](GenJetTruthTagger::Leaf const& l) { return reco::deltaR(l.eta, l.phi, jet.eta(), jet.phi()) < R; };
    c.dR = reco::deltaR(r.eta, r.phi, jet.eta(), jet.phi());
    auto& t = c.t;
    if (r.pdg == 6) {
      const GenJetTruthTagger::Leaf* b = nullptr;
      std::vector<GenJetTruthTagger::Leaf> wq, wl;
      for (auto const& l : r.leaves) {
        if (l.parent == 6 && l.pdg == 5)
          b = &l;
        else if (l.parent == 24 && l.pdg <= 5)
          wq.push_back(l);
        else if (l.parent == 24 && l.pdg != 22)
          wl.push_back(l);
      }
      if (!b || !inside(*b))
        return c;
      if (wq.size() == 2 && wl.empty()) {
        const int nin = inside(wq[0]) + inside(wq[1]);
        if (nin == 2) {
          t.glopart = "TopbWqq";
          t.pnet = (wq[0].pdg == 4 || wq[1].pdg == 4) ? "Tbcq" : "Tbqq";
        } else if (nin == 1) {
          t.glopart = "TopbWq";
          t.pnet = (inside(wq[0]) ? wq[0] : wq[1]).pdg == 4 ? "Tbc" : "Tbq";
        } else {
          return c;
        }
      } else if (wq.empty() && wl.size() == 1 && inside(wl[0])) {
        const int l = wl[0].pdg;
        t.glopart = (l == 11 || l == 1511) ? "TopbWev" : (l == 13 || l == 1513) ? "TopbWmv" : "TopbWtauhv";
        t.pnet = l == 11 ? "Tbel" : l == 13 ? "Tbmu" : "Tbta";
      } else {
        return c;
      }
      t.top = true;
      c.priority = 4;
      return c;
    }
    // W, Z, H
    for (auto const& l : r.leaves)
      if (l.pdg == 22)
        return c;
    const bool viaBosons = r.pdg == 25 && !r.leaves.empty() && r.leaves.front().parent != 25;
    if (viaBosons) {  // H -> WW/ZZ
      int nq = 0, nqIn = 0, nOut = 0;
      int lep = 0;
      for (auto const& l : r.leaves) {
        if (l.pdg <= 5) {
          ++nq;
          nqIn += inside(l);
        } else {
          lep = l.pdg;
          nOut += !inside(l);
        }
      }
      if (nq == 4 && nqIn == 4) {
        t.glopart = "XWW4q";
        t.pnet = "Hqqqq";
      } else if (nq == 4 && nqIn == 3) {
        t.glopart = "XWW3q";
      } else if (nq == 2 && nqIn == 2 && r.leaves.size() == 3 && nOut == 0 && (lep == 11 || lep == 13)) {
        t.glopart = lep == 11 ? "XWWqqev" : "XWWqqmv";
      } else {
        return c;
      }
      c.priority = 3;
      return c;
    }
    if (r.leaves.size() != 2 || !inside(r.leaves[0]) || !inside(r.leaves[1]))
      return c;
    const int a = r.leaves[0].pdg, b = r.leaves[1].pdg;
    std::string two;  // bb, cc, cs, qq, gg, tauhtauh, tauhtaue, tauhtaum
    if (a <= 5 && b <= 5) {
      const int nb = (a == 5) + (b == 5), nc = (a == 4) + (b == 4);
      two = nb == 2 ? "bb" : nc == 2 ? "cc" : nc == 1 ? "cs" : "qq";
    } else if (a == 21 && b == 21) {
      two = "gg";
    } else if ((a == 15 || a > 1500) && (b == 15 || b > 1500) && (a == 15 || b == 15)) {
      const int other = a == 15 ? b : a;
      two = other == 15 ? "tauhtauh" : other == 1511 ? "tauhtaue" : "tauhtaum";
    } else {
      return c;
    }
    const bool quarks = two == "bb" || two == "cc" || two == "cs" || two == "qq";
    // GlobalParT X classes (any of W, Z, H)
    if (two == "bb")
      t.glopart = "Xbb";
    else if (two == "cc")
      t.glopart = "Xcc";
    else if (two == "cs")
      t.glopart = "Xcs";
    else if (two == "qq")
      t.glopart = "Xqq";
    else if (two != "gg")
      t.glopart = "X" + two;
    // ParticleNet (legacy, with mass): resonance-specific classes
    if (r.pdg == 24 && quarks)
      t.pnet = (two == "qq") ? "Wqq" : "Wcq";
    else if (r.pdg == 23 && quarks)
      t.pnet = two == "bb" ? "Zbb" : two == "cc" ? "Zcc" : "Zqq";
    else if (r.pdg == 25 && (two == "bb" || two == "cc"))
      t.pnet = two == "bb" ? "Hbb" : "Hcc";
    // mass-decorrelated ParticleNet (legacy) and ParticleNet from MiniAOD: generic X/H
    if (quarks)
      t.pnetMD = two == "bb" ? "Xbb" : two == "qq" ? "Xqq" : "Xcc";
    if (quarks)
      t.pnetMiniAOD = two == "bb" ? "Hbb" : two == "qq" ? "Hqq" : "Hcc";
    else if (two == "gg")
      t.pnetMiniAOD = "Hgg";
    else
      t.pnetMiniAOD = two == "tauhtauh" ? "Htt" : two == "tauhtaue" ? "Hte" : "Htm";
    t.wHad = r.pdg == 24 && quarks;
    t.zHad = r.pdg == 23 && quarks;
    c.priority = r.pdg == 25 ? 3 : r.pdg == 23 ? 2 : 1;
    return c;
  }
}  // namespace

GenJetTruthTagger::EventTruth GenJetTruthTagger::eventTruth(const reco::GenParticleCollection* gps) {
  EventTruth ev;
  if (!gps)
    return ev;
  for (auto const& p : *gps) {
    const int a = std::abs(p.pdgId());
    if ((a == 11 || a == 13) && p.status() == 1) {
      if (p.statusFlags().isPrompt() || p.statusFlags().isDirectPromptTauDecayProduct())
        ev.promptLeptons.push_back({a, p.eta(), p.phi()});
      else if (fromBHadron(&p))
        ev.bDecayLeptons.push_back({a, p.eta(), p.phi()});
    } else if (a == 15 && p.isLastCopy() && tauLepton(&p) == 0) {
      reco::Candidate::LorentzVector vis;
      int prongs = 0, pi0s = 0;
      visibleTau(&p, vis, prongs, pi0s);
      if (vis.pt() <= 0)
        vis = p.p4();
      ev.hadTaus.push_back({vis.eta(), vis.phi(), p.charge() > 0 ? 1 : -1, prongs >= 3 ? 3 : 1, pi0s});
    } else if ((a == 6 || a == 23 || a == 24 || a == 25) && p.isLastCopy()) {
      Resonance r{a, p.eta(), p.phi(), {}};
      collectLeaves(&p, a, r.leaves);
      ev.resonances.push_back(std::move(r));
    }
  }
  return ev;
}

GenJetTruthTagger::AK4Truth GenJetTruthTagger::ak4Truth(const pat::GenJet& jet, const EventTruth& ev, double R) {
  AK4Truth t;
  auto near = [&](double eta, double phi) { return reco::deltaR(eta, phi, jet.eta(), jet.phi()) < R; };
  for (auto const& tau : ev.hadTaus) {
    if (near(tau.eta, tau.phi)) {
      const int pi0 = tau.prongs == 3 ? std::min(tau.pi0s, 1) : std::min(tau.pi0s, 2);
      t.lepton = std::string("tau") + (tau.charge > 0 ? "p" : "m") + std::to_string(tau.prongs) + "h" +
                 std::to_string(pi0) + "p";
      break;
    }
  }
  for (int want : {13, 11}) {
    if (!t.lepton.empty())
      break;
    for (auto const& l : ev.promptLeptons)
      if (l.pdg == want && near(l.eta, l.phi)) {
        t.lepton = want == 13 ? "mu" : "ele";
        break;
      }
  }
  const int hf = jet.hadronFlavour();
  if (hf == 5) {
    t.flavour = "b";
    if (jet.jetFlavourInfo().getbHadrons().size() >= 2)
      t.flavour = "bb";
    else
      for (auto const& l : ev.bDecayLeptons)
        if (near(l.eta, l.phi)) {
          t.flavour = "lepb";
          break;
        }
  } else if (hf == 4) {
    t.flavour = "c";
  } else {
    const int pf = std::abs(jet.partonFlavour());
    t.flavour = pf == 21 ? "g" : pf == 3 ? "s" : pf == 1 ? "d" : "u";
  }
  return t;
}

GenJetTruthTagger::AK8Truth GenJetTruthTagger::ak8Truth(const pat::GenJet& jet, const EventTruth& ev, double R) {
  Candidate8 best;
  for (auto const& r : ev.resonances) {
    Candidate8 c = classify(r, jet, R);
    if (c.priority > best.priority || (c.priority > 0 && c.priority == best.priority && c.dR < best.dR))
      best = c;
  }
  AK8Truth t = best.t;
  const size_t nB = jet.jetFlavourInfo().getbHadrons().size(), nC = jet.jetFlavourInfo().getcHadrons().size();
  const std::string qcd5 = nB >= 2 ? "QCDbb" : nB == 1 ? "QCDb" : nC >= 2 ? "QCDcc" : nC == 1 ? "QCDc" : "QCDothers";
  const size_t hf = nB + nC;
  if (t.glopart.empty())
    t.glopart = "QCD";
  if (t.pnet.empty())
    t.pnet = qcd5;
  if (t.pnetMD.empty())
    t.pnetMD = qcd5;
  if (t.pnetMiniAOD.empty())
    t.pnetMiniAOD = hf >= 2 ? "QCD2hf" : hf == 1 ? "QCD1hf" : "QCD0hf";
  return t;
}

const std::vector<std::string>& GenJetTruthTagger::labels(JetType type) {
  static const std::vector<std::string> ak4 = [] {
    std::vector<std::string> v;
    const std::vector<std::string> reg = {"ptcorr", "ptnu", "ptreshigh", "ptreslow"};
    for (auto const& s : {std::make_pair(kDF, std::vector<std::string>{"probb", "probbb", "problepb", "probc", "probuds", "probg"}),
                          std::make_pair(kPNC, plus({"probb", "probc"}, reg)),
                          std::make_pair(kPNF, reg),
                          std::make_pair(kPNCD, std::vector<std::string>{"BvsAll", "CvsL", "CvsB", "QvsG", "TauVsJet"}),
                          std::make_pair(kPNFD, std::vector<std::string>{"QvsG"}),
                          std::make_pair(kUP,
                                         plus({"probb", "probbb", "problepb", "probc", "probu", "probd", "probg", "probele", "probmu"},
                                              reg)),
                          std::make_pair(kUPD,
                                         std::vector<std::string>{"BvsAll", "CvsL", "CvsB", "QvsG", "SvsUDG", "SvsBC", "TauVsJet"}),
                          std::make_pair(kUP1, reg)})
      v = plus(v, prefixed(s.first, s.second));
    return v;
  }();
  static const std::vector<std::string> ak8 = [] {
    std::vector<std::string> v = prefixed(kGP + "prob", plus(kGPClasses, {"WithMassTopvsQCD", "WithMassWvsQCD", "WithMassZvsQCD"}));
    v = plus(v, prefixed(kGP, {"massCorrX2p", "massCorrGeneric"}));
    v = plus(v, prefixed(kPN + "prob", {"QCDbb", "QCDcc", "QCDb", "QCDc", "QCDothers"}));
    v = plus(v, prefixed(kPND, {"TvsQCD", "WvsQCD", "ZvsQCD", "HbbvsQCD", "HccvsQCD", "H4qvsQCD"}));
    v = plus(v, prefixed(kMD + "prob", kMDClasses));
    v = plus(v, prefixed(kP8, {"probQCD2hf", "probQCD1hf", "probQCD0hf", "masscorr"}));
    v = plus(v, prefixed(kP8D, {"HbbvsQCD", "HccvsQCD", "HqqvsQCD", "HggvsQCD", "HttvsQCD", "HtmvsQCD", "HtevsQCD", "WvsQCD"}));
    v.push_back(kMassReg);
    return v;
  }();
  return type == JetType::AK4 ? ak4 : ak8;
}

std::vector<std::pair<std::string, float>> GenJetTruthTagger::values(const pat::GenJet& jet,
                                                                     const EventTruth& ev,
                                                                     JetType type) {
  Map m;
  if (type == JetType::AK4) {
    const AK4Truth t = ak4Truth(jet, ev, 0.4);
    const bool isB = t.flavour == "b" || t.flavour == "bb" || t.flavour == "lepb";
    const std::string light = (t.flavour == "s" || t.flavour == "u" || t.flavour == "d") ? "uds" : t.flavour;
    oneHot(m, kDF, kDFClasses, light);
    oneHot(m, kPNC, kPNCClasses, !t.lepton.empty() ? t.lepton : isB ? "b" : light);
    oneHot(m, kPNF, {"q", "g"}, t.flavour == "g" ? "g" : "q");
    oneHot(m, kUP, kUPClasses, !t.lepton.empty() ? t.lepton : t.flavour);
    for (auto const& p : {kPNC, kPNF, kUP, kUP1})
      regressions(m, p);
    const std::vector<std::string> all4 = {"b", "c", "uds", "g"};
    m[kPNCD + "BvsAll"] = ratio(m, kPNC, {"b"}, all4);
    m[kPNCD + "CvsL"] = ratio(m, kPNC, {"c"}, {"c", "uds", "g"});
    m[kPNCD + "CvsB"] = ratio(m, kPNC, {"c"}, {"c", "b"});
    m[kPNCD + "QvsG"] = ratio(m, kPNC, {"uds"}, {"uds", "g"});
    m[kPNCD + "TauVsJet"] = ratio(m, kPNC, kTaus, plus(all4, kTaus));
    m[kPNFD + "QvsG"] = ratio(m, kPNF, {"q"}, {"q", "g"});
    const std::vector<std::string> bs = {"b", "bb", "lepb"}, uds = {"s", "u", "d"};
    const std::vector<std::string> jets = plus(plus(bs, {"c"}), plus(uds, {"g"}));
    m[kUPD + "BvsAll"] = ratio(m, kUP, bs, jets);
    m[kUPD + "CvsL"] = ratio(m, kUP, {"c"}, plus({"c"}, plus(uds, {"g"})));
    m[kUPD + "CvsB"] = ratio(m, kUP, {"c"}, plus({"c"}, bs));
    m[kUPD + "QvsG"] = ratio(m, kUP, uds, plus(uds, {"g"}));
    m[kUPD + "SvsUDG"] = ratio(m, kUP, {"s"}, {"s", "u", "d", "g"});
    m[kUPD + "SvsBC"] = ratio(m, kUP, {"s"}, plus({"s"}, plus(bs, {"c"})));
    m[kUPD + "TauVsJet"] = ratio(m, kUP, kTaus, plus(jets, kTaus));
  } else {
    const AK8Truth t = ak8Truth(jet, ev, 0.8);
    oneHot(m, kGP, kGPClasses, t.glopart);
    m[kGP + "probWithMassTopvsQCD"] = t.top ? 1.f : 0.f;
    m[kGP + "probWithMassWvsQCD"] = t.wHad ? 1.f : 0.f;
    m[kGP + "probWithMassZvsQCD"] = t.zHad ? 1.f : 0.f;
    m[kGP + "massCorrX2p"] = 1.f;
    m[kGP + "massCorrGeneric"] = 1.f;
    oneHot(m, kPN, kPNClasses, t.pnet);
    const std::vector<std::string> q5 = {"QCDbb", "QCDcc", "QCDb", "QCDc", "QCDothers"};
    m[kPND + "TvsQCD"] = ratio(m, kPN, {"Tbcq", "Tbqq"}, plus({"Tbcq", "Tbqq"}, q5));
    m[kPND + "WvsQCD"] = ratio(m, kPN, {"Wcq", "Wqq"}, plus({"Wcq", "Wqq"}, q5));
    m[kPND + "ZvsQCD"] = ratio(m, kPN, {"Zbb", "Zcc", "Zqq"}, plus({"Zbb", "Zcc", "Zqq"}, q5));
    for (auto const& h : {"Hbb", "Hcc"})
      m[kPND + h + "vsQCD"] = ratio(m, kPN, {h}, plus({h}, q5));
    m[kPND + "H4qvsQCD"] = ratio(m, kPN, {"Hqqqq"}, plus({"Hqqqq"}, q5));
    oneHot(m, kMD, kMDClasses, t.pnetMD);
    oneHot(m, kP8, kP8Classes, t.pnetMiniAOD);
    m[kP8 + "masscorr"] = 1.f;
    const std::vector<std::string> q3 = {"QCD2hf", "QCD1hf", "QCD0hf"};
    for (auto const& h : {"Hbb", "Hcc", "Hqq", "Hgg", "Htt", "Htm", "Hte"})
      m[kP8D + h + "vsQCD"] = ratio(m, kP8, {h}, plus({h}, q3));
    m[kP8D + "WvsQCD"] = ratio(m, kP8, {"Hqq", "Hcc"}, plus({"Hqq", "Hcc"}, q3));
    m[kMassReg] = jet.mass();
  }
  std::vector<std::pair<std::string, float>> out;
  for (auto const& label : labels(type))
    out.emplace_back(label, m.at(label));
  return out;
}
