#pragma once

#include <PhysicsObjects.hpp>

#include <mensura/core/JetMETReader.hpp>

#include <mensura/core/LeptonReader.hpp>
#include <mensura/core/GenJetMETReader.hpp>
#include <mensura/core/SystService.hpp>

#include <mensura/external/JERC/JetResolution.hpp>

#include <functional>
#include <memory>


class PileUpReader;
class PECInputData;


/**
 * \class JERCJetMETReader
 * \brief Provides reconstructed jets and MET
 * 
 * This plugin reads reconstructed jets and MET written into input file by CMSSW plugin
 * BasicJetMET [1], and translates them to standard classes used by the framework. User can specify
 * a kinematic selection to be applied to jets, using method SetSelection. By default, jets are
 * cleaned against tight leptons produced by a LeptonReader with name "Leptons". This behaviour
 * can be configured with method ConfigureLeptonCleaning.
 * [1] https://github.com/IPNL-CMS/multijet-jec/blob/1b9b985fb05496c5c98b67235dfcb99853671f37/CMSSW/plugins/BasicJetMET.h
 * 
 * Jets are corrected by default, but only raw missing pt is provided. The corrected missing pt is
 * set to null.
 * 
 * If the name of a plugin that reads generator-level jets is provided with the help of the method
 * SetGenJetReader, angular matching to them is performed. The maximal allowed angular distance for
 * matching is set to half of the radius parameter of reconstructed jets. User can additionally
 * impose a cut on the difference between pt of the two jets via method SetGenPtMatching.
 */
class JERCJetMETReader: public JetMETReader
{
public:
    /**
     * \brief Creates plugin with the given name
     * 
     * User is encouraged to keep the default name.
     */
    JERCJetMETReader(std::string const name = "JetMET");
    
    /// Copy constructor
    JERCJetMETReader(JERCJetMETReader const &src) noexcept;
    
    /// Default move constructor
    JERCJetMETReader(JERCJetMETReader &&) = default;
    
    /// Assignment operator is deleted
    JERCJetMETReader &operator=(JERCJetMETReader const &) = delete;
    
public:
    /**
     * \brief Sets up reading of a tree containing jets and MET
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
     * \brief Changes parameters of jet-lepton cleaning
     * 
     * The first argument is the name of a LeptonReader, which must precede this plugin in the
     * path. The second argument is the minimal allowed separation between a jet and a lepton in
     * the (eta, phi) metric. Each jets is checked against all tight leptons produced by the
     * LeptonReader, and if the separation from a lepton is less than dR, the jet is dropped.
     * 
     * The cleaning is enabled by default, with the lepton reader called "Leptons" and dR equal to
     * the jet radius. It can be disabled by providing an empty name ("") for the lepton reader.
     */
    void ConfigureLeptonCleaning(std::string const leptonPluginName, double dR);
    
    /// A short-cut for the above method that uses jet radius as the minimal allowed separation
    void ConfigureLeptonCleaning(std::string const leptonPluginName = "Leptons");
    
    /**
     * \brief Returns radius parameter used in the jet clustering algorithm
     * 
     * The radius is hard-coded in the current implementation, but it will be made configurable in
     * future if jets with larger radii are added.
     * 
     * Implemented from JetMETReader.
     */
    virtual double GetJetRadius() const override;
    
    /**
     * \brief Specifies whether jet ID selection should be applied or not
     * 
     * The selection is applied or not depending on the value of the given flag; by default, the
     * plugin is configured to apply it. If the selection is applied, jets that fail loose ID are
     * rejected. Otherwise all jets are written to the output collection, and for each jet a
     * UserInt with label "ID" is added whose value is 1 or 0 depending on whether the jet passes
     * the ID selection or not.
     */
    void SetApplyJetID(bool applyJetID);
    
    /// Specifies name of the plugin that provides generator-level jets
    void SetGenJetReader(std::string const name = "GenJetMET");
    
    /**
     * \brief Add a condition on difference in pt for the matching to generator-level jets
     * 
     * If this method is called, the matching to generator-level jets will be performed based not
     * only on the angular distance but requiring additionally that the difference in pt between
     * the two jets is compatible with the pt resolution in simulation.
     * 
     * The pt resultion is described in file jerFile, which must follow the standard format adopted
     * in the JERC group. The path to the file is resolved using the FileInPath service under a
     * subdirectory "JERC". The factor jerPtFactor is applied to the resolution before the
     * comparison.
     * 
     * If the pt matching has been requested, this plugin will also attempt to read pileup
     * information from the dedicated plugin.
     */
    void SetGenPtMatching(std::string const &jerFile, double jerPtFactor = 3.);
    
    /**
     * \brief Specifies desired selection on jet momentum
     * 
     * Jet pt is corrected using the correction read from the input file. Can be used together with
     * the version with a generic selector.
     */
    void SetSelection(double minPt, double maxAbsEta);
    
    /**
     * \brief Specifies desired generic selection on jets
     * 
     * Provided selector is applied to fully constructed jets. If it returns false, the jet is
     * dropped from the collection. Can be used together with the version with rectangular cut on
     * jet pt and eta.
     */
    void SetSelection(std::function<bool(Jet const &)> jetSelector);
    
private:
    /**
     * \brief Reads jets and MET from the input tree
     * 
     * Reimplemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of a plugin that reads PEC files
    std::string inputDataPluginName;
    
    /// Non-owning pointer to a plugin that reads PEC files
    PECInputData const *inputDataPlugin;
    
    /// Name of the tree containing information about jets and MET
    std::string treeName;
    
    /// Non-owning pointer to buffer to read the branch with jets
    std::vector<jec::Jet> *bfJets;
    
    /// Non-owning pointer to buffer to read missing pt
    jec::MET *bfMET;
    
    /// Minimal allowed transverse momentum
    double minPt;
    
    /// Maximal allowed absolute value of pseudorapidity
    double maxAbsEta;
    
    /// Generic jet selector
    std::function<bool(Jet const &)> jetSelector;
    
    /// Specifies whether selection on jet ID should be applied
    bool applyJetID;
    
    /**
     * \brief Name of the plugin that produces leptons
     * 
     * If name is an empty string, no cleaning against leptons will be performed
     */
    std::string leptonPluginName;
    
    /// Non-owning pointer to a plugin that produces leptons
    LeptonReader const *leptonPlugin;
    
    /**
     * \brief Minimal squared dR distance to leptons
     * 
     * Exploited in jet cleaning.
     */
    double leptonDR2;
    
    /**
     * \brief Name of the plugin that produces generator-level jets
     * 
     * The name can be empty. In this case no matching to generator-level jets is performed.
     */
    std::string genJetPluginName;
    
    /// Non-owning pointer to a plugin that produces generator-level jets
    GenJetMETReader const *genJetPlugin;
    
    /// Name of the plugin that provides information about pileup
    std::string puPluginName;
    
    /**
     * \brief Non-owning pointer to a plugin that produces information about pileup
     * 
     * Only used when jet resolution needs to be accessed and can be null in other cases.
     */
    PileUpReader const *puPlugin;
    
    /**
     * \brief Fully-qualified path to file with jet pt resolution in simulation
     * 
     * This string is empty if the jet matching based on pt has not been requested.
     */
    std::string jerFilePath;
    
    /// Factor used to define the cut for the jet pt matching
    double jerPtFactor;
    
    /**
     * \brief An object that provides jet pt resolution in simulation
     * 
     * Uninitialized if the jet matching based on pt has not been requested.
     */
    std::unique_ptr<JME::JetResolution> jerProvider;
};
