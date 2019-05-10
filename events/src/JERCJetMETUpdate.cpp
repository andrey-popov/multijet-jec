#include <JERCJetMETUpdate.hpp>

#include <mensura/EventIDReader.hpp>
#include <mensura/PileUpReader.hpp>
#include <mensura/Processor.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>


JERCJetMETUpdate::JERCJetMETUpdate(std::string const &name, std::string const &jetCorrFullName_,
  std::string const &jetCorrL1Name_):
    JetMETReader(name),
    jetmetPlugin(nullptr), jetmetPluginName("OrigJetMET"),
    eventIDPlugin(nullptr), eventIDPluginName("InputData"),
    puPlugin(nullptr), puPluginName("PileUp"),
    systServiceName("Systematics"),
    jetCorrFull(nullptr), jetCorrFullName(jetCorrFullName_),
    jetCorrL1(nullptr), jetCorrL1Name(jetCorrL1Name_),
    minPt(0.), maxAbsEta(std::numeric_limits<double>::infinity()), minPtForT1(15.), turnOnT1(0.)
{}


JERCJetMETUpdate::JERCJetMETUpdate(std::string const &jetCorrFullName,
  std::string const &jetCorrL1Name):
    JERCJetMETUpdate("JetMET", jetCorrFullName, jetCorrL1Name)
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
    jetCorrFull = dynamic_cast<JetCorrectorService const *>(
      GetMaster().GetService(jetCorrFullName));
    jetCorrL1 = dynamic_cast<JetCorrectorService const *>(GetMaster().GetService(jetCorrL1Name));
}


JERCJetMETUpdate *JERCJetMETUpdate::Clone() const
{
    return new JERCJetMETUpdate(*this);
}


double JERCJetMETUpdate::GetJetRadius() const
{
    if (not jetmetPlugin)
    {
        std::ostringstream message;
        message << "JERCJetMETUpdate[\"" << GetName() << "\"]::GetJetRadius: This method cannot "
          "be executed before a handle to the original JetMETReader has been obtained.";
        throw std::runtime_error(message.str());
    }
    
    return jetmetPlugin->GetJetRadius();
}


void JERCJetMETUpdate::SetSelection(double minPt_, double maxAbsEta_)
{
    minPt = minPt_;
    maxAbsEta = maxAbsEta_;
}


void JERCJetMETUpdate::SetT1Threshold(double thresholdStart, double thresholdEnd)
{
    minPtForT1 = thresholdStart;
    
    if (thresholdEnd <= 0. or thresholdStart == thresholdEnd)
        turnOnT1 = 0.;
    else
    {
        if (thresholdEnd < thresholdStart)
        {
            std::ostringstream message;
            message << "JERCJetMETUpdate[\"" << GetName() << "\"]::SetT1Threshold: Wrong ordering "
              "in range (" << thresholdStart << ", " << thresholdEnd << ").";
            throw std::runtime_error(message.str());
        }
        
        turnOnT1 = thresholdEnd - thresholdStart;
    }
}


bool JERCJetMETUpdate::ProcessEvent()
{
    // Update IOV in jet correctors
    auto const run = eventIDPlugin->GetEventID().Run();
    
    for (auto const &corrService: {jetCorrFull, jetCorrL1})
        corrService->SelectIOV(run);
    
    
    jets.clear();
    
    
    // Read value of rho
    double const rho = puPlugin->GetRho();
    
    
    // Loop over original collection of jets
    TLorentzVector updatedMET(jetmetPlugin->GetRawMET().P4());
    
    for (auto const &srcJet: jetmetPlugin->GetJets())
    {
        // Copy current jet and recorrect its momentum
        Jet jet(srcJet);
        
        double const corrFactor = jetCorrFull->Eval(srcJet, rho, systType, systDirection);
        jet.SetCorrectedP4(srcJet.RawP4() * corrFactor, 1. / corrFactor);
        
        
        // Evaluate type 1 correction to MET from the current jet. Systematic variations are not
        // propagated to the L1 correction.
        if (jet.Pt() > minPtForT1)
        {
            double const weight = WeightJet(jet.Pt());
            updatedMET -= (jet.P4() - srcJet.RawP4() * jetCorrL1->Eval(srcJet, rho)) * weight;
        }
        
        
        // Store the new jet if it passes the kinematical selection
        if (jet.Pt() > minPt and std::abs(jet.Eta()) < maxAbsEta)
            jets.emplace_back(jet);
    }
    
    
    // Make sure the new collection of jets is ordered in transverse momentum
    std::sort(jets.rbegin(), jets.rend());
    
    // Update MET
    met.SetPtEtaPhiM(updatedMET.Pt(), 0., updatedMET.Phi(), 0.);
    
    return true;
}


double JERCJetMETUpdate::WeightJet(double pt) const
{
    if (turnOnT1 <= 0.)
    {
        if (pt >= minPtForT1)
            return 1.;
        else
            return 0.;
    }
    
    double const x = (pt - minPtForT1) / turnOnT1;
    
    if (x < 0.)
        return 0.;
    else if (x > 1.)
        return 1.;
    else
        return -2 * std::pow(x, 3) + 3 * std::pow(x, 2);
}
