#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <TTree.h>


class PileUpReader;
class TFileService;


/**
 * \class PileUpVars
 * \brief Produces tuples with information about pile-up
 * 
 * Depends on the presence of a pile-up reader.
 */
class PileUpVars: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name
    PileUpVars(std::string const name = "PileUpVars");
    
public:
    /**
     * \brief Saves pointers to required plugins and services and sets up output tree
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
     * \brief Specifies name for the output tree
     * 
     * Can also include name of a directory. By default the name of the plugin is used.
     */
    void SetTreeName(std::string const &name);
    
private:
    /**
     * \brief Computes variables and fills the output tree
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of TFileService
    std::string fileServiceName;
    
    /// Non-owning pointer to TFileService
    TFileService const *fileService;
    
    /// Name of a plugin that reads information about pile-up
    std::string puPluginName;
    
    /// Non-owning pointer to a plugin that reads information about pile-up
    PileUpReader const *puPlugin;
    
    /// Name of the output tree and in-file directory
    std::string treeName, directoryName;
    
    /// Flag indicating whether current dataset is data or simulation
    bool isMC;
    
    /// Non-owning pointer to output tree
    TTree *tree;
    
    // Output buffers
    UShort_t bfNumPV;
    Float_t bfRho, bfLambdaPU;
};
