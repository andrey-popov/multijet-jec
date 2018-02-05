#include <JetIDFilter.hpp>

#include <mensura/core/JetMETReader.hpp>

#include <cmath>


JetIDFilter::JetIDFilter(std::string const &name, double minPt_):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    minPt(minPt_)
{}


JetIDFilter::JetIDFilter(double minPt):
    JetIDFilter("JetIDFilter", minPt)
{}


void JetIDFilter::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


Plugin *JetIDFilter::Clone() const
{
    return new JetIDFilter(*this);
}


bool JetIDFilter::ProcessEvent()
{
    for (auto const &jet: jetmetPlugin->GetJets())
    {
        if (jet.Pt() < minPt)
        {
            // Jets are ordered in pt
            break;
        }
        
        
        if (not jet.UserInt("ID"))
            return false;
    }
    
    
    // The event is accepted if the workflow reaches this point
    return true;
}
