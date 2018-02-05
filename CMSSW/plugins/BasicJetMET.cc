#include "BasicJetMET.h"

#include <FWCore/Utilities/interface/InputTag.h>
#include <FWCore/Framework/interface/MakerMacros.h>

#include <FWCore/Utilities/interface/Exception.h>

#include <cmath>


BasicJetMET::BasicJetMET(edm::ParameterSet const &cfg):
    runOnData(cfg.getParameter<bool>("runOnData"))
{
    // Register required input data
    jetToken = consumes<edm::View<pat::Jet>>(cfg.getParameter<edm::InputTag>("jets"));
    metToken = consumes<edm::View<pat::MET>>(cfg.getParameter<edm::InputTag>("met"));
    
    
    // Read version of jet ID
    std::string const jetIDLabel = cfg.getParameter<std::string>("jetIDVersion");
    
    if (jetIDLabel == "2017")
        jetIDVersion = JetID::Ver2017;
    else if (jetIDLabel == "2016")
        jetIDVersion = JetID::Ver2016;
    else
    {
        cms::Exception excp("Configuration");
        excp << "Jet ID version \"" << jetIDLabel << "\" is not known.";
        excp.raise();
    }
}


void BasicJetMET::fillDescriptions(edm::ConfigurationDescriptions &descriptions)
{
    edm::ParameterSetDescription desc;
    desc.add<bool>("runOnData")->
      setComment("Indicates whether data or simulation is being processed.");
    desc.add<edm::InputTag>("jets")->setComment("Collection of jets.");
    desc.add<std::vector<std::string>>("jetSelection", std::vector<std::string>())->
      setComment("User-defined selections for jets whose results will be stored in the output "
      "tree.");
    desc.add<std::string>("jetIDVersion")->setComment("Version of jet ID to evaluate.");
    desc.add<edm::InputTag>("met")->setComment("Missing pt.");
    
    descriptions.add("basicJetMET", desc);
}


void BasicJetMET::beginJob()
{
    outTree = fileService->make<TTree>("JetMET", "Reconstructed jets and missing pt");
    
    storeJets = nullptr;
    storeMET = nullptr;
    
    outTree->Branch("jets", &storeJets);
    outTree->Branch("met", &storeMET);
}


void BasicJetMET::analyze(edm::Event const &event, edm::EventSetup const &)
{
    // Read the jet collection
    edm::Handle<edm::View<pat::Jet>> srcJets;
    event.getByToken(jetToken, srcJets);
    
    
    // Loop through the collection and store relevant properties of jets
    storeJets->clear();
    jec::Jet storeJet;  // will reuse this object to fill the vector
    
    for (auto const &j: *srcJets)
    {
        reco::Candidate::LorentzVector const &rawP4 = j.correctedP4("Uncorrected");
        
        storeJet.ptRaw = rawP4.pt();
        storeJet.etaRaw = rawP4.eta();
        storeJet.phiRaw = rawP4.phi();
        storeJet.massRaw = rawP4.mass();
        
        storeJet.jecFactor = 1. / j.jecFactor("Uncorrected");
        //^ Here jecFactor("Uncorrected") returns the factor to get raw momentum starting from the
        //corrected one. Since in fact the raw momentum is stored, the factor is inverted.
        
        storeJet.area = j.jetArea();
        
        
        storeJet.bTagCMVA = j.bDiscriminator("pfCombinedMVAV2BJetTags");
        
        storeJet.bTagDeepCSV[0] = j.bDiscriminator("pfDeepCSVJetTags:probbb");
        storeJet.bTagDeepCSV[1] = j.bDiscriminator("pfDeepCSVJetTags:probb");
        storeJet.bTagDeepCSV[2] = j.bDiscriminator("pfDeepCSVJetTags:probc");
        storeJet.bTagDeepCSV[3] = j.bDiscriminator("pfDeepCSVJetTags:probudsg");
        
        
        // Pileup ID
        //[1] https://twiki.cern.ch/twiki/bin/viewauth/CMS/PileupJetID?rev=29#Information_for_13_TeV_data_anal
        storeJet.pileupDiscr = j.userFloat("pileupJetId:fullDiscriminant");
        
        
        if (not runOnData)
        {
            storeJet.flavourHadron = j.hadronFlavour();
            storeJet.flavourParton = j.partonFlavour();
            storeJet.hasGenMatch = bool(j.userInt("hasGenMatch"));
        }
        else
        {
            storeJet.flavourHadron = 0;
            storeJet.flavourParton = 0;
            storeJet.hasGenMatch = false;
        }
        
        
        // PF jet ID [1-2]. Accessors to energy fractions take into account JEC, so there is no
        //need to undo the corrections.
        //[1] https://twiki.cern.ch/twiki/bin/view/CMS/JetID13TeVRun2016?rev=1
        //[2] https://twiki.cern.ch/twiki/bin/view/CMS/JetID13TeVRun2017?rev=6
        bool passPFID = false;
        double const absEta = std::abs(rawP4.Eta());
        
        if (jetIDVersion == JetID::Ver2016)
        {
            if (absEta <= 2.7)
            {
                bool const commonCriteria = (j.neutralHadronEnergyFraction() < 0.99 and
                  j.neutralEmEnergyFraction() < 0.99 and
                  (j.chargedMultiplicity() + j.neutralMultiplicity() > 1));
                
                if (absEta <= 2.4)
                    passPFID = (commonCriteria and j.chargedHadronEnergyFraction() > 0. and
                      j.chargedMultiplicity() > 0 and j.chargedEmEnergyFraction() < 0.99);
                else
                    passPFID = commonCriteria;
            }
            else if (absEta <= 3.)
                passPFID = (j.neutralMultiplicity() > 2 and
                  j.neutralHadronEnergyFraction() < 0.98 and j.neutralEmEnergyFraction() > 0.01);
            else
                passPFID = (j.neutralMultiplicity() > 10 and j.neutralEmEnergyFraction() < 0.9);
        }
        else if (jetIDVersion == JetID::Ver2017)
        {
            // Use the "TightLepVeto" working point as recommended in [1]
            //[1] https://hypernews.cern.ch/HyperNews/CMS/get/jet-algorithms/462/3/1.html
            if (absEta <= 2.7)
            {
                bool const commonCriteria = (j.neutralHadronEnergyFraction() < 0.9 and
                  j.neutralEmEnergyFraction() < 0.9  and j.muonEnergyFraction() < 0.8 and
                  j.numberOfDaughters() > 1);
                
                if (absEta <= 2.4)
                    passPFID = (commonCriteria and j.chargedHadronEnergyFraction() > 0. and
                      j.chargedMultiplicity() > 0 and j.chargedEmEnergyFraction() < 0.8);
                else
                    passPFID = commonCriteria;
            }
            else if (absEta <= 3.)
                passPFID = (j.neutralMultiplicity() > 2 and
                  j.neutralEmEnergyFraction() < 0.99 and j.neutralEmEnergyFraction() > 0.02);
            else
                passPFID = (j.neutralMultiplicity() > 10 and j.neutralEmEnergyFraction() < 0.9 and
                  j.neutralHadronEnergyFraction() > 0.02);
        }
        
        storeJet.isGood = passPFID;
        
        
        // The jet is set up. Add it to the vector.
        storeJets->emplace_back(storeJet);
    }
    
    
    // Read MET
    edm::Handle<edm::View<pat::MET>> metHandle;
    event.getByToken(metToken, metHandle);
    pat::MET const &met = metHandle->front();
    
    storeMET->ptRaw = met.corPt(pat::MET::RawChs);
    storeMET->phiRaw = met.corPhi(pat::MET::RawChs);
    
    
    // Fill the output tree
    outTree->Fill();
}


DEFINE_FWK_MODULE(BasicJetMET);
