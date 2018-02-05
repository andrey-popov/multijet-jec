#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <initializer_list>
#include <limits>
#include <string>


class GenJetMETReader;
class JetMETReader;


/**
 * \class GenMatchFilter
 * \brief Selects events in which leading reconstructed jet is matched to a generator-level jet.
 * 
 * The matching is done based on maximal dR distance and minimal ratio of pt of the generator-level
 * jet to the leading reconstructed jet.
 * 
 * This plugin relies on the presence of a JetMETReader with a default name "JetMET" and a
 * GenJetMETReader with a default name "GenJetMET".
 */
class GenMatchFilter: public AnalysisPlugin
{
public:
    /// Creates a new instance with the given matching parameters
    GenMatchFilter(std::string const &name, double maxDR, double minRelPt);
    
    /// A short-cut for the above version with a default name "GenMatchFilter"
    GenMatchFilter(double maxDR, double minRelPt);
    
public:
    /**
     * \brief Saves pointer to the jet readers
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
     * \brief Performs selection based on matching for the leading jet
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of the plugin that produces jets
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets
    JetMETReader const *jetmetPlugin;
    
    /// Name of a plugin that produces generator-level jets
    std::string genJetPluginName;
    
    /// Non-owning pointer to the plugin that produces generator-level jets
    GenJetMETReader const *genJetPlugin;
    
    /// Parameters for matching
    double maxDR2, minRelPt;
};
