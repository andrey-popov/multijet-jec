#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <TTree.h>


class RecoilBuilder;
class TFileService;


/**
 * \class BalanceVars
 * \brief
 * 
 * 
 */
class BalanceVars: public AnalysisPlugin
{
public:
    BalanceVars(std::string const name = "BalanceVars");
    
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
    
    /// Name of a plugin that reconstructs recoil
    std::string recoilBuilderName;
    
    /// Non-owning pointer to a plugin that reconstruct recoil
    RecoilBuilder const *recoilBuilder;
    
    /// Non-owning pointer to output tree
    TTree *tree;
    
    // Output buffers
    Float_t bfPtRecoil;
    Float_t bfPtJ1, bfEtaJ1;
    Float_t bfA, bfAlpha, bfBeta;
    UShort_t bfTriggerBin;
    Float_t bfMJB;
};
