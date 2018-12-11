#pragma once

#include <mensura/core/JetMETReader.hpp>

#include <mensura/core/SystService.hpp>
#include <mensura/extensions/JetCorrectorService.hpp>


class EventIDReader;
class PileUpReader;


/**
 * \class JERCJetMETUpdate
 * \brief A plugin that applies energy corrections to jets and propagates them into MET
 * 
 * This plugin reads jets and MET provided by a JetMETReader with a default name "OrigJetMET" and
 * reapplies jet corrections, propagating them also into MET. Type 1 correction is applied to MET,
 * which is computed starting from the raw MET. Note that this correction can only be evaluated
 * properly if jets down to a sufficiently low pt have been stored.
 * 
 * It is possible to apply a version of the type 1 correction with a smooth threshold. In this case
 * the contribution of each jet near the threshold is included with a weight that changes smoothly
 * from 0 to 1. See documentation for method SetT1Threshold for details.
 * 
 * If a SystService with a non-trivial name is provided (by default, the plugin looks for a service
 * with name "Systematics"), plugin checks the requested systematics and applies variations in JEC
 * or JER as needed. However, systematic variations are never applied to jets with L1 corrections
 * that are used in the type 1 MET correction.
 */
class JERCJetMETUpdate: public JetMETReader
{
public:
    /**
     * \brief Constructor from jet correction services
     * 
     * \param name  Name for the plugin.
     * \param jetCorrFullName  Name of JetCorrectorService that provides the full jet correction.
     * \param jetCorrL1Name  Name of JetCorrectorService that provides L1 jet correction used in
     *     the type 1 MET correction.
     */
    JERCJetMETUpdate(std::string const &name, std::string const &jetCorrFullName,
      std::string const &jetCorrL1Name);
    
    /// Shortcut for constructor with default name "JetMET"
    JERCJetMETUpdate(std::string const &jetCorrFullName, std::string const &jetCorrL1Name);
    
public:
    /**
     * \brief Saves pointeres to all dependencies
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &) override;
    
    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual JERCJetMETUpdate *Clone() const override;
    
    /**
     * \brief Returns jet radius from the source JetMETReader
     * 
     * Implemented from JetMETReader.
     */
    virtual double GetJetRadius() const override;
    
    /// Specifies desired selection on jets
    void SetSelection(double minPt, double maxAbsEta);
    
    /**
     * \brief Sets the pt threshold used in the (smoothed) type 1 correction
     * 
     * Only jets with pt > thresholdStart contribute to the type 1 MET correction. If the second
     * argument is given, a smooth threshold is used. In that case jets with pt from thresholdStart
     * to thresholdEnd contribute with a weight that changes smoothly from 0 to 1 over the given
     * range. Jets with pt > thresholdEnd contribute with weight 1.
     */
    void SetT1Threshold(double thresholdStart, double thresholdEnd = 0.);
    
private:
    /**
     * \brief Reads jets and MET from the source JetMETReader and recorrects them
     * 
     * Reimplemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
    /**
     * \brief Computes jet weight for the computation of smoothed type 1 correction
     * 
     * \param pt  Transverse momentum of the jet.
     * \return  Weight in the range from 0. to 1.
     */
    double WeightJet(double pt) const;
    
private:
    /// Non-owning pointer to and name of a plugin that reads jets and MET
    JetMETReader const *jetmetPlugin;
    std::string jetmetPluginName;
    
    /// Non-owning pointer to a plugin that reports event ID
    EventIDReader const *eventIDPlugin;
    std::string eventIDPluginName;
    
    /// Non-owning pointer to and name of a plugin that reads information about pile-up
    PileUpReader const *puPlugin;
    std::string puPluginName;
    
    /// Name of a service that reports requested systematics
    std::string systServiceName;
    
    /// Non-owning pointer to and name of a service to recorrect jets
    JetCorrectorService const *jetCorrFull;
    std::string jetCorrFullName;
    
    /// Non-owning pointer to and name of service to recorrect jets up to L1 for T1 MET
    JetCorrectorService const *jetCorrL1;
    std::string jetCorrL1Name;
    
    /// Minimal allowed transverse momentum
    double minPt;
    
    /// Maximal allowed absolute value of pseudorapidity
    double maxAbsEta;
    
    /// Minimal transverse momentum for jets to be considered in T1 MET correction
    double minPtForT1;
    
    /// (Absolute) length of the turn-on part of the threshold in T1 MET correction
    double turnOnT1;
    
    /// Type of requested systematical variation
    JetCorrectorService::SystType systType;
    
    /// Requested direction of a systematical variation
    SystService::VarDirection systDirection;
};
