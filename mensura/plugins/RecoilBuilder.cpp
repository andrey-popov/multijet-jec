#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>

#include <TMath.h>
#include <TVector2.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>


RecoilBuilder::RecoilBuilder(std::string const &name, double minJetPt_,
  std::initializer_list<double> const &ptRecoilTriggerBins_):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    minJetPt(minJetPt_), ptRecoilTriggerBins(ptRecoilTriggerBins_),
    maxA(std::numeric_limits<double>::infinity()),
    maxAlpha(std::numeric_limits<double>::infinity()),
    minBeta(-std::numeric_limits<double>::infinity())
{
    // Make sure the binning in pt(recoil) is not empty and sorted
    if (ptRecoilTriggerBins.size() == 0)
    {
        std::ostringstream message;
        message << "RecoilBuilder::RecoilBuilder[\"" << GetName() <<
          "\"]: Set of bins in pt(recoil) cannot be empty.";
        throw std::logic_error(message.str());
    }
    
    std::sort(ptRecoilTriggerBins.begin(), ptRecoilTriggerBins.end());
}


RecoilBuilder::RecoilBuilder(double minJetPt,
  std::initializer_list<double> const &ptRecoilTriggerBins):
    RecoilBuilder("RecoilBuilder", minJetPt, ptRecoilTriggerBins)
{}


void RecoilBuilder::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


Plugin *RecoilBuilder::Clone() const
{
    return new RecoilBuilder(*this);
}


double RecoilBuilder::GetA() const
{
    return A;
}


double RecoilBuilder::GetAlpha() const
{
    return alpha;
}


double RecoilBuilder::GetBeta() const
{
    return beta;
}


unsigned RecoilBuilder::GetNumTriggerBins() const
{
    return ptRecoilTriggerBins.size() + 1;
}


TLorentzVector const &RecoilBuilder::GetP4LeadingJet() const
{
    return leadingJet->P4();
}


TLorentzVector const &RecoilBuilder::GetP4Recoil() const
{
    return p4Recoil;
}


unsigned RecoilBuilder::GetTriggerBin() const
{
    return triggerBin;
}


void RecoilBuilder::SetBalanceSelection(double maxA_, double maxAlpha_, double minBeta_)
{
    maxA = maxA_;
    maxAlpha = maxAlpha_;
    minBeta = minBeta_;
}


bool RecoilBuilder::ProcessEvent()
{
    // Make sure there is at least a couple of jets
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() < 2)
        return false;
    
    leadingJet = &jets.at(0);
    
    
    // Reconstruct the recoil. It is built from all jets that pass the kinematic selection,
    //excluding the leading one. At the same time compute the beta separation
    p4Recoil.SetPxPyPzE(0., 0., 0., 0.);
    beta = std::numeric_limits<double>::infinity();
    
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        auto const &j = jets.at(i);
        
        if (j.Pt() < minJetPt)
            break;
        
        p4Recoil += j.P4();
        double const betaCurrent = std::abs(TVector2::Phi_mpi_pi(j.Phi() - leadingJet->Phi()));
        
        if (betaCurrent < beta)
            beta = betaCurrent;
    }
    
    
    // Reject events with a soft recoil
    if (p4Recoil.Pt() < ptRecoilTriggerBins.front())
        return false;
    
    
    // Compute remaining balance variables and apply selection on them
    A = jets.at(1).Pt() / p4Recoil.Pt();
    alpha = TMath::Pi() - std::abs(TVector2::Phi_mpi_pi(p4Recoil.Phi() - leadingJet->Phi()));
    
    if (A > maxA or alpha > maxAlpha or beta < minBeta)
        return false;
    
    
    // Determine in which trigger bin the current event falls
    triggerBin = std::distance(ptRecoilTriggerBins.begin(),
      std::lower_bound(ptRecoilTriggerBins.begin(), ptRecoilTriggerBins.end(), p4Recoil.Pt()));
    
    
    // The event is accepted if the workflow reaches this point
    return true;
}
