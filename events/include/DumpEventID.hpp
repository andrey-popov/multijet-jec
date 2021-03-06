#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <TTree.h>

#include <string>


class EventIDReader;
class TFileService;


/**
 * \class DumpEventID
 * \brief A plugin to save ID of all encountered events in a ROOT file
 */
class DumpEventID: public AnalysisPlugin
{
public:
    /**
     * \brief Constructs a plugin with the given name
     * 
     * This name will also be given to the output tree.
     */
    DumpEventID(std::string const name = "EventID");
    
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
    
    /**
     * \brief Specifies name for the output tree
     * 
     * Can also include name of a directory. By default the name of the plugin is used.
     */
    void SetTreeName(std::string const &name);
    
private:
    /**
     * \brief Copies ID of the current event to the output tree
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of a plugin that reports event ID
    std::string eventIDPluginName;
    
    /// Non-owning pointer to a plugin that reports event ID
    EventIDReader const *eventIDPlugin;
    
    /// Name of TFileService
    std::string fileServiceName;
    
    /// Non-owning pointer to TFileService
    TFileService const *fileService;
    
    /// Name of the output tree and in-file directory
    std::string treeName, directoryName;
    
    /// Non-owning pointer to the output tree
    TTree *tree;
    
    // Output buffers
    ULong64_t bfRun, bfLumiBlock, bfEvent, bfBunchCrossing;
};
