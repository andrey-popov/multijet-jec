#include <TriggerBin.hpp>

#include <mensura/core/JetMETReader.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>


TriggerBin::TriggerBin(std::string const &name,
  std::initializer_list<double> const &ptJ1Thresholds_):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    ptJ1Thresholds(ptJ1Thresholds_)
{
    // Make sure the binning in pt(j1) is not empty and sorted
    if (ptJ1Thresholds.size() == 0)
    {
        std::ostringstream message;
        message << "TriggerBin::TriggerBin[\"" << GetName() <<
          "\"]: Set of thresholds cannot be empty.";
        throw std::logic_error(message.str());
    }
    
    std::sort(ptJ1Thresholds.begin(), ptJ1Thresholds.end());
}


TriggerBin::TriggerBin(std::initializer_list<double> const &ptJ1Thresholds):
    TriggerBin("TriggerBin", ptJ1Thresholds)
{}


void TriggerBin::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


Plugin *TriggerBin::Clone() const
{
    return new TriggerBin(*this);
}


unsigned TriggerBin::GetNumTriggerBins() const
{
    return ptJ1Thresholds.size() + 1;
}


unsigned TriggerBin::GetTriggerBin() const
{
    return triggerBin;
}


bool TriggerBin::ProcessEvent()
{
    // Find pt of the leading jet. If the event contains no jets, it is rejected.
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() == 0)
        return false;
    
    double const ptJ1 = jets[0].Pt();
    
    
    // Reject event if its leading jet is too soft
    if (ptJ1 < ptJ1Thresholds.front())
        return false;
    
    
    // Determine in which trigger bin the current event falls
    triggerBin = std::distance(ptJ1Thresholds.begin(),
      std::lower_bound(ptJ1Thresholds.begin(), ptJ1Thresholds.end(), ptJ1));
    
    
    // The event is accepted if the workflow reaches this point
    return true;
}
