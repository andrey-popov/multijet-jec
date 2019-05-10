#include <FirstJetFilter.hpp>

#include <mensura/JetMETReader.hpp>

#include <cmath>


FirstJetFilter::FirstJetFilter(std::string const &name, double minPt_,
  double maxAbsEta_ /*= std::numeric_limits<double>::infinity()*/):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    minPt(minPt_), maxAbsEta(maxAbsEta_)
{}


FirstJetFilter::FirstJetFilter(double minPt,
  double maxAbsEta /*= std::numeric_limits<double>::infinity()*/):
    FirstJetFilter("FirstJetFilter", minPt, maxAbsEta)
{}


void FirstJetFilter::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


Plugin *FirstJetFilter::Clone() const
{
    return new FirstJetFilter(*this);
}


bool FirstJetFilter::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() == 0)
        return false;
    
    if (jets[0].Pt() < minPt)
        return false;
    
    if (std::abs(jets[0].Eta()) > maxAbsEta)
        return false;
    
    
    // The event is accepted if the workflow reaches this point
    return true;
}
