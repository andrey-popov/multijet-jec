#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <initializer_list>
#include <string>
#include <vector>


class JetMETReader;


/**
 * \class TriggerBin
 * \brief A plugin to determine trigger bin based on pt(j1)
 * 
 * In addition to finding the trigger bin, applies a selection on pt(j1) according to the loosest
 * provided threshold.
 * 
 * This plugin relies on the presence of a JetMETReader with a default name "JetMET".
 */
class TriggerBin: public AnalysisPlugin
{
public:
    /// Creates a new instance with the given name and thresholds in pt(j1)
    TriggerBin(std::string const &name,
      std::initializer_list<double> const &ptJ1Thresholds);
    
    /// A short-cut for the above version with a default name "TriggerBin"
    TriggerBin(std::initializer_list<double> const &ptJ1Thresholds);
    
public:
    /**
     * \brief Saves pointer to the jet reader
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
    
    /**
     * \brief Returns the number of defined trigger bins
     * 
     * The underflow bin in pt(j1) is counted.
     */
    unsigned GetNumTriggerBins() const;
    
    /**
     * \brief Returns trigger bin determined by pt(j1)
     * 
     * Bin 0 includes events that fail the lowest provided threshold. Since such events are always
     * rejected, values returned by this method start from 1.
     */
    unsigned GetTriggerBin() const;
    
private:
    /**
     * \brief Determine trigger bin for the current event and apply selection on the leading jet
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of the plugin that produces jets
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets
    JetMETReader const *jetmetPlugin;
    
    /// Thresholds of pt(j1) that define different trigger bins
    std::vector<double> ptJ1Thresholds;
    
    /**
     * \brief Trigger bin for the current event
     * 
     * Consult documentation for method GetTriggerBin for details.
     */
    unsigned triggerBin;
};
