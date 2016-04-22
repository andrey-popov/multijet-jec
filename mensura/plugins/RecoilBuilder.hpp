#pragma once

#include <mensura/core/AnalysisPlugin.hpp>
#include <mensura/core/PhysicsObjects.hpp>

#include <TLorentzVector.h>

#include <initializer_list>
#include <vector>


class JetMETReader;


/**
 * \class RecoilBuilder
 * \brief Reconstructs the recoil, computes balancing variables, and applies a selection on them
 * 
 * This plugin reconstructs the recoil from the sum of non-leading jets. It computes several
 * observables that quantify the balance between the recoil and the leading jet in the event.
 * Based on the pt of the recoil, it determines the bin for trigger selection.
 * 
 * The plugin unconditionally rejects events with less than two jets as well as those in which pt
 * of the recoil is smaller than the lowest boundary defining trigger bins. User can also require
 * a selection on the balancing variables.
 * 
 * This plugin relies on the presence of a JetMETReader with a default name "JetMET".
 */
class RecoilBuilder: public AnalysisPlugin
{
public:
    /**
     * \brief Constructs a new instance with the given name
     * 
     * Recoil will be built from jets that satisfy the provided pt cut. Given thresholds in
     * pt of the recoil will be used to define the trigger bin and reject events that do not pass
     * the lowest threshold.
     */
    RecoilBuilder(std::string const &name, double minJetPt,
      std::initializer_list<double> const &ptRecoilTriggerBins);
    
    /// A short-cut for the above version with a default name "RecoilBuilder"
    RecoilBuilder(double minJetPt, std::initializer_list<double> const &ptRecoilTriggerBins);
    
public:
    /**
     * \brief Saves pointer to the jet reader
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
    
    /// Returns fraction A = pt(j2) / pt(recoil)
    double GetA() const;
    
    /// Returns alpha = pi - dPhi(j1, recoil)
    double GetAlpha() const;
    
    /**
     * \brief Returns the beta separation
     * 
     * Defined as minimal dPhi(j1, j) where j runs over all jets included in the recoil.
     */
    double GetBeta() const;
    
    /**
     * \brief Returns the number of defined trigger bins
     * 
     * The underflow bin in pt(recoil) is counted.
     */
    unsigned GetNumTriggerBins() const;
    
    /// Returns four-momentum of the leading jet
    TLorentzVector const &GetP4LeadingJet() const;
    
    /// Returns four-momentum of the reconstructed recoil
    TLorentzVector const &GetP4Recoil() const;
    
    /**
     * \brief Returns trigger bin dictated by pt(recoil)
     * 
     * Bin 0 includes events that fail the lowest provided threshold. Since such events are always
     * rejected, values returned by this method start from 1.
     */
    unsigned GetTriggerBin() const;
    
    /// Sets selection on balancing variables
    void SetBalanceSelection(double maxA, double maxAlpha, double minBeta);

private:
    /**
     * \brief Reconstructs the recoil and performs event selection based on its properties
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of the plugin that produces jets
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets
    JetMETReader const *jetmetPlugin;
    
    /// Selection on pt of jets to be included in the recoil
    double minJetPt;
    
    /// Thresholds of pt(recoil) that define different trigger bins
    std::vector<double> ptRecoilTriggerBins;
    
    /**
     * \brief Trigger bin for the current event
     * 
     * Consult documentation for method GetTriggerBin for details.
     */
    unsigned triggerBin;
    
    /// Momentum of the recoil reconstructed in the current event
    TLorentzVector p4Recoil;
    
    /// Pointer to the leading jet in the current event
    Jet const *leadingJet;
    
    /**
     * \brief Balance variables in the current event
     * 
     * Consult documentation for methods GetA, GetAlpha, GetBeta for their definitions.
     */
    double A, alpha, beta;
    
    /// Cuts on balance variables
    double maxA, maxAlpha, minBeta;
};
