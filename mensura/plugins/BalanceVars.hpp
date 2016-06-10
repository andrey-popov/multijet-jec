#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <TTree.h>


class EventWeightPlugin;
class JetMETReader;
class PECTriggerFilter;
class RecoilBuilder;
class TFileService;
class TriggerBin;


/**
 * \class BalanceVars
 * \brief Produces tuples with variables that describe multijet balancing
 * 
 * Depends on the presence of a jet reader, a TriggerBin plugin, a recoil builder, a trigger
 * filter, and a plugin to reweight for pile-up.
 */
class BalanceVars: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name
    BalanceVars(std::string const name = "BalanceVars");
    
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
    
    /// Specifies name of the recoil builder
    void SetRecoilBuilderName(std::string const &name);
    
    /**
     * \brief Specifies name for the output tree
     * 
     * By default the name of the plugin is used.
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
    
    /// Name of a plugin that produces jets and MET
    std::string jetmetPluginName;
    
    /// Non-owning pointer to a plugin that produces jets and MET
    JetMETReader const *jetmetPlugin;
    
    /// Name of a plugin that determines trigger bin
    std::string triggerBinPluginName;
    
    /// Non-owning pointer to a plugin that determines trigger bin
    TriggerBin const *triggerBinPlugin;
    
    /// Name of a plugin that reconstructs recoil
    std::string recoilBuilderName;
    
    /// Non-owning pointer to a plugin that reconstruct recoil
    RecoilBuilder const *recoilBuilder;
    
    /// Name of a trigger filter
    std::string triggerFilterName;
    
    /**
     * \brief Non-owning pointer to a trigger filter
     * 
     * Used only in simulation.
     */
    PECTriggerFilter const *triggerFilter;
    
    /// Name of a plugin that performs reweighting for pile-up
    std::string puReweighterName;
    
    /**
     * \brief Non-owning pointer to a plugin that performs reweighting for pile-up
     * 
     * Used only in simulation.
     */
    EventWeightPlugin const *puReweighter;
    
    /// Name of the output tree
    std::string treeName;
    
    /// Flag indicating whether current dataset is data or simulation
    bool isMC;
    
    /// Common event weight in the current dataset
    double weightDataset;
    
    /// Non-owning pointer to output tree
    TTree *tree;
    
    // Output buffers
    Float_t bfPtRecoil;
    Float_t bfPtJ1, bfEtaJ1;
    Float_t bfA, bfAlpha, bfBeta;
    UShort_t bfTriggerBin;
    Float_t bfMJB, bfMPF;
    Float_t bfCRecoil;
    Float_t bfWeight[3];
};
