#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <initializer_list>
#include <limits>
#include <string>


class JetMETReader;


/**
 * \class JetIDFilter
 * \brief A plugin to perform selection on jet ID
 * 
 * An event is rejected if at least one jet with pt above a given threshold fails the ID selection.
 * This plugin relies on the presence of a JetMETReader with a default name "JetMET", which must
 * have been configured to produce jets with their ID stored as UserInt with label "ID".
 */
class JetIDFilter: public AnalysisPlugin
{
public:
    /// Creates a new instance with the given pt threshold
    JetIDFilter(std::string const &name, double minPt);
    
    /// A short-cut for the above version with a default name "JetIDFilter"
    JetIDFilter(double minPt);
    
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
    
    /// Requested selection on pt
    double minPt;
};
