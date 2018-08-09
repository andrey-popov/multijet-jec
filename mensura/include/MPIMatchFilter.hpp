#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <initializer_list>
#include <limits>
#include <string>


class GenJetMETReader;
class GenParticleReader;


/**
 * \class MPIMatchFilter
 * \brief Selects events in which leading generator-level jet is matched to a parton from hard
 * interaction
 * 
 * This matching eliminates the double counting with MPI, effectively ensuring that the parton
 * interaction described by the matrix element has the highest energy scale compared to MPI [1].
 * This is similar to what is done by filter GenMatchFilter for the double counting with pileup.
 * The matching is performed based on the maximal dR distance.
 * 
 * This plugin relies on the presence of a GenParticleReader with a default name "GenParticles",
 * which reads particles from the hard interaction, and a GenJetMETReader with a default name
 * "GenJetMET".
 * 
 * [1] https://indico.cern.ch/event/747331/#6-l3res-independent-event-sele
 */
class MPIMatchFilter: public AnalysisPlugin
{
public:
    /// Creates a new instance with the given matching parameter
    MPIMatchFilter(std::string const &name, double maxDR);
    
    /// A short-cut for the above version with a default name "MPIMatchFilter"
    MPIMatchFilter(double maxDR);
    
public:
    /**
     * \brief Saves pointer to the reader plugins
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
     * \brief Performs selection based on matching for the leading jet
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of the plugin that reads particles from the hard interaction
    std::string genParticlePluginName;
    
    /// Non-owning pointer to the plugin that reads particles from the hard interaction
    GenParticleReader const *genParticlePlugin;
    
    /// Name of a plugin that produces generator-level jets
    std::string genJetPluginName;
    
    /// Non-owning pointer to the plugin that produces generator-level jets
    GenJetMETReader const *genJetPlugin;
    
    /// Maximal squared dR for matching
    double maxDR2;
};
