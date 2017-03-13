#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <TTree.h>

#include <initializer_list>
#include <string>
#include <vector>


class JetMETReader;
class PECTriggerObjectReader;
class TriggerBin;


/**
 * \class LeadJetTriggerFilter
 * \brief Selects events in which the leading jet is matched to a trigger object
 * 
 * The collection of trigger objects used in matching is chosen based on the trigger bin reported
 * by a TriggerBin plugin. If an event does not contain jets, it is rejected.
 */
class LeadJetTriggerFilter: public AnalysisPlugin
{
public:
    /**
     * \brief Constructor
     * 
     * Creates a plugin with the given name that matches jets to trigger objects corresponding to
     * trigger filters with provided names. The names of the filters must be given in an order that
     * correspond to trigger bins.
     */
    LeadJetTriggerFilter(std::string const &name,
      std::initializer_list<std::string> const &triggerFilters = {});
    
    /**
     * \brief Constructor
     * 
     * A short-cut for the above version with default name "TriggerFilter".
     */
    LeadJetTriggerFilter(std::initializer_list<std::string> const &triggerFilters = {});
    
public:
    /// Adds new trigger filter to used in matching
    void AddTriggerFilter(std::string const &filterName);
    
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
    /// Name of a plugin that determines trigger bin
    std::string triggerBinPluginName;
    
    /// Non-owning pointer to the plugin that determines trigger bin
    TriggerBin const *triggerBinPlugin;
    
    /// Name of a plugin that produces jets and MET
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets and MET
    JetMETReader const *jetmetPlugin;
    
    /// Name of a plugin that reads trigger objects
    std::string triggerObjectsPluginName;
    
    /// Non-owning pointer to the plugin that reads trigger objects
    PECTriggerObjectReader const *triggerObjectsPlugin;
    
    /// Names of trigger filters
    std::vector<std::string> triggerFilters;
    
    /**
     * \brief Indices of trigger filters in PECTriggerObjectReader
     * 
     * Indices of this vector are trigger bin indices minus one.
     */
    std::vector<unsigned> triggerFilterIndices;
    
    /// Maximal dR distance (squared) for matching
    double maxDR2;
};
