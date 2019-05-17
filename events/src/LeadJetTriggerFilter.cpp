#include <LeadJetTriggerFilter.hpp>

#include <mensura/Config.hpp>
#include <mensura/JetMETReader.hpp>
#include <mensura/Processor.hpp>

#include <mensura/PECReader/PECTriggerObjectReader.hpp>

#include <TVector2.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>


LeadJetTriggerFilter::LeadJetTriggerFilter(std::string const &name, std::string const &triggerName,
  std::string const &configFileName, bool useMargin):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    triggerObjectsPluginName("TriggerObjects"), triggerObjectsPlugin(nullptr),
    maxDR2(0.3 * 0.3)
{
    Config config(configFileName);
    auto const &root = config.Get();
    
    
    // Extract information about the requested trigger
    if (not root.isObject())
    {
        // The top-level entity is not a dictionary
        std::ostringstream message;
        message << "LeadJetTriggerFilter[\"" << GetName() << "\"]::LeadJetTriggerFilter: " <<
          "Top-level structure in the data file must be a dictionary. This is not true for " <<
          "file " << config.FilePath() << ".";
        throw std::runtime_error(message.str());
    }
    
    if (not root.isMember(triggerName))
    {
        std::ostringstream message;
        message << "LeadJetTriggerFilter[\"" << GetName() << "\"]::LeadJetTriggerFilter: " <<
          "File " << config.FilePath() << " does not contain entry for trigger \"" <<
          triggerName << "\".";
        throw std::runtime_error(message.str());
    }
    
    auto const &triggerInfo = root[triggerName];
    std::string const ptRangeLabel(useMargin ? "ptRangeMargined" : "ptRange");
    
    if (not triggerInfo.isMember("filter") or not triggerInfo.isMember(ptRangeLabel))
    {
        std::ostringstream message;
        message << "LeadJetTriggerFilter[\"" << GetName() << "\"]::LeadJetTriggerFilter: " <<
          "Entry \"" << triggerName << "\" in file " << config.FilePath() <<
          " does not contain required field \"filter\" or \"" << ptRangeLabel << "\".";
        throw std::runtime_error(message.str());
    }
    
    triggerFilter = triggerInfo["filter"].asString();
    auto const &ptRangeInfo = triggerInfo[ptRangeLabel];
    
    if (not ptRangeInfo.isArray() or ptRangeInfo.size() != 2)
    {
        std::ostringstream message;
        message << "LeadJetTriggerFilter[\"" << GetName() << "\"]::LeadJetTriggerFilter: " <<
          "Field \"" << ptRangeLabel << "\" in entry \"" << triggerName << "\" in file " <<
          config.FilePath() << " is not an array of two elements.";
        throw std::runtime_error(message.str());
    }
    
    minLeadPt = ptRangeInfo[0].asDouble();
    maxLeadPt = ptRangeInfo[1].asDouble();
}


LeadJetTriggerFilter::LeadJetTriggerFilter(std::string const &triggerName,
  std::string const &configFileName, bool useMargin):
    LeadJetTriggerFilter("TriggerFilter", triggerName, configFileName, useMargin)
{}


void LeadJetTriggerFilter::BeginRun(Dataset const &)
{
    // Save pointers to required services and plugins
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    triggerObjectsPlugin =
      dynamic_cast<PECTriggerObjectReader const *>(GetDependencyPlugin(triggerObjectsPluginName));
    
    
    // Cache trigger filter index
    triggerFilterIndex = triggerObjectsPlugin->GetFilterIndex(triggerFilter);
}


Plugin *LeadJetTriggerFilter::Clone() const
{
    return new LeadJetTriggerFilter(*this);
}


bool LeadJetTriggerFilter::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    
    // Filtering on pt of the leading jet
    if (jets.size() == 0)
        return false;
    
    double const ptLead = jets[0].Pt();
    
    if (ptLead < minLeadPt or ptLead >= maxLeadPt)
        return false;
    
    
    // Matching to trigger objects for the selected filter
    auto const &triggerObjects = triggerObjectsPlugin->GetObjects(triggerFilterIndex);
    
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
