#pragma once

#include <mensura/PECReader/PECTriggerFilter.hpp>

#include <initializer_list>
#include <utility>
#include <vector>


class TriggerBin;


/**
 * \class DynamicTriggerFilter
 * \brief A plugin to perform trigger selection with a dynamic choice of the trigger
 * 
 * This plugin depends on a TriggerBin plugin. In each event it identifies the trigger whose
 * decision is to be checked according to the trigger bin reported by that plugin.
 */
class DynamicTriggerFilter: public PECTriggerFilter
{
private:
    /// An auxiliary structure to aggregate information about a trigger
    struct Trigger
    {
        /// Name of the trigger with "HLT_" prefix and version postfix stripped off
        std::string name;
        
        /// Corresponding integrated luminosity
        double intLumi;
        
        /// Buffer into which trigger decisions will be read
        Bool_t buffer;
    };
    
public:
    /**
     * \brief Creates an instance with the given name
     * 
     * The second argument is a collection of triggers corresponding to trigger bins defined in
     * the TriggerBin plugin. Each element of the collection consists of a trigger name (with the
     * "HLT_" prefix and version postfix stripped off) and luminosity collected with this trigger
     * (in units of 1/pb). If "1" is provided in place of the trigger name, this is interpreted
     * as a trivial selection that accepts every event.
     */
    DynamicTriggerFilter(std::string const &name,
      std::initializer_list<std::pair<std::string, double>> triggers);
    
    /// A short-cut for the above version with a default name "TriggerFilter"
    DynamicTriggerFilter(std::initializer_list<std::pair<std::string, double>> triggers);
    
public:
    /**
     * \brief Saves pointers to required plugins, sets up reading of the trigger tree
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &dataset) override;
    
    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual Plugin *Clone() const override;
    
    /**
     * \brief Returns weight of the current event
     * 
     * The weight is unity in case of data and equals integrated luminosity (in 1/pb) for the
     * active trigger in case of simulation.
     * 
     * Reimplemented from PECTriggerFilter.
     */
    virtual double GetWeight() const override;
    
private:
    /**
     * \brief Performs selection on the trigger corresponding to the trigger bin identified by
     * TriggerBin plugin
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of a plugin that determines trigger bin
    std::string triggerBinPluginName;
    
    /// Non-owning pointer to a plugin that determines trigger bin
    TriggerBin const *triggerBinPlugin;
    
    /**
     * \brief Collection of defined triggers
     * 
     * Indices of this vector correspond to values returned by TriggerBin::GetTriggerBin(),
     * minus 1.
     */
    std::vector<Trigger> triggers;
    
    /// Index of the trigger to be checked in the current event
    unsigned requestedTriggerIndex;
    
    /// Flag indicating whether the current dataset is data or simulation
    bool isMC;
};
