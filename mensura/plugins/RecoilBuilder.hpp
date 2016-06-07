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
 * 
 * The plugin unconditionally rejects events with less than two jets. User can also require a
 * selection on the balancing variables.
 * 
 * This plugin relies on the presence of a JetMETReader with a default name "JetMET".
 */
class RecoilBuilder: public AnalysisPlugin
{
public:
    /**
     * \brief Constructs a new instance with the given name
     * 
     * Recoil will be built from jets that satisfy the provided pt cut.
     */
    RecoilBuilder(std::string const &name, double minJetPt);
    
    /// A short-cut for the above version with a default name "RecoilBuilder"
    RecoilBuilder(double minJetPt);
    
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
    
    /// Returns jet pt threshold used in definition of the recoil
    double GetJetPtThreshold() const;
    
    /// Returns four-momentum of the leading jet
    TLorentzVector const &GetP4LeadingJet() const;
    
    /// Returns four-momentum of the reconstructed recoil
    TLorentzVector const &GetP4Recoil() const;
    
    /// Sets selection on balancing variables
    void SetBalanceSelection(double maxA, double maxAlpha, double minBeta);
    
    /**
     * \brief Sets definition of dynamic pt cuts for jets considered for the beta cut
     * 
     * In computation of the beta separation only those jets are used whose pt is larger than pt of
     * the recoil multiplied by the given fraction. They also must satisfy the standard pt cut for
     * jets included in definition of the recoil. By default this fraction is set to zero, which
     * means that all jets included in the recoil are considered for computation of beta.
     */
    void SetBetaPtFraction(double betaPtFraction);

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
    
    /**
     * 
     */
    double betaPtFraction;
};
