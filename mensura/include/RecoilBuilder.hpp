#pragma once

#include <mensura/core/AnalysisPlugin.hpp>
#include <mensura/core/PhysicsObjects.hpp>

#include <TLorentzVector.h>

#include <functional>
#include <limits>
#include <vector>


class JetMETReader;


/**
 * \class RecoilBuilder
 * \brief Reconstructs the recoil
 * 
 * The recoil is reconstructed from the sum of non-leading jets down to a specified pt threshold.
 * Events with less than two jets (of any pt) are rejected.
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
    
    /// Returns jet pt threshold used in definition of the recoil
    double GetJetPtThreshold() const;
    
    /// Returns four-momentum of the leading jet
    TLorentzVector const &GetP4LeadingJet() const;
    
    /// Returns four-momentum of the reconstructed recoil
    TLorentzVector const &GetP4Recoil() const;
    
    /// Returns collection of jets included in the recoil
    std::vector<std::reference_wrapper<Jet const>> const &GetRecoilJets() const;

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
    
    /// Jets included in the recoil
    std::vector<std::reference_wrapper<Jet const>> recoilJets;
};
