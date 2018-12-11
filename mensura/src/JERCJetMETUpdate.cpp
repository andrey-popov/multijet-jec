#include <JERCJetMETUpdate.hpp>

#include <mensura/core/EventIDReader.hpp>
#include <mensura/core/PileUpReader.hpp>
#include <mensura/core/Processor.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>


JERCJetMETUpdate::JERCJetMETUpdate(std::string const name /*= "JetMET"*/):
    JetMETReader(name),
    jetmetPlugin(nullptr), jetmetPluginName("OrigJetMET"),
    eventIDPlugin(nullptr), eventIDPluginName("InputData"),
    puPlugin(nullptr), puPluginName("PileUp"),
    systServiceName("Systematics"),
    jetCorrForJets(nullptr),
    jetCorrForMETFull(nullptr), jetCorrForMETL1(nullptr),
    jetCorrForMETOrigFull(nullptr), jetCorrForMETOrigL1(nullptr),
    minPt(0.), maxAbsEta(std::numeric_limits<double>::infinity()), minPtForT1(15.),
    useRawMET(false)
{}


void JERCJetMETUpdate::BeginRun(Dataset const &)
{
    // Save pointers to the original JetMETReader and a PileUpReader
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    eventIDPlugin = dynamic_cast<EventIDReader const *>(GetDependencyPlugin(eventIDPluginName));
    puPlugin = dynamic_cast<PileUpReader const *>(GetDependencyPlugin(puPluginName));
    
    
    // Read requested systematic variation
    systType = JetCorrectorService::SystType::None;
    systDirection = SystService::VarDirection::Undefined;
    
    if (systServiceName != "")
    {
        SystService const *systService =
          dynamic_cast<SystService const *>(GetMaster().GetServiceQuiet(systServiceName));
        
        if (systService)
        {
            std::pair<bool, SystService::VarDirection> s;
            
            if ((s = systService->Test("JEC")).first)
            {
                systType = JetCorrectorService::SystType::JEC;
                systDirection = s.second;
            }
            else if ((s = systService->Test("JER")).first)
            {
                systType = JetCorrectorService::SystType::JER;
                systDirection = s.second;
            }
        }
    }
    
    
    // Read services for jet corrections
    for (auto const &p: {make_pair(&jetCorrForJetsName, &jetCorrForJets),
      make_pair(&jetCorrForMETFullName, &jetCorrForMETFull),
      make_pair(&jetCorrForMETL1Name, &jetCorrForMETL1),
      make_pair(&jetCorrForMETOrigFullName, &jetCorrForMETOrigFull),
      make_pair(&jetCorrForMETOrigL1Name, &jetCorrForMETOrigL1)})
    {
        if (*p.first != "")
            *p.second =
              dynamic_cast<JetCorrectorService const *>(GetMaster().GetService(*p.first));
    }
}


Plugin *JERCJetMETUpdate::Clone() const
{
    return new JERCJetMETUpdate(*this);
}


double JERCJetMETUpdate::GetJetRadius() const
{
    if (not jetmetPlugin)
    {
        std::ostringstream message;
        message << "JERCJetMETUpdate[\"" << GetName() << "\"]::GetJetRadius: This method cannot be " <<
          "executed before a handle to the original JetMETReader has been obtained.";
        throw std::runtime_error(message.str());
    }
    
    return jetmetPlugin->GetJetRadius();
}


void JERCJetMETUpdate::SetJetCorrection(std::string const &jetCorrServiceName)
{
    jetCorrForJetsName = jetCorrServiceName;
}


void JERCJetMETUpdate::SetJetCorrectionForMET(std::string const &fullNew, std::string const &l1New,
  std::string const &fullOrig, std::string const &l1Orig)
{
    // Make sure that at least one full correction is provided. It will be used to choose what
    //jets contribute to MET.
    if (fullNew == "" and fullOrig == "")
    {
        std::ostringstream message;
        message << "JERCJetMETUpdate[\"" << GetName() << "\"]::SetJetCorrectionForMET: At least one "
          "full correction must be provided.";
        throw std::runtime_error(message.str());
    }
    
    jetCorrForMETFullName = fullNew;
    jetCorrForMETOrigFullName = fullOrig;
    
    
    // If new and original L1 correctors are the same, drop them since their effect would cancel
    //out
    if (l1New != l1Orig)
    {
        jetCorrForMETL1Name = l1New;
        jetCorrForMETOrigL1Name = l1Orig;
    }
    else
        jetCorrForMETL1Name = jetCorrForMETOrigL1Name = "";
}


void JERCJetMETUpdate::SetSelection(double minPt_, double maxAbsEta_)
{
    minPt = minPt_;
    maxAbsEta = maxAbsEta_;
}


void JERCJetMETUpdate::UseRawMET(bool set /*= true*/)
{
    useRawMET = set;
}


bool JERCJetMETUpdate::ProcessEvent()
{
    // Update IOV in jet correctors
    auto const run = eventIDPlugin->GetEventID().Run();
    
    for (auto const &corrService: {jetCorrForJets, jetCorrForMETFull, jetCorrForMETL1,
      jetCorrForMETOrigFull, jetCorrForMETOrigL1})
        if (corrService)
            corrService->SelectIOV(run);
    
    
    jets.clear();
    
    
    // Read value of rho
    double const rho = puPlugin->GetRho();
    
    
    // A shift to be applied to MET to account for differences in T1 corrections
    TLorentzVector metShift;
    
    
    // Loop over original collection of jets
    for (auto const &srcJet: jetmetPlugin->GetJets())
    {
        // Copy current jet and recorrect its momentum
        Jet jet(srcJet);
        
        if (jetCorrForJets)
        {
            double const corrFactor = jetCorrForJets->Eval(srcJet, rho, systType, systDirection);
            jet.SetCorrectedP4(srcJet.RawP4() * corrFactor, 1. / corrFactor);
        }
        
        
        // Precompute full correction factors used for T1 MET corrections
        double corrFactorMETFull = 1., corrFactorMETOrigFull = 1.;
        
        if (jetCorrForMETFull)
        {
            // Sometimes some of jet systematic variations are not propagated into MET. Do not
            //attempt to evaluate them if they are have not been specified in the corresponding
            //jet corrector object
            if (jetCorrForMETFull->IsSystEnabled(systType))
                corrFactorMETFull = jetCorrForMETFull->Eval(srcJet, rho, systType, systDirection);
            else
                corrFactorMETFull = jetCorrForMETFull->Eval(srcJet, rho);
        }
        
        if (jetCorrForMETOrigFull)
        {
            if (jetCorrForMETOrigFull->IsSystEnabled(systType))
                corrFactorMETOrigFull =
                  jetCorrForMETOrigFull->Eval(srcJet, rho, systType, systDirection);
            else
                corrFactorMETOrigFull = jetCorrForMETOrigFull->Eval(srcJet, rho);
        }
        
        
        // Determine if the current jet contributes to the T1 correction. Use the target full
        //correction; if it is not available (e.g. user tries to undo the T1 correction), then use
        //the full original correction. Note that at least one of the two full corrections must
        //have been provided.
        double corrPtForT1 = srcJet.RawP4().Pt();
        
        if (jetCorrForMETFull)
            corrPtForT1 *= corrFactorMETFull;
        else if (jetCorrForMETOrigFull)
            corrPtForT1 *= corrFactorMETOrigFull;
        else
        {
            std::ostringstream message;
            message << "JERCJetMETUpdate[\"" << GetName() << "\"]::ProcessEvent: Jet corrections "
              "required to evaluate T1 MET correction are missing.";
            throw std::runtime_error(message.str());
        }
        
        
        // Compute the shift in MET due to T1 corrections. Systematic variations for L1 corrections
        //are ignored.
        if (corrPtForT1 > minPtForT1)
        {
            // Undo applied T1 corrections
            if (jetCorrForMETOrigL1)
                metShift -=
                  srcJet.RawP4() * jetCorrForMETOrigL1->Eval(srcJet, rho);
            
            if (jetCorrForMETOrigFull)
                metShift += srcJet.RawP4() * corrFactorMETOrigFull;
            
            
            // Apply new T1 corrections
            if (jetCorrForMETL1)
                metShift +=
                  srcJet.RawP4() * jetCorrForMETL1->Eval(srcJet, rho);
            
            if (jetCorrForMETFull)
                metShift -= srcJet.RawP4() * corrFactorMETFull;
        }
        
        
        // Store the new jet if it passes the kinematical selection
        if (jet.Pt() > minPt and std::abs(jet.Eta()) < maxAbsEta)
            jets.emplace_back(jet);
    }
    
    
    // Make sure the new collection of jets is ordered in transverse momentum
    std::sort(jets.rbegin(), jets.rend());
    
    
    // Update MET
    TLorentzVector const &startingMET = (useRawMET) ?
      jetmetPlugin->GetRawMET().P4() : jetmetPlugin->GetMET().P4();
    TLorentzVector updatedMET(startingMET + metShift);
    met.SetPtEtaPhiM(updatedMET.Pt(), 0., updatedMET.Phi(), 0.);
    
    
    // Debug information
    #ifdef DEBUG
    std::cout << "\nJetMETUpdate[\"" << GetName() << "\"]: Jets:\n";
    unsigned i = 1;
    
    for (auto const &j: jets)
    {
        std::cout << " Jet #" << i << "\n";
        std::cout << "  Fully corrected momentum (pt, eta, phi, m): " << j.Pt() << ", " <<
          j.Eta() << ", " << j.Phi() << ", " << j.M() << '\n';
        ++i;
    }
    
    std::cout << "\nJetMETUpdate[\"" << GetName() << "\"]: MET:\n";
    std::cout << " Starting MET (pt, phi): " << startingMET.Pt() << ", " << startingMET.Phi() << '\n';
    std::cout << " Fully corrected MET (pt, phi): " << met.Pt() << ", " << met.Phi() << '\n';
    #endif
    
    
    return true;
}
