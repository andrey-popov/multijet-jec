#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>

#include <TMath.h>
#include <TVector2.h>

#include <cmath>


RecoilBuilder::RecoilBuilder(std::string const &name, double minJetPt_):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    minJetPt(minJetPt_),
    minDPhi12(0.), maxDPhi12(std::numeric_limits<double>::infinity())
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


void RecoilBuilder::SetDPhi12Selection(double minimum, double maximum)
{
    minDPhi12 = minimum;
    maxDPhi12 = maximum;
}


bool RecoilBuilder::ProcessEvent()
{
    // Make sure there is at least a couple of jets
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() < 2)
        return false;
    
    leadingJet = &jets[0];
    
    
    // Reconstruct the recoil. It is built from all jets that pass the kinematic selection,
    //excluding the leading one
    p4Recoil.SetPxPyPzE(0., 0., 0., 0.);
    recoilJets.clear();
    
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        auto const &j = jets[i];
        
        if (j.Pt() < minJetPt)
            break;
        
        p4Recoil += j.P4();
        recoilJets.emplace_back(j);
    }
    
    
    // Apply selection on Delta(phi)
    double const dPhi12 = std::abs(TVector2::Phi_mpi_pi(jets[0].Phi() - jets[1].Phi()));
    
    return (dPhi12 > minDPhi12 and dPhi12 < maxDPhi12);
}
