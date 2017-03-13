#include <LeadJetTriggerFilter.hpp>

#include <TriggerBin.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>

#include <mensura/PECReader/PECTriggerObjectReader.hpp>

#include <TVector2.h>

#include <cmath>
#include <sstream>
#include <stdexcept>


LeadJetTriggerFilter::LeadJetTriggerFilter(std::string const &name,
  std::initializer_list<std::string> const &triggerFilters_ /*= {}*/):
    AnalysisPlugin(name),
    triggerBinPluginName("TriggerBin"), triggerBinPlugin(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    triggerObjectsPluginName("TriggerObjects"), triggerObjectsPlugin(nullptr),
    triggerFilters(triggerFilters_),
    maxDR2(0.3 * 0.3)
{}


LeadJetTriggerFilter::LeadJetTriggerFilter(
  std::initializer_list<std::string> const &triggerFilters_ /*= {}*/):
    LeadJetTriggerFilter("TriggerFilter", triggerFilters_)
{}


void LeadJetTriggerFilter::AddTriggerFilter(std::string const &filterName)
{
    triggerFilters.emplace_back(filterName);
}


void LeadJetTriggerFilter::BeginRun(Dataset const &)
{
    // Save pointers to required services and plugins
    triggerBinPlugin = dynamic_cast<TriggerBin const *>(GetDependencyPlugin(triggerBinPluginName));
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    triggerObjectsPlugin =
      dynamic_cast<PECTriggerObjectReader const *>(GetDependencyPlugin(triggerObjectsPluginName));
    
    
    // Sanity check
    if (triggerFilters.size() != triggerBinPlugin->GetNumTriggerBins() - 1)
    {
        std::ostringstream message;
        message << "LeadJetTriggerFilter[\"" << GetName() << "\"]::BeginRun: "
          "Number of provided filters (" << triggerFilters.size() <<
          ") does not match the number of trigger bins (" <<
          triggerBinPlugin->GetNumTriggerBins() << ", including underflow).";
        throw std::runtime_error(message.str());
    }
    
    
    // Cache trigger filter indices
    triggerFilterIndices.clear();
    
    for (auto const &filterName: triggerFilters)
        triggerFilterIndices.push_back(triggerObjectsPlugin->GetFilterIndex(filterName));
}


Plugin *LeadJetTriggerFilter::Clone() const
{
    return new LeadJetTriggerFilter(*this);
}


bool LeadJetTriggerFilter::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() == 0)
        return false;
    
    
    auto const &triggerObjects = triggerObjectsPlugin->GetObjects(
      triggerFilterIndices[triggerBinPlugin->GetTriggerBin() - 1]);
    
    for (auto const &triggerObject: triggerObjects)
    {
        double const dR2 = std::pow(jets[0].Eta() - triggerObject.Eta(), 2) +
          std::pow(TVector2::Phi_mpi_pi(jets[0].Phi() - triggerObject.Phi()), 2);
        
        if (dR2 < maxDR2)
        {
            // The jet is matched to a trigger object
            return true;
        }
    }
    
    
    // If workflow reaches this point, no matching trigger object has been found
    return false;
}
