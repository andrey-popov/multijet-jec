#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <string>


class JetMETReader;


/**
 * \class AngularFilter
 * \brief Applies angular cuts
 * 
 * This plugin implements an event selection based on angular variables, which is independent of
 * L3Res corrections. Can apply cuts on the relative azimuthal angles between the two leading jets
 * or the second and the third jets. Angles are normalized to a range [0, pi]. All awailable jets
 * are considered, without any pt threshold. If an event does not contain jets needed to compute an
 * angle and a non-trivial selection for that angle has been specified, the event is rejected.
 * 
 * This plugin relies on the presence of a JetMETReader with a default name "JetMET".
 */
class AngularFilter: public AnalysisPlugin
{
public:
    AngularFilter(std::string const name = "AngularFilter");
    
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
    virtual AngularFilter *Clone() const override;
    
    /**
     * \brief Sets selection on Delta(phi) between two leading jets
     * 
     * The selection is applied to the absolute value of the angle.
     */
    void SetDPhi12Cut(double minimum, double maximum);
    
    /**
     * \brief Sets selection on Delta(phi) between second and third jets
     * 
     * The selection is applied to the absolute value of the angle.
     */
    void SetDPhi23Cut(double minimum, double maximum);
    
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
    
    /// Selection on Delta(phi) between the two leading jets
    double minDPhi12, maxDPhi12;
    
    /// Selection on Delta(phi) between second and third jets
    double minDPhi23, maxDPhi23;
    
    /// Flags specifying whether non-trivial cuts have been set
    bool cutDPhi12Set, cutDPhi23Set;
};
