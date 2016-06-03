#pragma once

#include <mensura/extensions/EventWeightPlugin.hpp>

#include <TFile.h>
#include <TH1.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>


class PileUpReader;
class TriggerBin;


/**
 * \class DynamicPileUpWeight
 * \brief Plugin that implements reweighting for additional pp interactions ("pile-up")
 * 
 * Extends PileUpWeight plugin from mensura framework [1], allowing to choose the target pile-up
 * profile independently in each event according to the trigger bin reported by a TriggerBin
 * plugin. Consult [1] for documentation on details of pile-up reweighting.
 * 
 * This plugin exploits a PileUpReader with a default name "PileUp" and a TriggerBin plugin with a
 * default name "TriggerBin".
 * 
 * [1] https://github.com/andrey-popov/mensura/blob/84648f604ed4e45b48376ff6bc41ec38a3a7f934/include/mensura/extensions/PileUpWeight.hpp
 */
class DynamicPileUpWeight: public EventWeightPlugin
{
public:
    /**
     * \brief Creates a plugin with the given name
     * 
     * Input arguments specify names of files containing distributions of "true" number of pile-up
     * interactions in data and simulation. By default, provided file names are resolved with
     * respect to directory data/PileUp/. The data files must contain histograms named "pileup"
     * that describe the distributions. The file with profiles in simulation may contain individual
     * distributions for some or all processes (named according to labels returned by method
     * Dataset::GetSourceDatasetID), and, in addition to them, it must include the default
     * distribution named "nominal". Input histograms with arbitrary normalization and binning
     * (including variable one) are supported.
     * 
     * The last argument sets desired systematical variation as defined in [1].
     * [1] https://twiki.cern.ch/twiki/bin/view/CMS/PileupSystematicErrors
     */
    DynamicPileUpWeight(std::string const &name,
      std::initializer_list<std::string> const &dataPUFileNames, std::string const &mcPUFileName,
      double systError);
    
    /// A short-cut for the above version with a default name "PileUpWeight"
    DynamicPileUpWeight(std::initializer_list<std::string> const &dataPUFileNames,
      std::string const &mcPUFileName, double systError);
    
public:
    /**
     * \brief Saves pointers to required plugins and loads simulated pile-up profile for the
     * current dataset
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &dataset) override;
    
    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual Plugin *Clone() const override;
    
private:
    /**
     * \brief Calculates weights for the current event
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of a plugin that reads information about pile-up
    std::string puPluginName;
    
    /// Non-owning pointer to a plugin that reads information about pile-up
    PileUpReader const *puPlugin;
    
    /// Name of a plugin that determines trigger bin
    std::string triggerBinPluginName;
    
    /// Non-owning pointer to a plugin that determines trigger bin
    TriggerBin const *triggerBinPlugin;
    
    /**
     * \brief Target pile-up distributions in data
     * 
     * Indices of this vector correspond to values returned by TriggerBin::GetTriggerBin(),
     * minus 1.
     */
    std::vector<std::shared_ptr<TH1>> dataPUHists;
    
    /// File with per-dataset distributions of expected pile-up in simulation
    std::shared_ptr<TFile> mcPUFile;
    
    /// Distribution of expected pile-up used in generation of the current MC dataset
    std::shared_ptr<TH1> mcPUHist;
    
    /// Rescaling of the target distribution to estimate systematical uncertainty
    double const systError;
};
