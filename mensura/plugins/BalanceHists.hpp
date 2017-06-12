#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <TH1D.h>
#include <TH2D.h>
#include <TProfile.h>

#include <memory>
#include <string>
#include <vector>


class JetMETReader;
class RecoilBuilder;
class TFileService;


/**
 * \class BalanceHists
 * \brief Produces histograms needed to recompute balancing in multijet events
 * 
 * Intended to be used with data only. Depends on the presence of a jet reader and a recoil
 * builder.
 */
class BalanceHists: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name
    BalanceHists(std::string const name = "BalanceHists");
    
public:
    /**
     * \brief Saves pointers to required plugins and services and creates histograms
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
    
    /// Sets binning in ptlead
    void SetBinningPtLead(std::vector<double> const &binning);
    
    /// Sets binning in pt of jets in the recoil
    void SetBinningPtJetRecoil(std::vector<double> const &binning);
    
    /**
     * \brief Specifies name for the output in-file directory
     * 
     * By default the name of the plugin is used.
     */
    void SetDirectoryName(std::string const &name);
    
    /// Specifies name of the recoil builder
    void SetRecoilBuilderName(std::string const &name);
    
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
    
    /// Name of a plugin that reconstructs recoil
    std::string recoilBuilderName;
    
    /// Non-owning pointer to a plugin that reconstruct recoil
    RecoilBuilder const *recoilBuilder;
    
    /// Name for the output directory
    std::string outDirectoryName;
    
    /// Binning in ptlead
    std::vector<double> ptLeadBinning;
    
    /// Binning in pt of jets in the recoil
    std::vector<double> ptJetBinning;
    
    /// Histogram of ptlead
    TH1D *histPtLead;
    
    /// Profiles of ptlead, MJB, and MPF versus ptlead
    TProfile *profPtLead, *profMJB, *profMPF;
    
    /**
     * \brief 2D histograms of ptlead and pt of jets in the recoil
     * 
     * They are filled with no weights, weighted with cos(gamma) and pt(j) * cos(gamma)
     * respectively.
     */
    TH2D *histJetPt, *histJetPtProj, *histJetPtSumPtProj;
    
    /**
     * \brief Profile of effective pt in the recoil
     * 
     * Computed for a correction of the from 1 + a * ln(pt / ptmin) + a * (ptmin / pt - 1) with
     * ptmin = 15 GeV.
     */
    TProfile *profPtEff;
};
