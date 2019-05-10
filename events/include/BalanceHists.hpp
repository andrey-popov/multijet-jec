#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <TH1D.h>
#include <TH2D.h>
#include <TProfile.h>

#include <memory>
#include <string>
#include <vector>


class BalanceCalc;
class JetMETReader;
class TFileService;


/**
 * \class BalanceHists
 * \brief Produces histograms needed to recompute balancing in multijet events
 * 
 * Intended to be used with data only. Depends on the presence of a jet reader and a plugin to
 * compute balance observables.
 */
class BalanceHists: public AnalysisPlugin
{
public:
    /**
     * \brief Constructs a plugin with the given name
     * 
     * The pt threshold determines what jets enter histograms. Normally it should be aligned with
     * the threshold used for the type 1 correction of missing pt.
     */
    BalanceHists(std::string const &name, double minPt = 15.);
    
    /// A shortcut for the above version with default name "BalanceHists"
    BalanceHists(double minPt = 15.);
    
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
    
    /// Name of a plugin that computes balance observables
    std::string balanceCalcName;
    
    /// Non-owning pointer to a plugin that computes balance observables
    BalanceCalc const *balanceCalc;
    
    /// Name for the output directory
    std::string outDirectoryName;
    
    /// Binning in ptlead
    std::vector<double> ptLeadBinning;
    
    /// Binning in pt of jets in the recoil
    std::vector<double> ptJetBinning;
    
    /// Minimal pt to select jets
    double minPt;
    
    /// Histogram of ptlead
    TH1D *histPtLead;
    
    /// Profiles of ptlead, pt balance, and MPF versus ptlead
    TProfile *profPtLead, *profPtBal, *profMPF;
    
    /// Histogram of ptlead and pt of other jets in the event
    TH2D *histPtJet;
    
    /// Sum of pt(j) * cos(gamma) in bins of ptlead and pt of other jets in the event
    TH2D *histPtJetSumProj;
    
    /// As histPtJetSumProj, but divided by ptlead
    TH2D *histRelPtJetSumProj;
};
