#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <TTree.h>


class JetMETReader;
class TFileService;


/**
 * \class BasicJetVars
 * \brief Produces tuples with basic variables describing jets
 * 
 * Depends on the presence of a jet reader.
 */
class BasicJetVars: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name
    BasicJetVars(std::string const name = "BasicJetVars");
    
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
    
    /// Name of a plugin that produces jets and MET
    std::string jetmetPluginName;
    
    /// Non-owning pointer to a plugin that produces jets and MET
    JetMETReader const *jetmetPlugin;
    
    /// Flag indicating whether current dataset is data or simulation
    bool isMC;
    
    /// Non-owning pointer to output tree
    TTree *tree;
    
    // Output buffers
    Float_t bfPtJ1, bfPtJ2;
    Float_t bfEtaJ1, bfEtaJ2;
    Float_t bfHt;
    Float_t bfWeightDataset;
};
