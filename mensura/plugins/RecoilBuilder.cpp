#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>

#include <TMath.h>
#include <TVector2.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>


RecoilBuilder::RecoilBuilder(std::string const &name, double minJetPt_):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    minJetPt(minJetPt_),
    maxA(std::numeric_limits<double>::infinity()),
    maxAlpha(std::numeric_limits<double>::infinity()),
    minBeta(-std::numeric_limits<double>::infinity()),
    betaPtFraction(0.)
{}


RecoilBuilder::RecoilBuilder(double minJetPt):
    RecoilBuilder("RecoilBuilder", minJetPt)
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


double RecoilBuilder::GetJetPtThreshold() const
{
    return minJetPt;
}


TLorentzVector const &RecoilBuilder::GetP4LeadingJet() const
{
    return leadingJet->P4();
}


TLorentzVector const &RecoilBuilder::GetP4Recoil() const
{
    return p4Recoil;
}


std::vector<std::reference_wrapper<Jet const>> const &RecoilBuilder::GetRecoilJets() const
{
    return recoilJets;
}


void RecoilBuilder::SetBalanceSelection(double maxA_, double maxAlpha_, double minBeta_)
{
    maxA = maxA_;
    maxAlpha = maxAlpha_;
    minBeta = minBeta_;
}


void RecoilBuilder::SetBetaPtFraction(double betaPtFraction_)
{
    betaPtFraction = betaPtFraction_;
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
    recoilJets.clear();
    
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        auto const &j = jets.at(i);
        
        if (j.Pt() < minJetPt)
            break;
        
        p4Recoil += j.P4();
        recoilJets.emplace_back(j);
    }
    
    
    // Compute remaining balance variables and apply selection on them
    A = jets.at(1).Pt() / p4Recoil.Pt();
    alpha = TMath::Pi() - std::abs(TVector2::Phi_mpi_pi(p4Recoil.Phi() - leadingJet->Phi()));
    beta = std::numeric_limits<double>::infinity();
    
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        auto const &j = jets.at(i);
        
        if (j.Pt() < minJetPt or j.Pt() < betaPtFraction * p4Recoil.Pt())
            break;
        
        double const betaCurrent = std::abs(TVector2::Phi_mpi_pi(j.Phi() - leadingJet->Phi()));
        
        if (betaCurrent < beta)
            beta = betaCurrent;
    }
    
    if (A > maxA or alpha > maxAlpha or beta < minBeta)
        return false;
    
    
    // The event is accepted if the workflow reaches this point
    return true;
}
