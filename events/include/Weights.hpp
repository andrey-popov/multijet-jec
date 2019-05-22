#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <mensura/Dataset.hpp>
#include <mensura/TFileService.hpp>

#include <TTree.h>


/**
 * \brief Stores event weights
 *
 * This plugin must only be run on simulation.
 */
class Weights: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name
    Weights(std::string const &name);
    
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
    virtual Weights *Clone() const override;
    
    /**
     * \brief Specifies the name for the output tree
     * 
     * Can also include the name of the in-file directory. By default the name of the plugin is used
     * for the tree.
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
    
    /// Name of the output tree and its in-file directory
    std::string treeName, directoryName;
    
    /// Non-owning pointer to output tree
    TTree *tree;
    
    // Output buffers
    Float_t bfWeightSample;
};

