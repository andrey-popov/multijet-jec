#include <GenMatchFilter.hpp>

#include <mensura/core/GenJetMETReader.hpp>
#include <mensura/core/JetMETReader.hpp>

#include <TVector2.h>

#include <cmath>


GenMatchFilter::GenMatchFilter(std::string const &name, double maxDR, double minRelPt_):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    genJetPluginName("GenJetMET"), genJetPlugin(nullptr),
    maxDR2(std::pow(maxDR, 2)), minRelPt(minRelPt_)
{}


GenMatchFilter::GenMatchFilter(double maxDR, double minRelPt):
    GenMatchFilter("GenMatchFilter", maxDR, minRelPt)
{}


void GenMatchFilter::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    genJetPlugin = dynamic_cast<GenJetMETReader const *>(GetDependencyPlugin(genJetPluginName));
}


Plugin *GenMatchFilter::Clone() const
{
    return new GenMatchFilter(*this);
}


bool GenMatchFilter::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() == 0)
        return false;
    
    
    for (auto const &genJet: genJetPlugin->GetJets())
    {
        if (genJet.Pt() < minRelPt * jets[0].Pt())
        {
            // Generator-level jets are sorted in pt. If the current jet is too soft, there is no
            //point in checking remaining ones.
            return false;
        }
        
        double const dR2 = std::pow(jets[0].Eta() - genJet.Eta(), 2) +
          std::pow(TVector2::Phi_mpi_pi(jets[0].Phi() - genJet.Phi()), 2);
        
        if (dR2 < maxDR2)
            return true;
    }
    
    
    // If the workflow reaches this point, the matching has not succeeded
    return false;
}
