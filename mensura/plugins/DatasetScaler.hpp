#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <TRandom3.h>

#include <string>


/**
 * \class DatasetScaler
 * \brief A plugin to filter events for the given target integrated luminosity
 * 
 * Events are rejected randomly to reproduce the total number of events expected for the given
 * integrated luminosity. Rejection factor is computed based on the effective luminosity of a
 * dataset.
 */
class DatasetScaler: public AnalysisPlugin
{
public:
    /// Creates a new instance with the given name and target luminosity
    DatasetScaler(std::string const &name, double targetLumi, unsigned seed = 0);
    
    /// A short-cut for the above version with a default name "DatasetScaler"
    DatasetScaler(double targetLumi, unsigned seed = 0);
    
public:
    /**
     * \brief Computes effective luminosity for the current dataset
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
    
private:
    /**
     * \brief Randomly rejects events
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Random-number generator
    TRandom3 rGen;
    
    /// Target integrated luminosity
    double targetLumi;
    
    /// Ratio between effective luminosity for the current dataset and target luminosity
    double acceptFraction;
};
