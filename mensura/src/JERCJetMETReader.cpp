#include <JERCJetMETReader.hpp>

#include <mensura/core/FileInPath.hpp>
#include <mensura/core/PileUpReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/PhysicsObjects.hpp>
#include <mensura/core/ROOTLock.hpp>
#include <mensura/PECReader/PECInputData.hpp>

#include <TVector2.h>

#include <cmath>
#include <iostream>
#include <limits>


JERCJetMETReader::JERCJetMETReader(std::string name /*= "JetMET"*/):
    JetMETReader(name),
    inputDataPluginName("InputData"), inputDataPlugin(nullptr),
    treeName("basicJetMET/JetMET"),
    minPt(0.), maxAbsEta(std::numeric_limits<double>::infinity()),
    applyJetID(true),
    leptonPluginName("Leptons"), leptonPlugin(nullptr),
    genJetPluginName(""), genJetPlugin(nullptr),
    puPluginName("PileUp"), puPlugin(nullptr),
    jerFilePath(""), jerPtFactor(0.)
{
    leptonDR2 = std::pow(GetJetRadius(), 2);
}


JERCJetMETReader::JERCJetMETReader(JERCJetMETReader const &src) noexcept:
    JetMETReader(src),
    inputDataPluginName(src.inputDataPluginName), inputDataPlugin(src.inputDataPlugin),
    treeName(src.treeName),
    minPt(src.minPt), maxAbsEta(src.maxAbsEta), jetSelector(src.jetSelector),
    applyJetID(src.applyJetID),
    leptonPluginName(src.leptonPluginName), leptonPlugin(src.leptonPlugin),
    leptonDR2(src.leptonDR2),
    genJetPluginName(src.genJetPluginName), genJetPlugin(src.genJetPlugin),
    puPluginName(src.puPluginName), puPlugin(src.puPlugin),
    jerFilePath(src.jerFilePath), jerPtFactor(src.jerPtFactor)
{}


void JERCJetMETReader::BeginRun(Dataset const &)
{
    // Save pointers to required plugins
    inputDataPlugin = dynamic_cast<PECInputData const *>(GetDependencyPlugin(inputDataPluginName));
    
    if (leptonPluginName != "")
        leptonPlugin = dynamic_cast<LeptonReader const *>(GetDependencyPlugin(leptonPluginName));
    
    if (genJetPluginName != "")
        genJetPlugin = dynamic_cast<GenJetMETReader const *>(GetDependencyPlugin(genJetPluginName));
    
    if (jerFilePath != "")
        puPlugin = dynamic_cast<PileUpReader const *>(GetDependencyPlugin(puPluginName));
    
    
    // Set up the tree. Branches with properties that are not currently not used, are disabled
    inputDataPlugin->LoadTree(treeName);
    TTree *tree = inputDataPlugin->ExposeTree(treeName);
    
    ROOTLock::Lock();
    
    for (std::string const &propertyName: {"bTagCMVA", "bTagDeepCSV[4]", "pileupDiscr",
      "flavourHadron", "flavourParton", "hasGenMatch"})
        tree->SetBranchStatus(("jets." + propertyName).c_str(), false);
    
    bfJets = nullptr;
    bfMET = nullptr;
    
    tree->SetBranchAddress("jets", &bfJets);
    tree->SetBranchAddress("met", &bfMET);
    
    ROOTLock::Unlock();
    
    
    // Create an object to access jet pt resolution
    if (jerFilePath != "")
        jerProvider.reset(new JME::JetResolution(jerFilePath));
}


Plugin *JERCJetMETReader::Clone() const
{
    return new JERCJetMETReader(*this);
}


void JERCJetMETReader::ConfigureLeptonCleaning(std::string const leptonPluginName_, double dR)
{
    leptonPluginName = leptonPluginName_;
    leptonDR2 = dR * dR;
}


void JERCJetMETReader::ConfigureLeptonCleaning(std::string const leptonPluginName_ /*= "Leptons"*/)
{
    leptonPluginName = leptonPluginName_;
    leptonDR2 = std::pow(GetJetRadius(), 2);
}


double JERCJetMETReader::GetJetRadius() const
{
    return 0.4;
}


void JERCJetMETReader::SetApplyJetID(bool applyJetID_)
{
    applyJetID = applyJetID_;
}


void JERCJetMETReader::SetGenJetReader(std::string const name /*= "GenJetMET"*/)
{
    genJetPluginName = name;
}


void JERCJetMETReader::SetGenPtMatching(std::string const &jerFile, double jerPtFactor_ /*= 3.*/)
{
    jerFilePath = FileInPath::Resolve("JERC", jerFile);
    jerPtFactor = jerPtFactor_;
}


void JERCJetMETReader::SetSelection(double minPt_, double maxAbsEta_)
{
    minPt = minPt_;
    maxAbsEta = maxAbsEta_;
}


void JERCJetMETReader::SetSelection(std::function<bool(Jet const &)> jetSelector_)
{
    jetSelector = jetSelector_;
}


bool JERCJetMETReader::ProcessEvent()
{
    // Clear vector with jets from the previous event
    jets.clear();
    
    
    // Read jets and MET
    inputDataPlugin->ReadEventFromTree(treeName);
    
    // Collection of leptons against which jets will be cleaned
    auto const *leptonsForCleaning = (leptonPlugin) ? &leptonPlugin->GetLeptons() : nullptr;
    
    
    // Header for debug print out
    #ifdef DEBUG
    std::cout << "JERCJetMETReader[\"" << GetName() << "\"]: Jets in the current event:\n";
    unsigned curJetNumber = 0;
    #endif
    
    
    // Process jets in the current event
    for (jec::Jet const &j: *bfJets)
    {
        // Read raw jet momentum and apply corrections to it. The correction factor read from
        //pec::Jet is zero if only raw momentum is stored. In this case propagate the raw momentum
        //unchanged.
        TLorentzVector p4;
        p4.SetPtEtaPhiM(j.ptRaw, j.etaRaw, j.phiRaw, j.massRaw);
        
        double const corrFactor = j.jecFactor;
        
        if (corrFactor != 0.)
            p4 *= corrFactor;
        
        
        #ifdef DEBUG
        ++curJetNumber;
        std::cout << " Jet #" << curJetNumber << "\n";
        std::cout << "  Raw momentum (pt, eta, phi, m): " << j.ptRaw << ", " << j.etaRaw << ", " <<
          phiRaw << ", " << j.massRaw << '\n';
        std::cout << "  Fully corrected pt: " << p4.Pt() << '\n';
        #endif
        
        
        // Loose physics selection
        if (applyJetID and not j.isGood /* "loose" jet ID */)
            continue;
        
        
        // User-defined selection on momentum
        if (p4.Pt() < minPt or fabs(p4.Eta()) > maxAbsEta)
            continue;
        
        
        // Peform cleaning against leptons if enabled
        if (leptonsForCleaning)
        {
            bool overlap = false;
            
            for (auto const &l: *leptonsForCleaning)
            {
                double const dR2 = std::pow(p4.Eta() - l.Eta(), 2) +
                  std::pow(TVector2::Phi_mpi_pi(p4.Phi() - l.Phi()), 2);
                //^ Do not use TLorentzVector::DeltaR to avoid calculating sqrt
                
                if (dR2 < leptonDR2)
                {
                    overlap = true;
                    break;
                }
            }
            
            if (overlap)
                continue;
        }
        
        
        #ifdef DEBUG
        std::cout << "  Jet passes selection on kinematics and ID and does not overlap with " <<
          "a lepton\n";
        #endif
        
        
        // Build the jet object. At this point jet momentum must be fully corrected
        Jet jet;
        
        if (corrFactor != 0.)
            jet.SetCorrectedP4(p4, 1. / corrFactor);
        else
            jet.SetCorrectedP4(p4, 1.);
        
        jet.SetArea(j.area);
        
        // jet.SetBTag(BTagger::Algorithm::CMVA, j.bTagCMVA);
        // jet.SetPileUpID(j.pileupDiscr);
        
        // jet.SetFlavour(Jet::FlavourType::Hadron, j.flavourHadron);
        // jet.SetFlavour(Jet::FlavourType::Parton, j.flavourParton);
        
        if (not applyJetID)
            jet.SetUserInt("ID", int(j.isGood));
        
        
        // Perform matching to generator-level jets if the corresponding reader is available.
        //Choose the closest jet but require that the angular separation is not larger than half of
        //the radius parameter of reconstructed jets and, if the plugin has been configured to
        //check this, that the difference in pt is compatible with the pt resolution in simulation.
        if (genJetPlugin)
        {
            double minDR2 = std::pow(GetJetRadius() / 2., 2);
            GenJet const *matchedGenJet = nullptr;
            double maxDPt = std::numeric_limits<double>::infinity();
            
            if (jerProvider)
            {
                double const ptResolution = jerProvider->getResolution(
                  {{JME::Binning::JetPt, p4.Pt()}, {JME::Binning::JetEta, p4.Eta()},
                  {JME::Binning::Rho, puPlugin->GetRho()}});
                maxDPt = ptResolution * p4.Pt() * jerPtFactor;
            }
            
            
            for (auto const &genJet: genJetPlugin->GetJets())
            {
                double const dR2 = std::pow(p4.Eta() - genJet.Eta(), 2) +
                  std::pow(TVector2::Phi_mpi_pi(p4.Phi() - genJet.Phi()), 2);
                //^ Do not use TLorentzVector::DeltaR to avoid calculating sqrt
                
                if (dR2 < minDR2 and std::abs(p4.Pt() - genJet.Pt()) < maxDPt)
                {
                    matchedGenJet = &genJet;
                    minDR2 = dR2;
                }
            }
            
            jet.SetMatchedGenJet(matchedGenJet);
        }
        
        #ifdef DEBUG
        std::cout << "  Flavour: " << j.Flavour() << ", cMVA value: " <<
          j.BTag(pec::Jet::BTagAlgo::CMVA) << '\n';
        std::cout << "  Has a GEN-level match? ";
        
        if (genJetPlugin)
        {
            if (jet.MatchedGenJet())
                std::cout << "yes";
            else
                std::cout << "no";
        }
        else
            std::cout << "n/a";
        
        std::cout << '\n';
        #endif
        
        
        // Generic selection on the jet
        if (jetSelector and not jetSelector(jet))
            continue;
        
        
        jets.push_back(jet);
    }
    
    
    // Make sure collection of jets is ordered in transverse momentum
    std::sort(jets.rbegin(), jets.rend());
    
    
    // Read raw missing pt. The corrected missing pt is not available and set to null.
    rawMET.SetPtEtaPhiM(bfMET->ptRaw, 0., bfMET->phiRaw, 0.);
    met.SetPtEtaPhiM(0., 0., 0., 0.);
    
    
    #ifdef DEBUG
    std::cout << "JERCJetMETReader[\"" << GetName() << "\"]: MET in the current event:\n";
    std::cout << " Raw MET (pt, phi): " << rawMET.Pt() << ", " << rawMET.Phi() << '\n';
    std::cout << " Corrected MET is not filled" << std::endl;
    #endif
    
    
    
    // Since this reader does not have access to the input file, it does not know when there are
    //no more events in the dataset and thus always returns true
    return true;
}

