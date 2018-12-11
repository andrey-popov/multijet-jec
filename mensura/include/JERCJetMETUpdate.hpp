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
 * reapplies jet corrections, propagating them also into MET.
 * 
 * T1 MET corrections are evaluated approximately. The plugin starts from the provided MET (with
 * outdated corrections applied), undoes T1 corrections using only jets available from the source
 * JetMETReader (thus, they are usually cleaned against leptons and have physics ID applied) that
 * are additionally required to pass a selection on recorrected transverse momentum (by default,
 * pt > 15 GeV), and applies new T1 corrections computed with the same jets. If soft jets are not
 * saved in input files and thus are not available through the source JetMETReader, they will
 * contrubute to T1 MET corrections as they were at the time of computation of the original MET,
 * i.e. with outdated corrections. If stochastic JER corrections have been used in computation of
 * MET, they can only be undone on the average due to their random nature.
 * 
 * Because old T1 corrections are undone, the source MET can have any additive corrections applied.
 * They will be preserved by this plugin.
 * 
 * Alternatively, it is also possible to start computation from the raw MET rather than default
 * one. In this case T1 corrections can be applied directly, but any additional corrections will be
 * lost.
 * 
 * If a SystService with a non-trivial name is provided (by default, the plugin looks for a service
 * with name "Systematics"), plugin checks the requested systematics and applies variations in JEC
 * or JER as needed. However, systematic variations are never applied to jets with L1 corrections
 * that are used in T1 MET corrections.
 */
class JERCJetMETUpdate: public JetMETReader
{
public:
    /**
     * \brief Creates a plugin with the given name
     * 
     * User is encouraged to keep the default name.
     */
    JERCJetMETUpdate(std::string const name = "JetMET");
    
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
    virtual Plugin *Clone() const override;
    
    /**
     * \brief Returns jet radius from the source JetMETReader
     * 
     * Implemented from JetMETReader.
     */
    virtual double GetJetRadius() const override;
    
    /**
     * \brief Sets corrector for jets
     * 
     * Jets read from the source JetMETReader will be corrected using provided JetCorrectorService.
     * If an empty string is given, jet momenta will be copied without modifications.
     */
    void SetJetCorrection(std::string const &jetCorrServiceName);
    
    /**
     * \brief Sets jets correctors to be used to evaluate T1 MET corrections
     * 
     * Arguments are names of instances of JetCorrectorService that evaluate full and L1-only
     * corrections to be applied to compute T1 MET corrections. In addition, corrections used in
     * the original MET can be provided to undo outdated T1 corrections. The "full" corrections
     * to be applied are not necessarily the same as given to method SetJetCorrection.
     * 
     * For any correction level, empty strings can be given as names of the two corresponding
     * correctors. In this case the corresponding corrections are not applied. This is useful, for
     * instance, when L1-only corrections have not changed, and thus there is no need to evaluate
     * them explicitly since their contributions to MET would cancel out.
     * 
     * When computation starts from raw MET (as requested with UseRawMET), it is technically
     * possible to "undo" MET corrections corresponding to provided original corrections, but this
     * does not make sense.
     */
    void SetJetCorrectionForMET(std::string const &fullNew, std::string const &l1New,
      std::string const &fullOrig, std::string const &l1Orig);
    
    /// Specifies desired selection on jets
    void SetSelection(double minPt, double maxAbsEta);
    
    /// Instructs to use raw MET for computation instead of the default one
    void UseRawMET(bool set = true);
    
private:
    /**
     * \brief Reads jets and MET from the source JetMETReader and recorrects them
     * 
     * Reimplemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
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
    
    /**
     * \brief Non-owning pointer to and name of a service to recorrect jets
     * 
     * Might be uninitialized.
     */
    JetCorrectorService const *jetCorrForJets;
    std::string jetCorrForJetsName;
    
    /**
     * \brief Non-owning pointers to and names of services to recorrect jets for T1 MET
     * 
     * Services for a full and L1 only corrections. Might be uninitialized.
     */
    JetCorrectorService const *jetCorrForMETFull, *jetCorrForMETL1;
    std::string jetCorrForMETFullName, jetCorrForMETL1Name;
    
    /**
     * \brief Non-owning pointers to and names of services to undo T1 MET corrections
     * 
     * Services for a full and L1 only corrections as used in construction of T1-corrected MET.
     * Might be uninitialized.
     */
    JetCorrectorService const *jetCorrForMETOrigFull, *jetCorrForMETOrigL1;
    std::string jetCorrForMETOrigFullName, jetCorrForMETOrigL1Name;
    
    /// Minimal allowed transverse momentum
    double minPt;
    
    /// Maximal allowed absolute value of pseudorapidity
    double maxAbsEta;
    
    /// Minimal transverse momentum for jets to be considered in T1 MET corrections
    double minPtForT1;
    
    /// Specifies whether computation should start from raw or default MET
    bool useRawMET;
    
    /// Type of requested systematical variation
    JetCorrectorService::SystType systType;
    
    /// Requested direction of a systematical variation
    SystService::VarDirection systDirection;
};
