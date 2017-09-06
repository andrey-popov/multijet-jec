#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <string>


class JetMETReader;
class PECTriggerObjectReader;


/**
 * \class LeadJetTriggerFilter
 * \brief Selects events in which the leading jet is matched to a trigger object and its pt lies in
 * an allowed range
 * 
 * The plugin is configured using a JSON file with the following example structure:
 *   {
 *     "PFJet200": {
 *       "filter": "hltSinglePFJet200",
 *       "corrPtRange": [250.0, 320.0],
 *       "uncorrPtRange": [220.0, 360.0]
 *     },
 *     ...
 *   }
 * For each trigger the corresponding trigger name is provided along with two pt ranges. The ranges
 * are supposed to be chosen based on whether jets are fully corrected.
 */
class LeadJetTriggerFilter: public AnalysisPlugin
{
public:
    /**
     * \brief Constructor
     * 
     * Creates a plugin with the given name that applies selection specified for the given trigger.
     * The selection is described in a JSON configuration file, whose structure is explained in the
     * documentation for this class. The boolean flag uncorrPt determines which of the two pt
     * ranges are to be used in the event selection.
     */
    LeadJetTriggerFilter(std::string const &name, std::string const &triggerName,
      std::string const &configFileName, bool uncorrPt = true);
    
    /**
     * \brief Constructor
     * 
     * A shortcut for the above version with default name "TriggerFilter".
     */
    LeadJetTriggerFilter(std::string const &triggerName, std::string const &configFileName,
      bool uncorrPt = true);
    
public:
    /**
     * \brief Saves pointers to required plugins and services and sets up output tree
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &) override;
    
    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual Plugin *Clone() const override;
    
private:
    /**
     * \brief Computes variables and fills the output tree
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of a plugin that produces jets and MET
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets and MET
    JetMETReader const *jetmetPlugin;
    
    /// Name of a plugin that reads trigger objects
    std::string triggerObjectsPluginName;
    
    /// Non-owning pointer to the plugin that reads trigger objects
    PECTriggerObjectReader const *triggerObjectsPlugin;
    
    /// Name of trigger filter
    std::string triggerFilter;
    
    /// Cached index of trigger filter in PECTriggerObjectReader
    unsigned triggerFilterIndex;
    
    /// Allowed range of pt for the leading jet
    double minLeadPt, maxLeadPt;
    
    /// Maximal dR distance (squared) for matching
    double maxDR2;
};
