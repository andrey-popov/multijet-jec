#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <initializer_list>
#include <limits>
#include <string>


class JetMETReader;


/**
 * \class FirstJetFilter
 * \brief A plugin to perform kinematic selection on the leading jet
 * 
 * This plugin relies on the presence of a JetMETReader with a default name "JetMET".
 */
class FirstJetFilter: public AnalysisPlugin
{
public:
    /// Creates a new instance with the given selection
    FirstJetFilter(std::string const &name, double minPt,
      double maxAbsEta = std::numeric_limits<double>::infinity());
    
    /// A short-cut for the above version with a default name "FirstJetFilter"
    FirstJetFilter(double minPt, double maxAbsEta = std::numeric_limits<double>::infinity());
    
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
    
private:
    /**
     * \brief Performs selection on the leading jet
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of the plugin that produces jets
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets
    JetMETReader const *jetmetPlugin;
    
    /// Requested selection on pt and |eta|
    double minPt, maxAbsEta;
};
