#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <TLorentzVector.h>
#include <TTree.h>


class BalanceCalc;
class JetMETReader;
class TFileService;


/**
 * \class BalanceVars
 * \brief Produces tuples with variables that describe multijet balancing
 * 
 * Depends on the presence of a jet reader and a plugin to compute balance observables.
 */
class BalanceVars: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name and pt threshold for the recoil
    BalanceVars(std::string const &name, double minPtRecoil);
    
    /// Constructs a plugin with default name "BalanceVars" and given threshold for the recoil
    BalanceVars(double minPtRecoil);
    
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
     * \brief Computes variables and fills the output tree
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Threshold used in the definition of the recoil
    double minPtRecoil;
    
    /// Name of TFileService
    std::string fileServiceName;
    
    /// Non-owning pointer to TFileService
    TFileService const *fileService;
    
    /// Name of a plugin that produces jets and MET
    std::string jetmetPluginName;
    
    /// Non-owning pointer to a plugin that produces jets and MET
    JetMETReader const *jetmetPlugin;
    
    /// Name of a plugin that computes balance observables
    std::string balanceCalcName;
    
    /// Non-owning pointer to a plugin that computes balance observables
    BalanceCalc const *balanceCalc;
    
    /// Name of the output tree and in-file directory
    std::string treeName, directoryName;
    
    /// Non-owning pointer to output tree
    TTree *tree;
    
    // Output buffers
    Float_t bfPtJ1, bfPtJ2, bfPtJ3;
    Float_t bfPtRecoil;
    Float_t bfMET;
    Float_t bfDPhi12;
    Float_t bfPtBal, bfMPF;
};
