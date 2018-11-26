#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <limits>


class BalanceCalc;
class JetMETReader;


/**
 * \class BalanceFilter
 * \brief Rejects strongly imbalanced events
 * 
 * This filter is intended to reject extreme tails in the distribution of pt balance observable.
 * 
 * Depends on the presence of a jet reader and a plugin to compute balance observables.
 */
class BalanceFilter: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name and allowed range for pt balance
    BalanceFilter(std::string const &name, double minPtBal,
      double maxPtBal = std::numeric_limits<double>::infinity());
    
    /// A shortcut for the constructor with a default name "BalanceFilter"
    BalanceFilter(double minPtBal, double maxPtBal = std::numeric_limits<double>::infinity());
    
public:
    /**
     * \brief Saves pointer to recoil builder
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
     * \brief Sets minimal pt of the leading jet for the filtering to be applied
     * 
     * If pt of the leading jet is not greater than the given threshold, no filtering on the
     * pt balance will be applied.
     */
    void SetMinPtLead(double minPtLead);
    
private:
    /**
     * \brief Computes variables and fills the output tree
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Allowed range for the pt balance
    double minPtBal, maxPtBal;
    
    /// Minimal pt of the leading jet for the filtering to be enabled
    double minPtLead;
    
    /// Name of a plugin that produces jets and MET
    std::string jetmetPluginName;
    
    /// Non-owning pointer to a plugin that produces jets and MET
    JetMETReader const *jetmetPlugin;
    
    /// Name of a plugin that computes balance observables
    std::string balanceCalcName;
    
    /// Non-owning pointer to a plugin that computes balance observables
    BalanceCalc const *balanceCalc;
};
