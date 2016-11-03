#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <mensura/core/EventID.hpp>

#include <istream>
#include <map>
#include <string>
#include <vector>


class EventIDReader;


/**
 * \class RunFilter
 * \brief Filters events based on their run number
 * 
 * The filter relies on the presence of a EventIDReader with a default name "InputData".
 */
class RunFilter: public AnalysisPlugin
{
public:
    /// Supported comparisons
    enum class Mode
    {
        Less,
        LessEq,
        Greater,
        GreaterEq
    };
    
public:
    /// Constructor
    RunFilter(std::string const &name, Mode mode, unsigned long runNumber);
    
    /// A short-cut for the above version with a default name "RunFilter"
    RunFilter(Mode mode, unsigned long runNumber);
    
    /// Default move constructor
    RunFilter(RunFilter &&) = default;
    
    /// Assignment operator is deleted
    RunFilter &operator=(RunFilter const &) = delete;
    
private:
    /// Copy constructor used in method Clone
    RunFilter(RunFilter const &src);
    
public:
    /**
     * \brief Performs initialization for a new dataset
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
    
    /// Changes name of the plugin that provides event ID
    void SetEventIDPluginName(std::string const &name);
    
private:
    /**
     * \brief Checks ID of the current event
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
        
private:
    /// Name of the plugin that provides access to event ID
    std::string eventIDPluginName;
    
    /// Non-owning pointer to the plugin that provides access to event ID
    EventIDReader const *eventIDPlugin;
    
    /// Mode of comparison
    Mode mode;
    
    /// Critical run number
    unsigned long runNumber;
};
